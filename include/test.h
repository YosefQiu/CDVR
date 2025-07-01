// TransferFunctionTest.h
#pragma once
#include "ggl.h"
#include "PipelineManager.h"

struct SparsePoint 
{
    float x;
    float y;
    float value;
    float padding;  // 填充到16字节对齐
};

// GPU端的KD-Tree节点结构（16字节对齐）
struct GPUKDNode {
    float splitValue;      // 分割值
    uint32_t splitDim;     // 分割维度 (0=x, 1=y, 2=z for 3D)
    uint32_t leftChild;    // 左子节点索引 (0xFFFFFFFF表示无效)
    uint32_t rightChild;   // 右子节点索引 (0xFFFFFFFF表示无效)
    
    // 叶节点数据
    uint32_t pointStart;   // 叶节点点数据起始索引
    uint32_t pointCount;   // 叶节点包含的点数量
    float boundingBox[4];  // [minX, minY, maxX, maxY] 用于剪枝
};

// GPU端的点数据（简化版，只保留必要信息）
struct GPUPoint {
    float x, y;
    float value;
    float padding;  // 保持16字节对齐
};

class KDTreeBuilder {
private:
    std::vector<SparsePoint> points;
    std::vector<GPUKDNode> nodes;
    std::vector<GPUPoint> leafPoints;
    
    const size_t MAX_DEPTH = 6;        // 最大深度6层
    const size_t MIN_POINTS_PER_LEAF = 64;   // 叶节点最少64个点
    const size_t MAX_POINTS_PER_LEAF = 512;  // 叶节点最多512个点

public:
    struct KDTreeData {
        std::vector<GPUKDNode> nodes;
        std::vector<GPUPoint> points;
        uint32_t rootIndex = 0;
        uint32_t totalNodes = 0;
        uint32_t totalPoints = 0;
    };

    KDTreeData buildKDTree(const std::vector<SparsePoint>& inputPoints) {
        points = inputPoints;
        nodes.clear();
        leafPoints.clear();
        
        // 添加数据验证
        if (points.empty()) {
            std::cerr << "[KDTree] Warning: No input points provided" << std::endl;
            return KDTreeData{};
        }
        
        if (points.size() > 1000000) {  // 1M点限制
            std::cerr << "[KDTree] Warning: Too many points (" << points.size() << "), may cause performance issues" << std::endl;
        }
        
        // 验证点数据的有效性
        for (size_t i = 0; i < std::min(points.size(), size_t(10)); ++i) {
            const auto& p = points[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.value)) {
                std::cerr << "[KDTree] Error: Invalid point data at index " << i 
                         << " (x=" << p.x << ", y=" << p.y << ", value=" << p.value << ")" << std::endl;
                return KDTreeData{};
            }
        }
        
        std::cout << "[KDTree] Building tree for " << points.size() << " points..." << std::endl;
        
        // 预分配内存以避免频繁重分配
        nodes.reserve(points.size() / 10);  // 估算节点数量
        leafPoints.reserve(points.size());
        
        // 创建点索引数组
        std::vector<uint32_t> indices(points.size());
        std::iota(indices.begin(), indices.end(), 0);
        
        // 递归构建树
        uint32_t rootIdx = buildNode(indices, 0);
        
        std::cout << "[KDTree] Tree built successfully:" << std::endl;
        std::cout << "[KDTree]   Total nodes: " << nodes.size() << std::endl;
        std::cout << "[KDTree]   Leaf points: " << leafPoints.size() << std::endl;
        
        // 准备GPU数据
        KDTreeData result;
        result.nodes = std::move(nodes);
        result.points = std::move(leafPoints);
        result.rootIndex = rootIdx;
        result.totalNodes = result.nodes.size();
        result.totalPoints = result.points.size();
        
        return result;
    }

private:
    uint32_t buildNode(std::vector<uint32_t>& indices, size_t depth) {
        if (indices.empty()) return 0xFFFFFFFF;
        
        // 创建新节点
        uint32_t nodeIdx = nodes.size();
        nodes.emplace_back();
        GPUKDNode& node = nodes[nodeIdx];
        
        // 计算边界框
        float minX = FLT_MAX, minY = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX;
        for (uint32_t idx : indices) {
            const auto& p = points[idx];
            minX = std::min(minX, p.x);
            minY = std::min(minY, p.y);
            maxX = std::max(maxX, p.x);
            maxY = std::max(maxY, p.y);
        }
        node.boundingBox[0] = minX;
        node.boundingBox[1] = minY;
        node.boundingBox[2] = maxX;
        node.boundingBox[3] = maxY;
        
        // 决定是否创建叶节点
        if (depth >= MAX_DEPTH || 
            indices.size() <= MIN_POINTS_PER_LEAF ||
            indices.size() <= MAX_POINTS_PER_LEAF) {
            
            // 创建叶节点
            node.splitDim = 0xFFFFFFFF;  // 标记为叶节点
            node.splitValue = 0.0f;
            node.leftChild = 0xFFFFFFFF;
            node.rightChild = 0xFFFFFFFF;
            node.pointStart = leafPoints.size();
            node.pointCount = indices.size();
            
            // 将点数据复制到叶节点数组
            for (uint32_t idx : indices) {
                const auto& p = points[idx];
                leafPoints.push_back({p.x, p.y, p.value, 0.0f});
            }
            
            return nodeIdx;
        }
        
        // 选择分割维度（交替使用x和y）
        uint32_t splitDim = depth % 2;  // 0=x, 1=y
        
        // 按选定维度排序
        std::sort(indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
            if (splitDim == 0) {
                return points[a].x < points[b].x;
            } else {
                return points[a].y < points[b].y;
            }
        });
        
        // 找中位数分割点
        size_t medianIdx = indices.size() / 2;
        float splitValue;
        if (splitDim == 0) {
            splitValue = points[indices[medianIdx]].x;
        } else {
            splitValue = points[indices[medianIdx]].y;
        }
        
        // 设置内部节点信息
        node.splitDim = splitDim;
        node.splitValue = splitValue;
        node.pointStart = 0;
        node.pointCount = 0;
        
        // 分割点集
        std::vector<uint32_t> leftIndices(indices.begin(), indices.begin() + medianIdx);
        std::vector<uint32_t> rightIndices(indices.begin() + medianIdx, indices.end());
        
        // 递归构建子树
        node.leftChild = buildNode(leftIndices, depth + 1);
        node.rightChild = buildNode(rightIndices, depth + 1);
        
        return nodeIdx;
    }
};

class KDTreeVerifier {
public:
    struct VerificationResult {
        bool isValid = true;
        std::string errorMessage;
        
        // 性能统计
        size_t totalNodesVisited = 0;
        size_t leafNodesVisited = 0;
        double averageSearchTime = 0.0;
        
        // 准确性统计
        size_t totalQueries = 0;
        size_t foundQueries = 0;
        double averageError = 0.0;
    };
    
    // CPU端KNN搜索（用于对比验证）
    struct KNNResult {
        std::vector<size_t> indices;
        std::vector<float> distances;
        std::vector<float> values;
        
        KNNResult(size_t k = 3) {
            indices.reserve(k);
            distances.reserve(k);
            values.reserve(k);
        }
    };
    
    // 暴力搜索KNN（作为ground truth）
    static KNNResult bruteForceKNN(const std::vector<SparsePoint>& points, 
                                  const glm::vec2& queryPos, 
                                  size_t k, 
                                  float searchRadius) {
        KNNResult result(k);
        
        struct PointDistance {
            size_t index;
            float distance;
            float value;
        };
        
        std::vector<PointDistance> candidates;
        
        for (size_t i = 0; i < points.size(); ++i) {
            float dx = points[i].x - queryPos.x;
            float dy = points[i].y - queryPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist <= searchRadius) {
                candidates.push_back({i, dist, points[i].value});
            }
        }
        
        // 按距离排序
        std::sort(candidates.begin(), candidates.end(), 
                 [](const PointDistance& a, const PointDistance& b) {
                     return a.distance < b.distance;
                 });
        
        // 取前k个
        size_t count = std::min(k, candidates.size());
        for (size_t i = 0; i < count; ++i) {
            result.indices.push_back(candidates[i].index);
            result.distances.push_back(candidates[i].distance);
            result.values.push_back(candidates[i].value);
        }
        
        return result;
    }
    
    // CPU端KD-Tree遍历
    static KNNResult kdTreeCPUSearch(const KDTreeBuilder::KDTreeData& kdData,
                                    const std::vector<SparsePoint>& originalPoints,
                                    const glm::vec2& queryPos,
                                    size_t k,
                                    float searchRadius,
                                    VerificationResult& stats) {
        KNNResult result(k);
        stats.totalNodesVisited = 0;
        stats.leafNodesVisited = 0;
        
        if (kdData.nodes.empty()) {
            return result;
        }
        
        // 使用栈进行迭代遍历（模拟GPU版本）
        std::vector<uint32_t> stack;
        stack.reserve(16);
        stack.push_back(kdData.rootIndex);
        
        while (!stack.empty()) {
            uint32_t nodeIdx = stack.back();
            stack.pop_back();
            stats.totalNodesVisited++;
            
            if (nodeIdx == 0xFFFFFFFF || nodeIdx >= kdData.nodes.size()) {
                continue;
            }
            
            const auto& node = kdData.nodes[nodeIdx];
            
            // 边界框剪枝检查
            float minX = node.boundingBox[0];
            float minY = node.boundingBox[1];
            float maxX = node.boundingBox[2];
            float maxY = node.boundingBox[3];
            
            float dx = std::max(0.0f, std::max(minX - queryPos.x, queryPos.x - maxX));
            float dy = std::max(0.0f, std::max(minY - queryPos.y, queryPos.y - maxY));
            float distToBBox = std::sqrt(dx * dx + dy * dy);
            
            if (distToBBox > searchRadius) {
                continue;  // 剪枝
            }
            
            // 检查是否为叶节点
            if (node.splitDim == 0xFFFFFFFF) {
                // 叶节点：搜索所有点
                stats.leafNodesVisited++;
                
                for (uint32_t i = 0; i < node.pointCount; ++i) {
                    uint32_t pointIdx = node.pointStart + i;
                    if (pointIdx >= kdData.points.size()) {
                        break;
                    }
                    
                    const auto& point = kdData.points[pointIdx];
                    float dx = point.x - queryPos.x;
                    float dy = point.y - queryPos.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    
                    if (dist <= searchRadius) {
                        // 插入到结果中（保持排序）
                        auto insertPos = std::upper_bound(result.distances.begin(), 
                                                         result.distances.end(), dist);
                        size_t pos = insertPos - result.distances.begin();
                        
                        if (pos < k) {
                            result.distances.insert(insertPos, dist);
                            result.values.insert(result.values.begin() + pos, point.value);
                            result.indices.insert(result.indices.begin() + pos, pointIdx);
                            
                            // 保持最多k个元素
                            if (result.distances.size() > k) {
                                result.distances.pop_back();
                                result.values.pop_back();
                                result.indices.pop_back();
                            }
                        }
                    }
                }
            } else {
                // 内部节点：决定搜索顺序
                float splitPos = (node.splitDim == 0) ? queryPos.x : queryPos.y;
                bool isLeftSide = splitPos < node.splitValue;
                float distToSplit = std::abs(splitPos - node.splitValue);
                
                if (isLeftSide) {
                    // 优先搜索左子树
                    if (node.leftChild != 0xFFFFFFFF) {
                        stack.push_back(node.leftChild);
                    }
                    // 如果可能包含更近的点，也搜索右子树
                    if (distToSplit <= searchRadius && node.rightChild != 0xFFFFFFFF) {
                        stack.push_back(node.rightChild);
                    }
                } else {
                    // 优先搜索右子树
                    if (node.rightChild != 0xFFFFFFFF) {
                        stack.push_back(node.rightChild);
                    }
                    // 如果可能包含更近的点，也搜索左子树
                    if (distToSplit <= searchRadius && node.leftChild != 0xFFFFFFFF) {
                        stack.push_back(node.leftChild);
                    }
                }
            }
        }
        
        return result;
    }
    
    // 验证KD-Tree的正确性
    static VerificationResult verifyKDTree(const KDTreeBuilder::KDTreeData& kdData,
                                          const std::vector<SparsePoint>& originalPoints) {
        VerificationResult result;
        
        std::cout << "[KDTreeVerifier] Starting verification..." << std::endl;
        std::cout << "  Original points: " << originalPoints.size() << std::endl;
        std::cout << "  KD-Tree nodes: " << kdData.totalNodes << std::endl;
        std::cout << "  KD-Tree leaf points: " << kdData.totalPoints << std::endl;
        
        // 1. 验证点数量一致性
        if (kdData.totalPoints != originalPoints.size()) {
            result.isValid = false;
            result.errorMessage = "Point count mismatch: original=" + 
                                std::to_string(originalPoints.size()) + 
                                ", kdtree=" + std::to_string(kdData.totalPoints);
            return result;
        }
        
        // 2. 验证树结构
        if (!validateTreeStructure(kdData, result)) {
            return result;
        }
        
        // 3. 性能和准确性测试
        const size_t numTestQueries = 100;
        const float searchRadius = 5.0f;
        const size_t k = 3;
        
        std::vector<glm::vec2> testQueries;
        generateTestQueries(originalPoints, testQueries, numTestQueries);
        
        double totalSearchTime = 0.0;
        double totalError = 0.0;
        size_t foundQueries = 0;
        size_t totalNodesVisited = 0;
        size_t totalLeafNodesVisited = 0;
        
        for (const auto& queryPos : testQueries) {
            // KD-Tree搜索
            VerificationResult queryStats;
            auto startTime = std::chrono::high_resolution_clock::now();
            auto kdResult = kdTreeCPUSearch(kdData, originalPoints, queryPos, k, searchRadius, queryStats);
            auto endTime = std::chrono::high_resolution_clock::now();
            
            double searchTime = std::chrono::duration<double, std::micro>(endTime - startTime).count();
            totalSearchTime += searchTime;
            totalNodesVisited += queryStats.totalNodesVisited;
            totalLeafNodesVisited += queryStats.leafNodesVisited;
            
            // 暴力搜索（ground truth）
            auto bruteResult = bruteForceKNN(originalPoints, queryPos, k, searchRadius);
            
            if (!kdResult.distances.empty()) {
                foundQueries++;
                
                // 比较结果
                double error = compareKNNResults(kdResult, bruteResult);
                totalError += error;
            }
        }
        
        // 统计结果
        result.totalQueries = numTestQueries;
        result.foundQueries = foundQueries;
        result.averageSearchTime = totalSearchTime / numTestQueries;
        result.averageError = (foundQueries > 0) ? totalError / foundQueries : 0.0;
        result.totalNodesVisited = totalNodesVisited;
        result.leafNodesVisited = totalLeafNodesVisited;
        
        // 输出验证结果
        std::cout << "[KDTreeVerifier] Verification completed:" << std::endl;
        std::cout << "  Structure valid: " << (result.isValid ? "YES" : "NO") << std::endl;
        std::cout << "  Test queries: " << result.totalQueries << std::endl;
        std::cout << "  Successful queries: " << result.foundQueries << std::endl;
        std::cout << "  Average search time: " << result.averageSearchTime << " μs" << std::endl;
        std::cout << "  Average error: " << result.averageError << std::endl;
        std::cout << "  Avg nodes visited per query: " << (double)totalNodesVisited / numTestQueries << std::endl;
        std::cout << "  Avg leaf nodes visited per query: " << (double)totalLeafNodesVisited / numTestQueries << std::endl;
        
        return result;
    }

private:
    static bool validateTreeStructure(const KDTreeBuilder::KDTreeData& kdData, VerificationResult& result) {
        if (kdData.nodes.empty()) {
            result.isValid = false;
            result.errorMessage = "Empty tree";
            return false;
        }
        
        // 验证根节点
        if (kdData.rootIndex >= kdData.nodes.size()) {
            result.isValid = false;
            result.errorMessage = "Invalid root index";
            return false;
        }
        
        // 递归验证每个节点
        std::vector<bool> visited(kdData.nodes.size(), false);
        return validateNode(kdData, kdData.rootIndex, visited, result, 0);
    }
    
    static bool validateNode(const KDTreeBuilder::KDTreeData& kdData, 
                           uint32_t nodeIdx, 
                           std::vector<bool>& visited,
                           VerificationResult& result,
                           size_t depth) {
        if (nodeIdx == 0xFFFFFFFF) return true;
        
        if (nodeIdx >= kdData.nodes.size()) {
            result.isValid = false;
            result.errorMessage = "Node index out of bounds: " + std::to_string(nodeIdx);
            return false;
        }
        
        if (visited[nodeIdx]) {
            result.isValid = false;
            result.errorMessage = "Circular reference detected at node " + std::to_string(nodeIdx);
            return false;
        }
        
        visited[nodeIdx] = true;
        const auto& node = kdData.nodes[nodeIdx];
        
        // 验证叶节点
        if (node.splitDim == 0xFFFFFFFF) {
            if (node.pointStart + node.pointCount > kdData.points.size()) {
                result.isValid = false;
                result.errorMessage = "Leaf node point range out of bounds";
                return false;
            }
            return true;
        }
        
        // 验证内部节点
        if (node.splitDim > 1) {  // 2D只有0(x)和1(y)
            result.isValid = false;
            result.errorMessage = "Invalid split dimension: " + std::to_string(node.splitDim);
            return false;
        }
        
        // 递归验证子节点
        return validateNode(kdData, node.leftChild, visited, result, depth + 1) &&
               validateNode(kdData, node.rightChild, visited, result, depth + 1);
    }
    
    static void generateTestQueries(const std::vector<SparsePoint>& points, 
                                  std::vector<glm::vec2>& queries, 
                                  size_t numQueries) {
        if (points.empty()) return;
        
        // 计算数据范围
        float minX = points[0].x, maxX = points[0].x;
        float minY = points[0].y, maxY = points[0].y;
        
        for (const auto& p : points) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        
        // 生成测试查询点
        queries.reserve(numQueries);
        srand(12345);  // 固定种子确保可重复
        
        for (size_t i = 0; i < numQueries; ++i) {
            float x = minX + (maxX - minX) * ((float)rand() / RAND_MAX);
            float y = minY + (maxY - minY) * ((float)rand() / RAND_MAX);
            queries.emplace_back(x, y);
        }
    }
    
    static double compareKNNResults(const KNNResult& kdResult, const KNNResult& bruteResult) {
        if (kdResult.distances.size() != bruteResult.distances.size()) {
            return 1.0;  // 完全不匹配
        }
        
        double totalError = 0.0;
        for (size_t i = 0; i < kdResult.distances.size(); ++i) {
            double error = std::abs(kdResult.distances[i] - bruteResult.distances[i]);
            totalError += error;
        }
        
        return totalError / kdResult.distances.size();
    }
};

class TransferFunctionTest 
{

public:
    TransferFunctionTest(wgpu::Device device, wgpu::Queue queue, wgpu::TextureFormat swapChainFormat);
    ~TransferFunctionTest();

    
    struct RS_Uniforms 
    {
        alignas(16) glm::mat4 viewMatrix = glm::mat4(1.0f);
        alignas(16) glm::mat4 projMatrix = glm::mat4(1.0f);
    };

    struct CS_Uniforms 
    {
        float minValue;
        float maxValue;
        float gridWidth;
        float gridHeight;
        uint32_t numPoints;
        float searchRadius;
        float padding[2];
    };

    struct DataHeader {
        uint32_t width;
        uint32_t height;
        uint32_t numPoints;
    };

    struct ComputeStage
    {
        wgpu::ComputePipeline pipeline = nullptr;
        wgpu::BindGroup data_bindGroup = nullptr;
        wgpu::BindGroup TF_bindGroup = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        wgpu::Buffer storageBuffer = nullptr;

        bool Init(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints, const CS_Uniforms uniforms);
        bool CreatePipeline(wgpu::Device device);
        bool UpdateBindGroup(wgpu::Device device, wgpu::TextureView inputTF, wgpu::TextureView outputTexture);
        void RunCompute(wgpu::Device device, wgpu::Queue queue);
        void Release();
    private:
        bool InitSSBO(wgpu::Device device, wgpu::Queue queue, const std::vector<SparsePoint>& sparsePoints);
        bool InitUBO(wgpu::Device device, CS_Uniforms uniforms);
    };

    struct RenderStage
    {
        wgpu::RenderPipeline pipeline = nullptr;
        wgpu::BindGroup bindGroup = nullptr;
        wgpu::Sampler sampler = nullptr;
        wgpu::Buffer vertexBuffer = nullptr;
        wgpu::Buffer uniformBuffer = nullptr;
        
        bool Init(wgpu::Device device, wgpu::Queue queue, RS_Uniforms uniforms, float data_width, float data_height);
        bool CreatePipeline(wgpu::Device device, wgpu::TextureFormat swapChainFormat);
        bool InitBindGroup(wgpu::Device device, wgpu::TextureView outputTexture);
        void Render(wgpu::RenderPassEncoder renderPass);
        void Release();
        void UpdateUniforms(wgpu::Queue queue, RS_Uniforms uniforms);
    private:
        bool InitVBO(wgpu::Device device, wgpu::Queue queue, float data_width, float data_height);
        bool InitUBO(wgpu::Device devic, RS_Uniforms uniforms);
        bool InitSampler(wgpu::Device device);
        bool InitResources(wgpu::Device device);
        
    };
    

    bool Initialize(glm::mat4 vMat, glm::mat4 pMat);
    bool InitOutputTexture(uint32_t width = 512, uint32_t height = 512, uint32_t depth = 1, wgpu::TextureFormat format = wgpu::TextureFormat::RGBA16Float);
    bool InitDataFromBinary(const std::string& filename);
    void Render(wgpu::RenderPassEncoder renderPass);
    void OnWindowResize(glm::mat4 veiwMatrix, glm::mat4 projMatrix);
    void UpdateSSBO(wgpu::TextureView tfTextureView);
    void ComputeValueRange();
    void UpdateUniforms(glm::mat4 viewMatrix, glm::mat4 projMatrix);
protected:
    std::vector<SparsePoint> m_sparsePoints;
    DataHeader m_header;
    RS_Uniforms m_RS_Uniforms;
    CS_Uniforms m_CS_Uniforms;
private:
    wgpu::Device m_device;
    wgpu::Queue m_queue;
    wgpu::TextureFormat m_swapChainFormat;
    wgpu::TextureView m_tfTextureView = nullptr;
    wgpu::Texture m_outputTexture;                     
    wgpu::TextureView m_outputTextureView;
    ComputeStage m_computeStage;
    RenderStage m_renderStage;
    bool m_needsUpdate = false;
};
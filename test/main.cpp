#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <limits>
#include <cassert>

struct SparsePoint {
    float x;
    float y;
    float value;
    float padding;
};

struct GPUPoint {
    float x, y;
    float value;
    float padding;
    
    GPUPoint(float px = 0, float py = 0, float val = 0, float pad = 0) 
        : x(px), y(py), value(val), padding(pad) {}
};

class KDTreeBuilder {
private:
    struct BinaryTree {
        static int numLevelsFor(int numNodes) {
            if (numNodes <= 0) return 0;
            return static_cast<int>(std::floor(std::log2(numNodes))) + 1;
        }
        
        static int leftChildOf(int node) { return 2 * node + 1; }
        static int rightChildOf(int node) { return 2 * node + 2; }
        static int parentOf(int node) { return (node - 1) / 2; }
    };
    
    struct FullBinaryTreeOf {
        int level;
        explicit FullBinaryTreeOf(int l) : level(l) {}
        
        int numNodes() const {
            if (level <= 0) return 0;
            return (1 << level) - 1;  // 2^level - 1
        }
    };
    
    struct ArrayLayoutInStep {
        int level;
        int numPoints;
        
        ArrayLayoutInStep(int l, int n) : level(l), numPoints(n) {}
        
        int pivotPosOf(int subtree) const {
            int nodesInLevel = 1 << level;  // 2^level
            
            int baseSubtreeSize = numPoints / nodesInLevel;
            int remainder = numPoints % nodesInLevel;
            
            int subtreeSize = baseSubtreeSize;
            if (subtree < remainder) {
                subtreeSize++;
            }
            
            int subtreeStart = subtree * baseSubtreeSize + std::min(subtree, remainder);
            
            return subtreeStart + subtreeSize / 2;
        }
    };
    
    // 修正：改为存储 (tag, point) 而不是 (tag, index)
    struct ZipData {
        uint32_t tag;
        GPUPoint point;
        
        ZipData(uint32_t t, const GPUPoint& p) : tag(t), point(p) {}
    };
    
    struct ZipCompare {
        int dim;
        
        ZipCompare(int d) : dim(d) {}
        
        // 修正：使用正确的比较逻辑
        bool operator()(const ZipData& a, const ZipData& b) const {
            // 主要按tag排序，tag相同时按坐标排序
            if (a.tag != b.tag) {
                return a.tag < b.tag;
            }
            
            float coord_a = (dim == 0) ? a.point.x : a.point.y;
            float coord_b = (dim == 0) ? b.point.x : b.point.y;
            
            // 这里是关键：相同tag时，严格按坐标排序
            return coord_a < coord_b;
        }
    };
    
    void updateTags(std::vector<uint32_t>& tags, int numPoints, int level) {
        int numSettled = FullBinaryTreeOf(level).numNodes();
        
        for (int gid = numSettled; gid < numPoints; gid++) {
            int subtree = tags[gid];
            int pivotPos = ArrayLayoutInStep(level, numPoints).pivotPosOf(subtree);
            
            if (gid < pivotPos) {
                subtree = BinaryTree::leftChildOf(subtree);
            } else if (gid > pivotPos) {
                subtree = BinaryTree::rightChildOf(subtree);
            }
            
            tags[gid] = subtree;
        }
    }

public:
    struct TreeData {
        std::vector<GPUPoint> points;
        uint32_t totalPoints = 0;
        uint32_t numLevels = 0;
    };
    
    TreeData buildLeftBalancedKDTree(const std::vector<SparsePoint>& inputPoints) {
        TreeData result;
        
        if (inputPoints.empty()) {
            std::cout << "[CompleteBuilder] 错误：输入为空" << std::endl;
            return result;
        }
        
        int numPoints = static_cast<int>(inputPoints.size());
        
        std::cout << "[CompleteBuilder] 开始构建 " << numPoints << " 个点的left-balanced KD tree..." << std::endl;
        
        // 转换输入数据
        std::vector<GPUPoint> points(numPoints);
        for (size_t i = 0; i < inputPoints.size(); i++) {
            points[i] = {inputPoints[i].x, inputPoints[i].y, inputPoints[i].value, 0.0f};
        }
        
        // 创建标签数组，初始化为0（所有点都在根子树中）
        std::vector<uint32_t> tags(numPoints, 0);
        
        // 修正：创建zip数组，直接存储 (tag, point)
        std::vector<ZipData> zipData;
        zipData.reserve(numPoints);
        for (int i = 0; i < numPoints; i++) {
            zipData.push_back(ZipData(tags[i], points[i]));
        }
        
        // 计算树的层数
        int numLevels = BinaryTree::numLevelsFor(numPoints);
        int deepestLevel = numLevels - 1;
        
        std::cout << "[CompleteBuilder] 树将有 " << numLevels 
                  << " 层（最深层: " << deepestLevel << "）" << std::endl;
        
        // 逐层构建树（论文核心算法）
        for (int level = 0; level < deepestLevel; level++) {
            std::cout << "[CompleteBuilder] 处理第 " << level << " 层（维度 " << (level % 2) << "）" << std::endl;
            
            // 按当前维度zip排序
            std::sort(zipData.begin(), zipData.end(), ZipCompare(level % 2));
            
            // 更新tags数组
            for (int i = 0; i < numPoints; i++) {
                tags[i] = zipData[i].tag;
            }
            
            // 更新标签为下一层
            updateTags(tags, numPoints, level);
            
            // 重新组装zip数据
            for (int i = 0; i < numPoints; i++) {
                zipData[i].tag = tags[i];
            }
        }
        
        // 最后一次排序，确保最终顺序
        std::sort(zipData.begin(), zipData.end(), ZipCompare(deepestLevel % 2));
        
        // 提取最终的点数据（按level-order排列）
        std::vector<GPUPoint> finalPoints(numPoints);
        std::vector<uint32_t> finalTags(numPoints);
        
        for (int i = 0; i < numPoints; i++) {
            finalTags[i] = zipData[i].tag;
            finalPoints[i] = zipData[i].point;
        }
        
        std::cout << "[CompleteBuilder] 构建完成！" << std::endl;
        
        // 验证最终结果
        validateLevelOrderProperty(finalPoints, finalTags, numLevels);
        
        result.points = std::move(finalPoints);
        result.totalPoints = numPoints;
        result.numLevels = numLevels;
        
        return result;
    }
    
private:
    void validateLevelOrderProperty(
        const std::vector<GPUPoint>& points,
        const std::vector<uint32_t>& tags,
        int numLevels) {
        
        std::cout << "[CompleteBuilder] 验证level-order属性..." << std::endl;
        
        // 检查标签是否按顺序排列
        for (size_t i = 1; i < tags.size(); i++) {
            if (tags[i] < tags[i-1]) {
                std::cout << "[CompleteBuilder] 警告：标签顺序不正确 at " << i << std::endl;
                return;
            }
        }
        
        // 显示树结构的一些信息
        if (!points.empty()) {
            std::cout << "[CompleteBuilder] 根节点（索引0）: (" 
                      << points[0].x << ", " << points[0].y 
                      << ", value=" << points[0].value << ", tag=" << tags[0] << ")" << std::endl;
        }
        
        if (points.size() > 1) {
            std::cout << "[CompleteBuilder] 左子节点（索引1）: (" 
                      << points[1].x << ", " << points[1].y 
                      << ", tag=" << tags[1] << ")" << std::endl;
        }
        
        if (points.size() > 2) {
            std::cout << "[CompleteBuilder] 右子节点（索引2）: (" 
                      << points[2].x << ", " << points[2].y 
                      << ", tag=" << tags[2] << ")" << std::endl;
        }
        
        std::cout << "[CompleteBuilder] Level-order验证完成" << std::endl;
    }
};


class KNNResult {
private:
    std::vector<float> distances;  // 距离数组
    std::vector<int> indices;      // 索引数组
    int count;                     // 当前数量
    int max_size;                  // 最大容量
    
public:
    KNNResult(uint32_t k) : max_size(static_cast<int>(k)), count(0) {
        distances.resize(k);
        indices.resize(k);
    }
    
    void addPoint(float distance, int pointIndex) {
        if (count < max_size) {
            // 数组未满，直接添加
            distances[count] = distance;
            indices[count] = pointIndex;
            count++;
            
            // 插入排序，保持数组按距离从小到大排序
            for (int i = count - 1; i > 0; i--) {
                if (distances[i] < distances[i-1]) {
                    std::swap(distances[i], distances[i-1]);
                    std::swap(indices[i], indices[i-1]);
                } else {
                    break;
                }
            }
        } else if (distance < distances[max_size-1]) {
            // 新点比最远的点更近，替换最远的点
            distances[max_size-1] = distance;
            indices[max_size-1] = pointIndex;
            
            // 向前移动到正确位置
            for (int i = max_size - 1; i > 0; i--) {
                if (distances[i] < distances[i-1]) {
                    std::swap(distances[i], distances[i-1]);
                    std::swap(indices[i], indices[i-1]);
                } else {
                    break;
                }
            }
        }
    }
    
    float getFarthestDistance() const {
        if (count == 0) {
            return std::numeric_limits<float>::max();
        }
        return distances[count-1];  // 最后一个元素是最远的
    }
    
    bool isFull() const {
        return count >= max_size;
    }
    
    size_t size() const {
        return static_cast<size_t>(count);
    }
    
    std::vector<std::pair<float, int>> getResults() const {
        std::vector<std::pair<float, int>> results;
        results.reserve(count);
        for (int i = 0; i < count; i++) {
            results.push_back({distances[i], indices[i]});
        }
        return results;
    }
    
    // 为了方便WGSL移植，提供直接数组访问
    const std::vector<float>& getDistances() const { return distances; }
    const std::vector<int>& getIndices() const { return indices; }
    int getCount() const { return count; }
    int getMaxSize() const { return max_size; }
};

static KNNResult BruteForceKNN(
    const std::vector<GPUPoint>& nodes,
    float queryX, float queryY,
    uint32_t k) {
    
    KNNResult result(k);
    // 遍历所有点
    for (size_t i = 0; i < nodes.size(); i++) {
        // 计算距离的平方（避免开方运算）
        float dx = nodes[i].x - queryX;
        float dy = nodes[i].y - queryY;
        float distanceSquared = dx * dx + dy * dy;
        
        // 添加到结果中
        result.addPoint(distanceSquared, static_cast<int>(i));
    }
    
    return result;
}


static KNNResult TraverseKDTree(
    const std::vector<GPUPoint>& nodes,
    float queryX, float queryY,
    uint32_t k,
    float max_SearchRadius = std::numeric_limits<float>::max()) {
    
    // 创建查询点数组（严格按照您的原始代码）
    float queryPoint[2] = {queryX, queryY};
    
    // current node: start at root
    int N = static_cast<int>(nodes.size());
    KNNResult result(k);
    int curr = 0;
    // previous node, initialize to 'parent' of root node
    int prev = -1;
    float maxSearchRadius = max_SearchRadius;
    
    // 处理节点的函数（需要您来实现具体逻辑）
    auto processNode = [&](int nodeIndex) {
        if (nodeIndex >= 0 && nodeIndex < N) {
            float dx = nodes[nodeIndex].x - queryX;
            float dy = nodes[nodeIndex].y - queryY;
            float distSq = dx * dx + dy * dy;
            
            if (distSq <= maxSearchRadius * maxSearchRadius) {
                result.addPoint(distSq, nodeIndex);
            }
        }
    };
    
    // 获取可能缩小的搜索半径
    auto possiblyShrunkenSearchRadius = [&]() -> float {
        if (result.isFull()) {
            return std::sqrt(result.getFarthestDistance());
        }
        return maxSearchRadius;
    };
    
    // 分割维度函数（需要您来实现具体逻辑）
    auto splitDimOf = [&](const std::vector<GPUPoint>& nodes, int nodeIndex) -> int {
        // 根据节点深度计算分割维度
        int depth = 0;
        int temp = nodeIndex + 1;
        while (temp > 1) {
            temp /= 2;
            depth++;
        }
        return depth % 2;
    };
    
    // 获取节点指定维度的坐标
    auto getCoordinate = [&](const GPUPoint& point, int dim) -> float {
        return (dim == 0) ? point.x : point.y;
    };
    
    // repeat until we break out
    while (true) {
        int parent = (curr + 1) / 2 - 1;  // 严格按照您的公式
        
        if (curr >= N) {
            // we reached a child that does not exist; go back to parent
            prev = curr;
            curr = parent;
            continue;
        }
        
        const int child = 2 * curr + 1;
        bool from_parent = (prev < child);  // 严格按照您的判断逻辑
        
        if (from_parent) {
            processNode(curr);
            // check if processing current node has led to
            // a smaller search radius:
            // maxSearchRadius = possiblyShrunkenSearchRadius();
        }
        
        // compute close and far child:
        int splitDim = splitDimOf(nodes, curr);
        float splitPos = getCoordinate(nodes[curr], splitDim);  // 直接使用x,y而不是coords数组
        float signedDist = queryPoint[splitDim] - splitPos;
        int closeSide = (signedDist > 0.f);  // 严格按照您的逻辑
        int closeChild = 2 * curr + 1 + closeSide;
        int farChild = 2 * curr + 2 - closeSide;
        
        bool farInRange = (fabsf(signedDist) <= maxSearchRadius);  // 使用fabsf
        
        // compute next node to step to
        int next;
        if (from_parent)
            next = closeChild;
        else if (prev == closeChild)
            next = (farInRange ? farChild : parent);
        else {
            next = parent;
        }
        
        if (next == -1) {
            // the only way this can happen is if the entire tree under
            // node number 0 (i.e., the entire tree) is done traversing,
            // and the root node tries to step to its parent ... in
            // which case we have traversed the entire tree and are done.
            break;  // 修正：这里应该是break而不是return
        }
        
        // aaaand ... do the step
        prev = curr;
        curr = next;
    }
    
    return result;
}



// 生成测试数据的函数
std::vector<GPUPoint> generateTestData(int numPoints = 100) {
    std::vector<GPUPoint> nodes;
    nodes.reserve(numPoints);
    
    // 设置随机数种子
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    for (int i = 0; i < numPoints; i++) {
        // 在 150x450 范围内随机生成点
        float x = static_cast<float>(std::rand()) / RAND_MAX * 150.0f;  // 0-150
        float y = static_cast<float>(std::rand()) / RAND_MAX * 450.0f;  // 0-450
        float value = static_cast<float>(i + 1);  // 简单递增值
        
        nodes.push_back(GPUPoint(x, y, value));
    }
    
    return nodes;
}

// 生成规则网格数据的函数
std::vector<GPUPoint> generateGridData(int gridX = 15, int gridY = 30) {
    std::vector<GPUPoint> nodes;
    nodes.reserve(gridX * gridY);
    
    float stepX = 150.0f / (gridX - 1);  // X方向步长
    float stepY = 450.0f / (gridY - 1);  // Y方向步长
    
    int valueCounter = 1;
    for (int i = 0; i < gridX; i++) {
        for (int j = 0; j < gridY; j++) {
            float x = i * stepX;
            float y = j * stepY;
            nodes.push_back(GPUPoint(x, y, static_cast<float>(valueCounter++)));
        }
    }
    
    return nodes;
}

// 生成聚类数据的函数（模拟真实数据分布）
std::vector<GPUPoint> generateClusteredData(int numClusters = 5, int pointsPerCluster = 20) {
    std::vector<GPUPoint> nodes;
    nodes.reserve(numClusters * pointsPerCluster);
    
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    int valueCounter = 1;
    for (int cluster = 0; cluster < numClusters; cluster++) {
        // 随机生成聚类中心
        float centerX = static_cast<float>(std::rand()) / RAND_MAX * 150.0f;
        float centerY = static_cast<float>(std::rand()) / RAND_MAX * 450.0f;
        
        // 在聚类中心周围生成点
        for (int i = 0; i < pointsPerCluster; i++) {
            // 高斯分布近似（使用Box-Muller变换的简化版）
            float radius = static_cast<float>(std::rand()) / RAND_MAX * 20.0f;  // 聚类半径
            float angle = static_cast<float>(std::rand()) / RAND_MAX * 2.0f * 3.14159f;
            
            float x = centerX + radius * std::cos(angle);
            float y = centerY + radius * std::sin(angle);
            
            // 确保点在有效范围内
            x = std::max(0.0f, std::min(150.0f, x));
            y = std::max(0.0f, std::min(450.0f, y));
            
            nodes.push_back(GPUPoint(x, y, static_cast<float>(valueCounter++)));
        }
    }
    
    return nodes;
}

std::vector<std::pair<float, float>> generateRandomQueryPoints(int numQueries = 5) {
    std::vector<std::pair<float, float>> queryPoints;
    queryPoints.reserve(numQueries);
    
    // 使用当前时间作为种子
    std::srand(static_cast<unsigned int>(std::time(nullptr)) + 12345); // 稍微偏移避免与数据生成冲突
    
    for (int i = 0; i < numQueries; i++) {
        // 在 150x450 范围内随机生成查询点
        float x = static_cast<float>(std::rand()) / RAND_MAX * 150.0f;  // 0-150
        float y = static_cast<float>(std::rand()) / RAND_MAX * 450.0f;  // 0-450
        queryPoints.push_back({x, y});
    }
    
    return queryPoints;
}

void TEST1()
{
    // 创建测试数据
    std::vector<GPUPoint> nodes = {
        GPUPoint(5.0f, 5.0f, 1.0f),   // 根节点
        GPUPoint(2.0f, 3.0f, 2.0f),   // 左子节点
        GPUPoint(8.0f, 7.0f, 3.0f),   // 右子节点
        GPUPoint(1.0f, 1.0f, 4.0f),   // 左左子节点
        GPUPoint(3.0f, 4.0f, 5.0f),   // 左右子节点
        GPUPoint(7.0f, 6.0f, 6.0f),   // 右左子节点
        GPUPoint(9.0f, 9.0f, 7.0f)    // 右右子节点
    };

    std::vector<SparsePoint> sparseNodes;
    {
        for (const auto& node : nodes) {
            sparseNodes.push_back({node.x, node.y, node.value, 0.0f});
        }
    }
     
    // 构建 KDTree
    KDTreeBuilder builder;
    auto treeData = builder.buildLeftBalancedKDTree(sparseNodes).points;

    // 查询点
    float queryX = 1.0f, queryY = 1.0f;
    uint32_t k = 3;
    
    std::cout << "查询点: (" << queryX << ", " << queryY << ")" << std::endl;
    std::cout << "k = " << k << std::endl;
    std::cout << "节点数: " << nodes.size() << std::endl;

    // 执行搜索
    KNNResult result = TraverseKDTree(treeData, queryX, queryY, k);
    KNNResult bruteForceResult = BruteForceKNN(nodes, queryX, queryY, k);
    
    // 输出结果1
    std::cout << "\nKD树搜索结果:" << std::endl;
    auto results = result.getResults();
    std::cout << "\n找到的 " << results.size() << " 个最近邻:" << std::endl;
    
    for (size_t i = 0; i < results.size(); i++) {
        int pointIndex = results[i].second;
        float distance = std::sqrt(results[i].first);
        const GPUPoint& point = treeData[pointIndex];
        
        std::cout << "第 " << (i+1) << " 近邻: 索引=" << pointIndex 
                  << ", 坐标=(" << point.x << ", " << point.y << ")"
                  << ", 距离=" << distance << std::endl;
    }

    // 输出结果2
    std::cout << "\n暴力搜索结果:" << std::endl;
    auto bruteForceResults = bruteForceResult.getResults();
    std::cout << "\n找到的 " << bruteForceResults.size() << " 个最近邻:" << std::endl;      
    for (size_t i = 0; i < bruteForceResults.size(); i++) {
        int pointIndex = bruteForceResults[i].second;
        float distance = std::sqrt(bruteForceResults[i].first);
        const GPUPoint& point = nodes[pointIndex];
        
        std::cout << "第 " << (i+1) << " 近邻: 索引=" << pointIndex 
                  << ", 坐标=(" << point.x << ", " << point.y << ")"
                  << ", 距离=" << distance << std::endl;
    }
}

void TEST2()
{
    std::cout << "=== KNN 搜索测试 (150x450 数据范围) ===" << std::endl;
    
    // 生成不同类型的测试数据
    std::cout << "\n1. 随机数据测试:" << std::endl;
    auto randomNodes = generateTestData(2000); 

    std::cout << "\n2. 网格数据测试:" << std::endl;
    auto gridNodes = generateGridData(15, 30);  
    
    std::cout << "\n3. 聚类数据测试:" << std::endl;
    auto clusteredNodes = generateClusteredData(8, 25);  
    
    // 选择测试数据集
    std::vector<GPUPoint> testNodes = randomNodes;  // 可以换成 gridNodes 或 clusteredNodes
    
    std::cout << "\n使用随机数据进行测试，共 " << testNodes.size() << " 个点" << std::endl;
    
    // 显示数据范围
    float minX = 150.0f, maxX = 0.0f, minY = 450.0f, maxY = 0.0f;
    for (const auto& node : testNodes) {
        minX = std::min(minX, node.x);
        maxX = std::max(maxX, node.x);
        minY = std::min(minY, node.y);
        maxY = std::max(maxY, node.y);
    }
    std::cout << "数据范围: X[" << minX << ", " << maxX << "], Y[" << minY << ", " << maxY << "]" << std::endl;
    
    // 构建KD树
    std::cout << "\n========== 构建KD树 ==========" << std::endl;
    KDTreeBuilder builder;
    
    // 将GPUPoint转换为SparsePoint用于构建
    std::vector<SparsePoint> sparsePoints;
    sparsePoints.reserve(testNodes.size());
    for (const auto& node : testNodes) {
        sparsePoints.push_back({node.x, node.y, node.value});
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    auto treeData = builder.buildLeftBalancedKDTree(sparsePoints);
    auto end = std::chrono::high_resolution_clock::now();
    auto buildDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "KD树构建完成，用时: " << buildDuration.count() << " 微秒" << std::endl;
    std::cout << "树结构: " << treeData.numLevels << " 层，" << treeData.totalPoints << " 个节点" << std::endl;
    
    // 多个查询点测试
    std::vector<std::pair<float, float>> queryPoints = generateRandomQueryPoints(25);  

    uint32_t k = 3;
    
    for (size_t i = 0; i < queryPoints.size(); i++) {
        float queryX = queryPoints[i].first;
        float queryY = queryPoints[i].second;
        
        std::cout << "\n========== 查询点 " << (i+1) << ": (" << queryX << ", " << queryY << ") ==========" << std::endl;
        
        // 暴力搜索
        auto start = std::chrono::high_resolution_clock::now();
        KNNResult bruteResult = BruteForceKNN(testNodes, queryX, queryY, k);
        auto end = std::chrono::high_resolution_clock::now();
        auto bruteDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // KD树搜索
        start = std::chrono::high_resolution_clock::now();
        KNNResult kdtreeResult = TraverseKDTree(treeData.points, queryX, queryY, k);
        end = std::chrono::high_resolution_clock::now();
        auto kdtreeDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // std::cout << "暴力搜索用时: " << bruteDuration.count() << " 微秒" << std::endl;
        // std::cout << "KD树搜索用时: " << kdtreeDuration.count() << " 微秒" << std::endl;
        
        // if (kdtreeDuration.count() > 0) {
        //     std::cout << "加速比: " << (double)bruteDuration.count() / kdtreeDuration.count() << "x" << std::endl;
        // }
        
        // 显示暴力搜索结果
        auto bruteResults = bruteResult.getResults();
        std::cout << "\n暴力搜索结果 (" << bruteResults.size() << " 个):" << std::endl;
        for (size_t j = 0; j < bruteResults.size(); j++) {
            int pointIndex = bruteResults[j].second;
            float distance = std::sqrt(bruteResults[j].first);
            const GPUPoint& point = testNodes[pointIndex];
            
            std::cout << "  第 " << (j+1) << " 近邻: 索引=" << pointIndex 
                      << ", 坐标=(" << std::fixed << std::setprecision(2) << point.x << ", " << point.y << ")"
                      << ", 距离=" << distance << std::endl;
        }
        
        // 显示KD树搜索结果
        auto kdtreeResults = kdtreeResult.getResults();
        std::cout << "\nKD树搜索结果 (" << kdtreeResults.size() << " 个):" << std::endl;
        for (size_t j = 0; j < kdtreeResults.size(); j++) {
            int pointIndex = kdtreeResults[j].second;
            float distance = std::sqrt(kdtreeResults[j].first);
            const GPUPoint& point = treeData.points[pointIndex];
            
            std::cout << "  第 " << (j+1) << " 近邻: 索引=" << pointIndex 
                      << ", 坐标=(" << std::fixed << std::setprecision(2) << point.x << ", " << point.y << ")"
                      << ", 距离=" << distance << std::endl;
        }
        
        // 验证结果一致性
        if (bruteResults.size() == kdtreeResults.size()) {
            bool resultsMatch = true;
            for (size_t j = 0; j < bruteResults.size(); j++) {
                if (std::abs(bruteResults[j].first - kdtreeResults[j].first) > 1e-6) {
                    resultsMatch = false;
                    break;
                }
            }
            std::cout << "\n结果验证: " << (resultsMatch ? "✓ 一致" : "✗ 不一致") << std::endl;
        }
    }
    
}

// 测试代码
int main() {
    // TEST1();
    TEST2();
    return 0;
}
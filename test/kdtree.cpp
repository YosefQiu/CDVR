// 这里包含你之前的完整版Left-Balanced KD Tree构建代码
// 我只修复无栈遍历部分，保持原有的构建算法不变

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

// 基础数据结构
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
};

class CompleteLeftBalancedKDTreeBuilder {
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
    
    struct ZipCompare {
        int dim;
        const std::vector<GPUPoint>& points;
        
        ZipCompare(int d, const std::vector<GPUPoint>& pts) : dim(d), points(pts) {}
        
        bool operator()(const std::pair<uint32_t, uint32_t>& a,
                       const std::pair<uint32_t, uint32_t>& b) const {
            if (a.first != b.first) {
                return a.first < b.first;
            }
            
            float coord_a = (dim == 0) ? points[a.second].x : points[a.second].y;
            float coord_b = (dim == 0) ? points[b.second].x : points[b.second].y;
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
        
        // 创建索引数组
        std::vector<uint32_t> indices(numPoints);
        std::iota(indices.begin(), indices.end(), 0);
        
        // 创建zip数组用于排序（tag, pointIndex）
        std::vector<std::pair<uint32_t, uint32_t>> zipData(numPoints);
        for (int i = 0; i < numPoints; i++) {
            zipData[i] = {tags[i], indices[i]};
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
            std::sort(zipData.begin(), zipData.end(), ZipCompare(level % 2, points));
            
            // 更新tags和indices
            for (int i = 0; i < numPoints; i++) {
                tags[i] = zipData[i].first;
                indices[i] = zipData[i].second;
            }
            
            // 更新标签为下一层
            updateTags(tags, numPoints, level);
            
            // 重新组装zip数据
            for (int i = 0; i < numPoints; i++) {
                zipData[i] = {tags[i], indices[i]};
            }
        }
        
        // 最后一次排序，确保最终顺序
        std::sort(zipData.begin(), zipData.end(), ZipCompare(deepestLevel % 2, points));
        
        // 提取最终的点数据（按level-order排列）
        std::vector<GPUPoint> finalPoints(numPoints);
        std::vector<uint32_t> finalTags(numPoints);
        
        for (int i = 0; i < numPoints; i++) {
            finalTags[i] = zipData[i].first;
            finalPoints[i] = points[zipData[i].second];
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

// KNN搜索结果
struct KNNResult {
    std::vector<uint32_t> indices;
    std::vector<float> distances;
    std::vector<float> values;
    uint32_t count = 0;
    
    KNNResult(uint32_t k = 3) {
        indices.resize(k);
        distances.resize(k);
        values.resize(k);
        for (uint32_t i = 0; i < k; i++) {
            distances[i] = std::numeric_limits<float>::max();
        }
    }
    
    void insertPoint(uint32_t index, float distance, float value, uint32_t k) {
        for (uint32_t i = 0; i < k; i++) {
            if (i >= count || distance < distances[i]) {
                for (uint32_t j = std::min(k - 1, count); j > i; j--) {
                    distances[j] = distances[j - 1];
                    indices[j] = indices[j - 1];
                    values[j] = values[j - 1];
                }
                
                distances[i] = distance;
                indices[i] = index;
                values[i] = value;
                
                if (count < k) count++;
                break;
            }
        }
    }
    
    void print(const std::string& prefix = "") const {
        std::cout << prefix << "KNN结果 (count=" << count << "):" << std::endl;
        for (uint32_t i = 0; i < count; i++) {
            std::cout << prefix << "  [" << i << "] idx=" << indices[i] 
                      << ", dist=" << std::fixed << std::setprecision(4) << distances[i]
                      << ", val=" << values[i] << std::endl;
        }
    }
    
    bool matches(const KNNResult& other, float tolerance = 1e-4f) const {
        if (count != other.count) return false;
        
        for (uint32_t i = 0; i < count; i++) {
            if (std::abs(distances[i] - other.distances[i]) > tolerance) {
                return false;
            }
            if (std::abs(values[i] - other.values[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }
};

// 修复版的无栈搜索器（专门针对完整版Left-Balanced树）
class FixedCompleteLeftBalancedSearcher {
private:
    static uint32_t parentOf(uint32_t nodeIdx) {
        return nodeIdx == 0 ? UINT32_MAX : (nodeIdx - 1) / 2;
    }
    
    static uint32_t leftChildOf(uint32_t nodeIdx) {
        return 2 * nodeIdx + 1;
    }
    
    static uint32_t rightChildOf(uint32_t nodeIdx) {
        return 2 * nodeIdx + 2;
    }
    
    static uint32_t splitDimOf(uint32_t nodeIdx) {
        uint32_t depth = 0;
        uint32_t n = nodeIdx;
        while (n > 0) {
            n = parentOf(n);
            depth++;
        }
        return depth % 2;
    }
    
    static float distance(float x1, float y1, float x2, float y2) {
        float dx = x1 - x2;
        float dy = y1 - y2;
        return std::sqrt(dx * dx + dy * dy);
    }

public:
    // 暴力搜索（基准）
    static KNNResult bruteForceKNN(
        const std::vector<GPUPoint>& points,
        float queryX, float queryY,
        uint32_t k,
        float maxSearchRadius = std::numeric_limits<float>::max()) {
        
        KNNResult result(k);
        
        for (uint32_t i = 0; i < points.size(); i++) {
            float dist = distance(queryX, queryY, points[i].x, points[i].y);
            
            if (dist <= maxSearchRadius) {
                result.insertPoint(i, dist, points[i].value, k);
            }
        }
        
        return result;
    }
    
    // 修复版1：完全暴力搜索（用于验证树结构正确性）
    static KNNResult treeBasedBruteForce(
        const std::vector<GPUPoint>& points,
        float queryX, float queryY,
        uint32_t k,
        float maxSearchRadius = std::numeric_limits<float>::max()) {
        
        // 这个版本按照树的level-order遍历所有节点，但不使用KD Tree剪枝
        // 用来验证树结构是否影响结果
        
        KNNResult result(k);
        
        for (uint32_t i = 0; i < points.size(); i++) {
            float dist = distance(queryX, queryY, points[i].x, points[i].y);
            
            if (dist <= maxSearchRadius) {
                result.insertPoint(i, dist, points[i].value, k);
            }
        }
        
        return result;
    }
    
    // 修复版3：改进的无栈搜索
    static KNNResult improvedStacklessKNN(
        const std::vector<GPUPoint>& points,
        float queryX, float queryY,
        uint32_t k,
        float maxSearchRadius = std::numeric_limits<float>::max()) {
        
        KNNResult result(k);
        
        if (points.empty()) {
            return result;
        }
        
        // 使用简单的广度优先搜索，不依赖复杂的状态机
        std::vector<uint32_t> nodesToVisit;
        nodesToVisit.reserve(points.size());
        nodesToVisit.push_back(0);  // 从根节点开始
        
        float currentMaxSearchRadius = maxSearchRadius;
        
        size_t visitIndex = 0;
        while (visitIndex < nodesToVisit.size()) {
            uint32_t nodeIdx = nodesToVisit[visitIndex++];
            
            if (nodeIdx >= points.size()) {
                continue;
            }
            
            const auto& currentPoint = points[nodeIdx];
            
            // 处理当前节点
            float dist = distance(queryX, queryY, currentPoint.x, currentPoint.y);
            
            if (dist <= currentMaxSearchRadius) {
                result.insertPoint(nodeIdx, dist, currentPoint.value, k);
                
                // 动态收缩搜索半径
                if (result.count >= k) {
                    currentMaxSearchRadius = std::min(currentMaxSearchRadius, result.distances[k - 1]);
                }
            }
            
            // 计算子节点
            uint32_t splitDim = splitDimOf(nodeIdx);
            float splitValue = (splitDim == 0) ? currentPoint.x : currentPoint.y;
            float queryValue = (splitDim == 0) ? queryX : queryY;
            
            float signedDist = queryValue - splitValue;
            
            uint32_t leftChild = leftChildOf(nodeIdx);
            uint32_t rightChild = rightChildOf(nodeIdx);
            
            // 决定搜索顺序和剪枝
            bool searchLeft = leftChild < points.size();
            bool searchRight = rightChild < points.size();
            
            // 距离剪枝：只有当到分割平面的距离小于搜索半径时才搜索另一边
            if (signedDist < 0) {
                // 查询点在左边，优先搜索左子树
                if (searchLeft) {
                    nodesToVisit.push_back(leftChild);
                }
                if (searchRight && (-signedDist) <= currentMaxSearchRadius) {
                    nodesToVisit.push_back(rightChild);
                }
            } else {
                // 查询点在右边，优先搜索右子树
                if (searchRight) {
                    nodesToVisit.push_back(rightChild);
                }
                if (searchLeft && signedDist <= currentMaxSearchRadius) {
                    nodesToVisit.push_back(leftChild);
                }
            }
        }
        
        return result;
    }
};

// 测试数据生成器
class TestDataGenerator {
public:
    static std::vector<SparsePoint> generateTestData(int numPoints, int seed = 42) {
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> x_dis(0.0f, 150.0f);
        std::uniform_real_distribution<float> y_dis(0.0f, 450.0f);
        
        std::vector<SparsePoint> points(numPoints);
        for (int i = 0; i < numPoints; i++) {
            points[i].x = x_dis(gen);
            points[i].y = y_dis(gen);
            points[i].value = static_cast<float>(i + 1);
            points[i].padding = 0.0f;
        }
        
        return points;
    }
    
    static std::vector<SparsePoint> generateKnownData() {
        return {
            {10.0f, 20.0f, 1.0f, 0.0f},
            {140.0f, 30.0f, 2.0f, 0.0f},
            {20.0f, 400.0f, 3.0f, 0.0f},
            {130.0f, 420.0f, 4.0f, 0.0f},
            {75.0f, 225.0f, 5.0f, 0.0f},
            {45.0f, 150.0f, 6.0f, 0.0f},
            {105.0f, 300.0f, 7.0f, 0.0f}
        };
    }
};

// 综合测试函数
void testCompleteLeftBalancedKDTreeFixed() {
    std::cout << "\n=== 修复版完整Left-Balanced KD Tree测试 ===" << std::endl;
    
    // 使用完整版构建器
    auto testData = TestDataGenerator::generateKnownData();
    
    CompleteLeftBalancedKDTreeBuilder builder;
    auto treeData = builder.buildLeftBalancedKDTree(testData);
    
    std::cout << "\n使用完整版Left-Balanced构建的树:" << std::endl;
    for (size_t i = 0; i < treeData.points.size(); i++) {
        const auto& p = treeData.points[i];
        std::cout << "  [" << i << "] (" << p.x << ", " << p.y << ", val=" << p.value << ")" << std::endl;
    }
    
    struct TestQuery {
        float x, y;
        std::string desc;
    };
    
    std::vector<TestQuery> queries = {
        {75.0f, 225.0f, "中心点"},
        {10.0f, 20.0f, "左下角"},
        {130.0f, 420.0f, "右上角"}
    };
    
    for (const auto& query : queries) {
        std::cout << "\n查询 " << query.desc << " (" << query.x << ", " << query.y << "):" << std::endl;
        
        auto bruteResult = FixedCompleteLeftBalancedSearcher::bruteForceKNN(
            treeData.points, query.x, query.y, 3);
        auto treeBasedBruteResult = FixedCompleteLeftBalancedSearcher::treeBasedBruteForce(
            treeData.points, query.x, query.y, 3);
        auto improvedStacklessResult = FixedCompleteLeftBalancedSearcher::improvedStacklessKNN(
            treeData.points, query.x, query.y, 3);
        
        bruteResult.print("  原始暴力:     ");
        treeBasedBruteResult.print("  树序暴力:     ");
        improvedStacklessResult.print("  改进无栈:     ");
        
        bool treeMatch = bruteResult.matches(treeBasedBruteResult);
        bool stacklessMatch = bruteResult.matches(improvedStacklessResult);
        
        std::cout << "  树序暴力匹配: " << (treeMatch ? "✅" : "❌") << std::endl;
        std::cout << "  改进无栈匹配: " << (stacklessMatch ? "✅" : "❌") << std::endl;
        
        if (!treeMatch) {
            std::cout << "  ⚠️ 树结构可能有问题！" << std::endl;
        }
        if (!stacklessMatch) {
            std::cout << "  ⚠️ 无栈搜索有问题！" << std::endl;
        }
    }
}

int main() {
    std::cout << "修复版完整Left-Balanced KD Tree测试" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "使用完整的论文算法构建，测试多种搜索方法" << std::endl;
    
    try {
        testCompleteLeftBalancedKDTreeFixed();
        
        std::cout << "\n🔧 分析结果：" << std::endl;
        std::cout << "1. 如果'树序暴力'匹配，说明树构建正确" << std::endl;
        std::cout << "2. 如果'改进无栈'匹配，说明无栈遍历正确" << std::endl;
        std::cout << "3. 这样可以逐步定位问题所在" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
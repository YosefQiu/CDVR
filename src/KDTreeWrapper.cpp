#include "KDTreeWrapper.h"

KDTreeBuilder::KDTreeBuilder() 
    : m_pointCount(0), m_isBuilt(false)
{
}

KDTreeBuilder::~KDTreeBuilder()
{
    clear();
}

bool KDTreeBuilder::buildTree(const std::vector<SparsePoint>& inputPoints)
{
    return buildTree(inputPoints.data(), inputPoints.size());
}

bool KDTreeBuilder::buildTree(const SparsePoint* points, size_t numPoints)
{
    if (!points || numPoints == 0) {
        std::cerr << "KDTreeBuilder: Invalid input points" << std::endl;
        return false;
    }
    
    clear();
    
    // 转换输入格式
    m_originalPoints.assign(points, points + numPoints);
    m_kdtreePoints.reserve(numPoints);
    
    for (size_t i = 0; i < numPoints; ++i) {
        m_kdtreePoints.push_back(sparseToKDTree(points[i]));
    }
    
    try {
        // 构建KDTree
        auto start = std::chrono::high_resolution_clock::now();
        kdTree::buildTree_host<kdTree::float2, kdTree::default_data_traits<kdTree::float2>>(
            m_kdtreePoints.data(), numPoints, &m_worldBounds);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "[KDTree] KDTree built successfully in " << duration_ms.count() << " ms" << std::endl;
        
        m_pointCount = numPoints;
        m_isBuilt = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "KDTreeBuilder: Failed to build tree - " << e.what() << std::endl;
        clear();
        return false;
    }
}

template<int K>
bool KDTreeBuilder::knnSearch(const SparsePoint& queryPoint, float searchRadius, 
                             std::vector<GPUPoint>& results, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder: Tree not built" << std::endl;
        return false;
    }
    
    // 转换查询点格式
    kdTree::float2 queryKDTree = sparseToKDTree(queryPoint);
    
    // 创建候选列表
    kdTree::FixedCandidateList<K> candidateList(searchRadius);
    
    try {
        // 执行KNN查询
        kdTree::knn<kdTree::FixedCandidateList<K>, kdTree::float2, kdTree::default_data_traits<kdTree::float2>>(
            candidateList, queryKDTree, m_kdtreePoints.data(), m_pointCount);
        
        // 转换结果
        results.clear();
        distances.clear();
        results.reserve(K);
        distances.reserve(K);
        
        for (int i = 0; i < K; ++i) 
        {
            int pointID = candidateList.get_pointID(i);
            float dist2 = candidateList.get_dist2(i);
            
            if (pointID >= 0 && pointID < static_cast<int>(m_pointCount)) 
            {
                float x = m_kdtreePoints[pointID].x;
                float y = m_kdtreePoints[pointID].y;
                float value;
                for (auto & originalPoint : m_originalPoints) 
                {
                    if (originalPoint.x == x && originalPoint.y == y) 
                    {
                        value = originalPoint.value;
                        break;
                    }
                }
                
                results.push_back({x, y, value, 0.0f});
                distances.push_back(std::sqrt(dist2));
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "KDTreeBuilder: Search failed - " << e.what() << std::endl;
        return false;
    }
}

template<int K>
bool KDTreeBuilder::knnSearch(const SparsePoint& queryPoint, float searchRadius,
                             std::vector<int>& indices, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder: Tree not built" << std::endl;
        return false;
    }
    
    // 转换查询点格式
    kdTree::float2 queryKDTree = sparseToKDTree(queryPoint);
    
    // 创建候选列表
    kdTree::FixedCandidateList<K> candidateList(searchRadius);
    
    try {
        // 执行KNN查询
        kdTree::knn<kdTree::FixedCandidateList<K>, kdTree::float2, kdTree::default_data_traits<kdTree::float2>>(
            candidateList, queryKDTree, m_kdtreePoints.data(), m_pointCount);
        
        // 转换结果
        indices.clear();
        distances.clear();
        indices.reserve(K);
        distances.reserve(K);
        
        for (int i = 0; i < K; ++i) {
            int pointID = candidateList.get_pointID(i);
            float dist2 = candidateList.get_dist2(i);
            
            if (pointID >= 0 && pointID < static_cast<int>(m_pointCount)) {
                indices.push_back(pointID);
                distances.push_back(std::sqrt(dist2));
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "KDTreeBuilder: Search failed - " << e.what() << std::endl;
        return false;
    }
}

std::vector<GPUPoint> KDTreeBuilder::getGPUPoints() const
{
    std::vector<GPUPoint> gpuPoints;
    if (!m_isBuilt) {
        return gpuPoints;
    }
    
    gpuPoints.reserve(m_pointCount);
    for (const auto& point : m_kdtreePoints) 
    {
        float x = point.x; float y = point.y;
        float value = 0.0f;
        for (const auto& originalPoint : m_originalPoints)
        {
            if (originalPoint.x == x && originalPoint.y == y) 
            {
                value = originalPoint.value;
                break;
            }
        }
        gpuPoints.push_back({x, y, value, 0.0f});
    }
    
    return gpuPoints;
}

bool KDTreeBuilder::getWorldBounds(float& minX, float& maxX, float& minY, float& maxY) const
{
    if (!m_isBuilt) {
        return false;
    }
    
    minX = m_worldBounds.lower.x;
    maxX = m_worldBounds.upper.x;
    minY = m_worldBounds.lower.y;
    maxY = m_worldBounds.upper.y;
    
    return true;
}

void KDTreeBuilder::clear()
{
    m_kdtreePoints.clear();
    m_originalPoints.clear();
    m_pointCount = 0;
    m_isBuilt = false;
}

// 辅助函数实现
kdTree::float2 KDTreeBuilder::sparseToKDTree(const SparsePoint& point) const
{
    return kdTree::make_float2(point.x, point.y);
}

GPUPoint KDTreeBuilder::sparseToGPU(const SparsePoint& point) const
{
    GPUPoint gpuPoint;
    gpuPoint.x = point.x;
    gpuPoint.y = point.y;
    gpuPoint.value = point.value;
    gpuPoint.padding = point.padding;
    return gpuPoint;
}

SparsePoint KDTreeBuilder::kdtreeToSparse(const kdTree::float2& point, int originalIndex) const
{
    if (originalIndex >= 0 && originalIndex < static_cast<int>(m_originalPoints.size())) {
        return m_originalPoints[originalIndex];
    }
    
    // 如果找不到原始索引，创建一个基本的SparsePoint
    SparsePoint sparse;
    sparse.x = point.x;
    sparse.y = point.y;
    sparse.value = 0.0f;
    sparse.padding = 0.0f;
    return sparse;
}

template bool KDTreeBuilder::knnSearch<1>(const SparsePoint&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder::knnSearch<3>(const SparsePoint&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder::knnSearch<5>(const SparsePoint&, float, std::vector<int>&, std::vector<float>&) const;

template bool KDTreeBuilder::knnSearch<1>(const SparsePoint&, float, std::vector<GPUPoint>&, std::vector<float>&) const;
template bool KDTreeBuilder::knnSearch<3>(const SparsePoint&, float, std::vector<GPUPoint>&, std::vector<float>&) const;
template bool KDTreeBuilder::knnSearch<5>(const SparsePoint&, float, std::vector<GPUPoint>&, std::vector<float>&) const;
#include "KDTreeWrapper.h"
#include "common.hpp"

KDTreeBuilder2D::KDTreeBuilder2D() 
    : m_pointCount(0), m_isBuilt(false)
{
}

KDTreeBuilder2D::~KDTreeBuilder2D()
{
    clear();
}

bool KDTreeBuilder2D::buildTree(const std::vector<SparsePoint2D>& inputPoints)
{
    return buildTree(inputPoints.data(), inputPoints.size());
}

bool KDTreeBuilder2D::buildTree(const SparsePoint2D* points, size_t numPoints)
{
    if (!points || numPoints == 0) {
        std::cerr << "KDTreeBuilder2D: Invalid input points" << std::endl;
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
        std::cerr << "KDTreeBuilder2D: Failed to build tree - " << e.what() << std::endl;
        clear();
        return false;
    }
}

template<int K>
bool KDTreeBuilder2D::knnSearch(const SparsePoint2D& queryPoint, float searchRadius, 
                             std::vector<GPUPoint2D>& results, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder2D: Tree not built" << std::endl;
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
        std::cerr << "KDTreeBuilder2D: Search failed - " << e.what() << std::endl;
        return false;
    }
}

template<int K>
bool KDTreeBuilder2D::knnSearch(const SparsePoint2D& queryPoint, float searchRadius,
                             std::vector<int>& indices, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder2D: Tree not built" << std::endl;
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
        std::cerr << "KDTreeBuilder2D: Search failed - " << e.what() << std::endl;
        return false;
    }
}

std::vector<GPUPoint2D> KDTreeBuilder2D::getGPUPoints() const
{
    std::vector<GPUPoint2D> gpuPoints;
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

bool KDTreeBuilder2D::getWorldBounds(float& minX, float& maxX, float& minY, float& maxY) const
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

void KDTreeBuilder2D::clear()
{
    m_kdtreePoints.clear();
    m_originalPoints.clear();
    m_pointCount = 0;
    m_isBuilt = false;
}

// 辅助函数实现
kdTree::float2 KDTreeBuilder2D::sparseToKDTree(const SparsePoint2D& point) const
{
    return kdTree::make_float2(point.x, point.y);
}

GPUPoint2D KDTreeBuilder2D::sparseToGPU(const SparsePoint2D& point) const
{
    GPUPoint2D gpuPoint;
    gpuPoint.x = point.x;
    gpuPoint.y = point.y;
    gpuPoint.value = point.value;
    gpuPoint.padding = point.padding;
    return gpuPoint;
}

SparsePoint2D KDTreeBuilder2D::kdtreeToSparse(const kdTree::float2& point, int originalIndex) const
{
    if (originalIndex >= 0 && originalIndex < static_cast<int>(m_originalPoints.size())) {
        return m_originalPoints[originalIndex];
    }
    
    // 如果找不到原始索引，创建一个基本的SparsePoint
    SparsePoint2D sparse;
    sparse.x = point.x;
    sparse.y = point.y;
    sparse.value = 0.0f;
    sparse.padding = 0.0f;
    return sparse;
}

template bool KDTreeBuilder2D::knnSearch<1>(const SparsePoint2D&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder2D::knnSearch<3>(const SparsePoint2D&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder2D::knnSearch<5>(const SparsePoint2D&, float, std::vector<int>&, std::vector<float>&) const;

template bool KDTreeBuilder2D::knnSearch<1>(const SparsePoint2D&, float, std::vector<GPUPoint2D>&, std::vector<float>&) const;
template bool KDTreeBuilder2D::knnSearch<3>(const SparsePoint2D&, float, std::vector<GPUPoint2D>&, std::vector<float>&) const;
template bool KDTreeBuilder2D::knnSearch<5>(const SparsePoint2D&, float, std::vector<GPUPoint2D>&, std::vector<float>&) const;

//// 3D KDTreeBuilder Implementation

KDTreeBuilder3D::KDTreeBuilder3D() 
    : m_pointCount(0), m_isBuilt(false)
{
}

KDTreeBuilder3D::~KDTreeBuilder3D()
{
    clear();
}

bool KDTreeBuilder3D::buildTree(const std::vector<SparsePoint3D>& inputPoints)
{
    return buildTree(inputPoints.data(), inputPoints.size());
}

bool KDTreeBuilder3D::buildTree(const SparsePoint3D* points, size_t numPoints)
{
    if (!points || numPoints == 0) {
        std::cerr << "KDTreeBuilder2D: Invalid input points" << std::endl;
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
        kdTree::buildTree_host<kdTree::float3, kdTree::default_data_traits<kdTree::float3>>(
            m_kdtreePoints.data(), numPoints, &m_worldBounds);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "[KDTree] KDTree built successfully in " << duration_ms.count() << " ms" << std::endl;
        
        m_pointCount = numPoints;
        m_isBuilt = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "KDTreeBuilder3D: Failed to build tree - " << e.what() << std::endl;
        clear();
        return false;
    }
}

template<int K>
bool KDTreeBuilder3D::knnSearch(const SparsePoint3D& queryPoint, float searchRadius,
                             std::vector<GPUPoint3D>& results, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder3D: Tree not built" << std::endl;
        return false;
    }
    
    // 转换查询点格式
    kdTree::float3 queryKDTree = sparseToKDTree(queryPoint);
    
    // 创建候选列表
    kdTree::FixedCandidateList<K> candidateList(searchRadius);
    
    try {
        // 执行KNN查询
        kdTree::knn<kdTree::FixedCandidateList<K>, kdTree::float3, kdTree::default_data_traits<kdTree::float3>>(
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
                float z = m_kdtreePoints[pointID].z;
                float value;
                for (auto & originalPoint : m_originalPoints) 
                {
                    if (originalPoint.x == x && originalPoint.y == y && originalPoint.z == z) 
                    {
                        value = originalPoint.value;
                        break;
                    }
                }

                results.push_back({x, y, z, value, {}});
                distances.push_back(std::sqrt(dist2));
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "KDTreeBuilder3D: Search failed - " << e.what() << std::endl;
        return false;
    }
}

template<int K>
bool KDTreeBuilder3D::knnSearch(const SparsePoint3D& queryPoint, float searchRadius,
                             std::vector<int>& indices, std::vector<float>& distances) const
{
    if (!m_isBuilt) {
        std::cerr << "KDTreeBuilder3D: Tree not built" << std::endl;
        return false;
    }
    
    // 转换查询点格式
    kdTree::float3 queryKDTree = sparseToKDTree(queryPoint);
    
    // 创建候选列表
    kdTree::FixedCandidateList<K> candidateList(searchRadius);
    
    try {
        // 执行KNN查询
        kdTree::knn<kdTree::FixedCandidateList<K>, kdTree::float3, kdTree::default_data_traits<kdTree::float3>>(
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
        std::cerr << "KDTreeBuilder3D: Search failed - " << e.what() << std::endl;
        return false;
    }
}

std::vector<GPUPoint3D> KDTreeBuilder3D::getGPUPoints() const
{
    std::vector<GPUPoint3D> gpuPoints;
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

bool KDTreeBuilder3D::getWorldBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const
{
    if (!m_isBuilt) {
        return false;
    }
    
    minX = m_worldBounds.lower.x;
    maxX = m_worldBounds.upper.x;
    minY = m_worldBounds.lower.y;
    maxY = m_worldBounds.upper.y;
    minZ = m_worldBounds.lower.z;
    maxZ = m_worldBounds.upper.z;

    return true;
}

void KDTreeBuilder3D::clear()
{
    m_kdtreePoints.clear();
    m_originalPoints.clear();
    m_pointCount = 0;
    m_isBuilt = false;
}

// 辅助函数实现
kdTree::float3 KDTreeBuilder3D::sparseToKDTree(const SparsePoint3D& point) const
{
    return kdTree::make_float3(point.x, point.y, point.z);
}

GPUPoint3D KDTreeBuilder3D::sparseToGPU(const SparsePoint3D& point) const
{
    GPUPoint3D gpuPoint;
    gpuPoint.x = point.x;
    gpuPoint.y = point.y;
    gpuPoint.z = point.z;
    gpuPoint.value = point.value;
    gpuPoint.padding[0] = point.padding[0];
    gpuPoint.padding[1] = point.padding[1];
    gpuPoint.padding[2] = point.padding[2];
    return gpuPoint;
}

SparsePoint3D KDTreeBuilder3D::kdtreeToSparse(const kdTree::float3& point, int originalIndex) const
{
    if (originalIndex >= 0 && originalIndex < static_cast<int>(m_originalPoints.size())) {
        return m_originalPoints[originalIndex];
    }
    
    // 如果找不到原始索引，创建一个基本的SparsePoint
    SparsePoint3D sparse;
    sparse.x = point.x;
    sparse.y = point.y;
    sparse.z = point.z;
    sparse.value = 0.0f;
    sparse.padding[0] = 0.0f;
    sparse.padding[1] = 0.0f;
    sparse.padding[2] = 0.0f;
    return sparse;
}

template bool KDTreeBuilder3D::knnSearch<1>(const SparsePoint3D&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder3D::knnSearch<3>(const SparsePoint3D&, float, std::vector<int>&, std::vector<float>&) const;
template bool KDTreeBuilder3D::knnSearch<5>(const SparsePoint3D&, float, std::vector<int>&, std::vector<float>&) const;

template bool KDTreeBuilder3D::knnSearch<1>(const SparsePoint3D&, float, std::vector<GPUPoint3D>&, std::vector<float>&) const;
template bool KDTreeBuilder3D::knnSearch<3>(const SparsePoint3D&, float, std::vector<GPUPoint3D>&, std::vector<float>&) const;
template bool KDTreeBuilder3D::knnSearch<5>(const SparsePoint3D&, float, std::vector<GPUPoint3D>&, std::vector<float>&) const;
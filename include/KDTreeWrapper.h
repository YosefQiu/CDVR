#pragma once
#include "ggl.h"
#include "kdtree.h"

struct SparsePoint2D 
{
    float x;
    float y;
    float value;
    float padding;  // 填充到16字节对齐
};

struct SparsePoint3D 
{
    float x;
    float y;
    float z;
    float value;
    float padding[3];  // 填充到32字节对齐
};


struct GPUPoint2D {
    float x, y;
    float value;
    float padding;  // 保持16字节对齐
};

struct GPUPoint3D {
    float x, y, z;
    float value;
    float padding[4];  // 保持32字节对齐
};

class KDTreeBuilder2D 
{
public:
    KDTreeBuilder2D();
    ~KDTreeBuilder2D();
    struct TreeData2D 
    {
        std::vector<GPUPoint2D> points;
        size_t numLevels;
    };
    // 构建KDTree
    bool buildTree(const std::vector<SparsePoint2D>& inputPoints);
    bool buildTree(const SparsePoint2D* points, size_t numPoints);
    
    // K近邻查询
    template<int K>
    bool knnSearch(const SparsePoint2D& queryPoint, float searchRadius, 
                   std::vector<GPUPoint2D>& results, std::vector<float>& distances) const;
    
    // 重载版本，直接返回索引和距离
    template<int K>
    bool knnSearch(const SparsePoint2D& queryPoint, float searchRadius,
                   std::vector<int>& indices, std::vector<float>& distances) const;
    
    // 获取构建的点数据（转换为GPUPoint2D格式）
    std::vector<GPUPoint2D> getGPUPoints() const;
    
    // 获取世界边界
    bool getWorldBounds(float& minX, float& maxX, float& minY, float& maxY) const;
    
    // 获取树的统计信息
    size_t getPointCount() const { return m_pointCount; }
    bool isBuilt() const { return m_isBuilt; }
    size_t getNumLevels() const 
    {
        return kdTree::BinaryTree::numLevelsFor(m_pointCount);
    }
    
    // 清理资源
    void clear();

private:
    // 内部数据
    std::vector<kdTree::float2> m_kdtreePoints;  // KDTree使用的点格式
    std::vector<SparsePoint2D> m_originalPoints;   // 原始输入点
    kdTree::box_t<kdTree::float2> m_worldBounds; // 世界边界
    size_t m_pointCount;
    bool m_isBuilt;
    
    // 辅助函数
    kdTree::float2 sparseToKDTree(const SparsePoint2D& point) const;
    GPUPoint2D sparseToGPU(const SparsePoint2D& point) const;
    SparsePoint2D kdtreeToSparse(const kdTree::float2& point, int originalIndex) const;
};

class KDTreeBuilder3D 
{
public:
    KDTreeBuilder3D();
    ~KDTreeBuilder3D();
    struct TreeData3D 
    {
        std::vector<GPUPoint3D> points;
        size_t numLevels;
    };
    // 构建KDTree
    bool buildTree(const std::vector<SparsePoint3D>& inputPoints);
    bool buildTree(const SparsePoint3D* points, size_t numPoints);
    
    // K近邻查询
    template<int K>
    bool knnSearch(const SparsePoint3D& queryPoint, float searchRadius,
                   std::vector<GPUPoint3D>& results, std::vector<float>& distances) const;

    // 重载版本，直接返回索引和距离
    template<int K>
    bool knnSearch(const SparsePoint3D& queryPoint, float searchRadius,
                   std::vector<int>& indices, std::vector<float>& distances) const;

    // 获取构建的点数据（转换为GPUPoint3D格式）
    std::vector<GPUPoint3D> getGPUPoints() const;
    
    // 获取世界边界
    bool getWorldBounds(float& minX, float& maxX, float& minY, float& maxY, float& minZ, float& maxZ) const;

    // 获取树的统计信息
    size_t getPointCount() const { return m_pointCount; }
    bool isBuilt() const { return m_isBuilt; }
    size_t getNumLevels() const 
    {
        return kdTree::BinaryTree::numLevelsFor(m_pointCount);
    }
    
    // 清理资源
    void clear();

private:
    // 内部数据
    std::vector<kdTree::float3> m_kdtreePoints;  // KDTree使用的点格式
    std::vector<SparsePoint3D> m_originalPoints;   // 原始输入点
    kdTree::box_t<kdTree::float3> m_worldBounds; // 世界边界
    size_t m_pointCount;
    bool m_isBuilt;
    
    // 辅助函数
    kdTree::float3 sparseToKDTree(const SparsePoint3D& point) const;
    GPUPoint3D sparseToGPU(const SparsePoint3D& point) const;
    SparsePoint3D kdtreeToSparse(const kdTree::float3& point, int originalIndex) const;
};
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>

#include "kdtree.h"


struct SparsePoint 
{
    float x;
    float y;
    float value;
    float padding;  
};


struct GPUPoint 
{
    float x, y;
    float value;
    float padding; 
};

struct DataHeader 
{
    uint32_t width;
    uint32_t height;
    uint32_t numPoints;
};


std::vector<SparsePoint> InitDataFromBinary(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) 
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }

    DataHeader m_header;
    file.read(reinterpret_cast<char*>(&m_header), sizeof(DataHeader));
    
    std::vector<SparsePoint> m_sparsePoints;
    m_sparsePoints.resize(m_header.numPoints);
    file.read(reinterpret_cast<char*>(m_sparsePoints.data()), 
              m_header.numPoints * sizeof(SparsePoint));
    
    file.close();

    return m_sparsePoints;
}

void TEST()
{
    using namespace kdTree;
    // 1. loading data
    auto TestData = InitDataFromBinary("../../pruned_simple_data.bin");
    const int numPoints = TestData.size();
    std::vector<float2> points(numPoints);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 150.0f);

    std::cout << "Loading " << numPoints << " random 2D points..." << std::endl;
    for (int i = 0; i < numPoints; i++) 
        points[i] = make_float2(TestData[i].x, TestData[i].y);

    // 2. building KDTree
    std::cout << "Building KDTree..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    box_t<float2> worldBounds;
    buildTree_host<float2, default_data_traits<float2>>(points.data(), numPoints, &worldBounds);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "KDTree building completed in: " << duration_ms.count() << " ms" << std::endl;

    std::cout << "World bounds: " << worldBounds << std::endl;

    // 3. performing k-nearest neighbor search
    const int k = 5;
    const float searchRadius = 50.0f;

    // 4. Choose a random query point within the bounds
    float2 queryPoint = make_float2(dis(gen), dis(gen));
    std::cout << "\nSearching for " << k << " nearest neighbors at point " << queryPoint << "..." << std::endl;

    // 5. Create candidate list
    FixedCandidateList<k> candidateList(searchRadius);

    // 6. Perform query using KDTree
    start = std::chrono::high_resolution_clock::now();
    knn<FixedCandidateList<k>, float2, default_data_traits<float2>>
    (candidateList, queryPoint, points.data(), numPoints);
    end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "KDTree search completed in: " << duration_us.count() << " μs" << std::endl;

    for (int i = 0; i < k; i++) 
    {
        int pointID = candidateList.get_pointID(i);
        float dist2 = candidateList.get_dist2(i);
        
        if (pointID >= 0 && pointID < numPoints) 
        {
            float2 &point = points[pointID];
            std::cout << "  " << i << ": ID=" << pointID 
                    << ", Distance²=" << dist2 
                    << ", Distance=" << std::sqrt(dist2)
                    << ", Point=(" << point.x << "," << point.y << ")"
                    << std::endl;
        }
    }

    // 7. Perform query using brute-force search
    start = std::chrono::high_resolution_clock::now();
    auto bruteResult = bruteForceKNN<k>(points, queryPoint, searchRadius);
    end = std::chrono::high_resolution_clock::now();
    auto brute_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Brute-force search completed in: " << brute_duration.count() << " μs" << std::endl;
    for (int i = 0; i < k; i++) 
    {
        int pointID = bruteResult.get_pointID(i);
        float dist2 = bruteResult.get_dist2(i);
        
        if (pointID >= 0 && pointID < numPoints) 
        {
            float2 &point = points[pointID];
            std::cout << "  " << i << ": ID=" << pointID 
                    << ", Distance²=" << dist2 
                    << ", Distance=" << std::sqrt(dist2)
                    << ", Point=(" << point.x << "," << point.y << ")"
                    << std::endl;
        }
    }

   
    // 8. Compare results
    std::cout << "\nComparing results:" << std::endl;
    bool resultsMatch = true;
    
    for (int i = 0; i < k; i++) 
    {
        int kdtreeID = candidateList.get_pointID(i);
        int bruteID = bruteResult.get_pointID(i);
        float kdtreeDist2 = candidateList.get_dist2(i);
        float bruteDist2 = bruteResult.get_dist2(i);
        
        float distDiff = std::abs(kdtreeDist2 - bruteDist2);
        if (distDiff > 1e-6f) 
        {
            resultsMatch = false;
            std::cout << "  Position " << i << ": Distance mismatch! KDTree=" 
            << kdtreeDist2 << ", Brute-force=" << bruteDist2 << std::endl;
            std::cout << "  KDTree ID: " << kdtreeID 
            << ", Brute-force ID: " << bruteID << std::endl;
        }
    }
    
    std::cout << "  KDTree acceleration ratio: " << (float)brute_duration.count() / duration_us.count() << "x" << std::endl;
    if (resultsMatch) 
        std::cout << "  ✓ All results match!" << std::endl;
    else 
        std::cout << "  ✗ Results do not match" << std::endl;

}


int main() 
{

    TEST();
    
   
    
    return 0;
}
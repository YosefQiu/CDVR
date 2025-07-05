#pragma once
#include "traverse.hpp"

namespace kdTree 
{
    struct BaseCandidateList 
    {
    protected:
        inline uint64_t encode(float f, int i);
        inline float decode_dist2(uint64_t v) const;
        inline int   decode_pointID(uint64_t v) const;
    };

    template<int k>
    struct CandidateList : public BaseCandidateList 
    {

        inline CandidateList(float cutOffRadius) {}
        inline float maxRadius2() const /* abstract */;
        inline float get_dist2(int i) const;
        inline int   get_pointID(int i) const; /*! returns ID of i'th found k-nearest data point */

        using BaseCandidateList::encode;
        using BaseCandidateList::decode_dist2;
        using BaseCandidateList::decode_pointID;
        
        /*! storage for k elements; we encode those float:int pairs as a
            single int64 to make reading/writing/swapping faster */
        uint64_t entry[k];
        enum { num_k = k };
    };

    template<int k>
    struct FixedCandidateList : public CandidateList<k>
    {
        using CandidateList<k>::entry;
        using CandidateList<k>::encode;
        
        FixedCandidateList(float cutOffRadius) : CandidateList<k>(cutOffRadius)
        {
            for (int i=0;i<k;i++)
                entry[i] = this->encode(cutOffRadius*cutOffRadius,-1);
        }
        
        float maxRadius2() const 
        { return this->decode_dist2(entry[k-1]); }
        
        float returnValue() const 
        { return maxRadius2(); }
        
        float processCandidate(int candPrimID, float candDist2)
        {
            push(candDist2,candPrimID);
            return maxRadius2();
        }
        
        float initialCullDist2() const 
        { return maxRadius2(); }

        void push(float dist, int pointID)
        {
            uint64_t v = this->encode(dist,pointID);
            for (int i=0;i<k;i++) 
            {
                uint64_t vmax = std::max(entry[i],v);
                uint64_t vmin = std::min(entry[i],v);
                entry[i] = vmin;
                v = vmax;
            }
        }
    };

    template<int k>
    struct BruteForceResult 
    {
        struct Entry 
        {
            float dist2;
            int pointID;
            
            bool operator<(const Entry& other) const 
            {
                return dist2 < other.dist2;
            }
        };
        
        Entry entries[k];
        int count;
        
        BruteForceResult() : count(0) 
        {
            for (int i = 0; i < k; i++) 
            {
                entries[i].dist2 = std::numeric_limits<float>::max();
                entries[i].pointID = -1;
            }
        }
        
        void addCandidate(float dist2, int pointID) 
        {
            if (count < k) 
            {
                entries[count].dist2 = dist2;
                entries[count].pointID = pointID;
                count++;
                std::sort(entries, entries + count);
            } 
            else if (dist2 < entries[k-1].dist2) 
            {
                entries[k-1].dist2 = dist2;
                entries[k-1].pointID = pointID;
                std::sort(entries, entries + k);
            }
        }
        
        float get_dist2(int i) const 
        {
            return (i < count) ? entries[i].dist2 : std::numeric_limits<float>::max();
        }

        int get_pointID(int i) const 
        {
            return (i < count) ? entries[i].pointID : -1;
        }
    };


    template<typename CandidateList, typename data_t, typename data_traits=default_data_traits<data_t>>
    inline float knn(CandidateList &result, typename data_traits::point_t queryPoint, const data_t *d_nodes, int N);
    
    template<typename CandidateList, typename data_t, typename data_traits=default_data_traits<data_t>>
    inline float knn(CandidateList &result, typename data_traits::point_t queryPoint, const box_t<typename data_traits::point_t> worldBounds, const data_t *d_nodes, int N)
    {
        /* TODO: add early-out if distance to worldbounds is >= max query dist */
        return knn<CandidateList,data_t,data_traits>(result,queryPoint,worldBounds,d_nodes,N);
    }


    inline float uint_as_float(uint32_t u)
    {
        union 
        {
            uint32_t u;
            float f;
        } converter;
        converter.u = u;
        return converter.f;
    }

    inline uint32_t float_as_uint(float f)
    {
        union 
        {
            float f;
            uint32_t u;
        } converter;
        converter.f = f;
        return converter.u;
    }

    template<int k>
    inline float CandidateList<k>::get_dist2(int i) const
    { return decode_dist2(entry[i]); }

    template<int k>
    inline int CandidateList<k>::get_pointID(int i) const
    { return decode_pointID(entry[i]); }
    
    inline uint64_t BaseCandidateList::encode(float f, int i)
    { return (uint64_t(float_as_uint(f)) << 32) | uint32_t(i); }

    inline float BaseCandidateList::decode_dist2(uint64_t v) const
    { return uint_as_float(v >> 32); }

    inline int BaseCandidateList::decode_pointID(uint64_t v) const
    { return int(uint32_t(v)); }


    template<typename CandidateList, typename data_t, typename data_traits>
    inline float knn(CandidateList &result, typename data_traits::point_t queryPoint, const data_t *d_nodes, int N)
    {
        traverse_stack_free<CandidateList,data_t,data_traits>(result,queryPoint,d_nodes,N);
        return result.returnValue();
    }


    template<int k>
    BruteForceResult<k> bruteForceKNN(const std::vector<float3>& points, const float3& queryPoint, float maxRadius = std::numeric_limits<float>::max()) 
    {
        BruteForceResult<k> result;
        float maxRadius2 = maxRadius * maxRadius;
        
        for (std::size_t i = 0; i < points.size(); i++) 
        {
            float dist2 = sqrDistance(points[i], queryPoint);
            if (dist2 <= maxRadius2) 
            {
                result.addCandidate(dist2, i);
            }
        }
        
        return result;
    }

    template<int k>
    BruteForceResult<k> bruteForceKNN(const std::vector<float2>& points, const float2& queryPoint, float maxRadius = std::numeric_limits<float>::max()) 
    {
        BruteForceResult<k> result;
        float maxRadius2 = maxRadius * maxRadius;
        
        for (std::size_t i = 0; i < points.size(); i++) 
        {
            float dist2 = sqrDistance(points[i], queryPoint);
            if (dist2 <= maxRadius2) 
            {
                result.addCandidate(dist2, i);
            }
        }
        return result;
    }
}


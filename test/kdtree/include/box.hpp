#pragma once
#include "common.hpp"  

namespace kdTree
{
    template<typename T> inline T empty_box_lower();
    template<typename T> inline T empty_box_upper();
    template<> inline float empty_box_lower<float>() { return +INFINITY; }
    template<> inline float empty_box_upper<float>() { return -INFINITY; }
    template<> inline int empty_box_lower<int>() { return std::numeric_limits<int>::max(); }
    template<> inline int empty_box_upper<int>() { return std::numeric_limits<int>::min(); }

    template<typename point_t>
    struct box_t 
    {
        using point_traits = kdTree::point_traits<point_t>;
        using scalar_t = typename point_traits::scalar_t;
    
        inline int widestDimension() const
        {
            enum { num_dims = point_traits::num_dims };
        
            int d_best = 0;
            scalar_t w_best = scalar_t(0);
            for (int d=0;d<num_dims;d++) 
            {
                scalar_t w_d = get_coord(upper,d) - get_coord(lower,d);
                if (w_d < w_best) continue;
                w_best = w_d;
                d_best = d;
            }
            return d_best;
        }
        
        inline bool contains(const point_t &p) const
        {
            enum { num_dims = num_dims_of<point_t>::value };
            for (int d=0;d<num_dims;d++) 
            {
                if (point_traits::get_coord(p,d) < point_traits::get_coord(lower,d)) return false;
                if (point_traits::get_coord(p,d) > point_traits::get_coord(upper,d)) return false;
            }
            return true;
        }
        
        inline void grow(const point_t &p)
        {
            lower = min(lower,p);
            upper = max(upper,p);
        }
        
        inline void setEmpty()
        {
            for (int d=0;d<point_traits::num_dims;d++) 
            {
                point_traits::set_coord(lower,d,empty_box_lower<scalar_t>());
                point_traits::set_coord(upper,d,empty_box_upper<scalar_t>());
            }
        }
        
        inline void setInfinite()
        {
            for (int d=0;d<point_traits::num_dims;d++) 
            {
                point_traits::set_coord(lower,d,empty_box_upper<scalar_t>());
                point_traits::set_coord(upper,d,empty_box_lower<scalar_t>());
            }
        }
        
        point_t lower, upper;
    };


    template<typename T>
    inline std::ostream &operator<<(std::ostream &o, const box_t<T> &b)
    {
    o << "{" << b.lower << "," << b.upper << "}";
    return o;
    }


    template<typename point_t>
    inline point_t project(const box_t<point_t> &box, const point_t &point)
    {
        return min(max(point,box.lower),box.upper);
    }

    template<typename point_t>
    inline auto sqrDistance(const box_t<point_t> &box, const point_t &point)
    { 
        return sqrDistance(project(box,point),point); 
    }


}


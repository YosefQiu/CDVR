#pragma once
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <tuple>

#include "helper.hpp"
#include "common.hpp"
#include "box.hpp"

namespace kdTree 
{
    template<typename data_t, typename data_traits>
    inline void host_computeBounds(box_t<typename data_traits::point_t> *d_bounds, const data_t *d_points, int numPoints)
    {
        d_bounds->setEmpty();
        for (int i=0;i<numPoints;i++)
            d_bounds->grow(data_traits::get_point(d_points[i]));
    }

    template<typename data_t,typename data_traits>
    inline box_t<typename data_traits::point_t> findBounds(int subtree, const box_t<typename data_traits::point_t> *d_bounds, data_t *d_nodes)
    {
        using point_t  = typename data_traits::point_t;
        using point_traits = kdTree::point_traits<point_t>;
        using scalar_t = typename point_traits::scalar_t;
        enum { num_dims = point_traits::num_dims };
        
        box_t<typename data_traits::point_t> bounds = *d_bounds;
        int curr = subtree;
        while (curr > 0) 
        {
            const int     parent = (curr+1)/2-1;
            const data_t &parent_node = d_nodes[parent];
            const int     parent_dim
            = if_has_dims<data_t,data_traits,data_traits::has_explicit_dim>
            ::get_dim(parent_node, BinaryTree::levelOf(parent) % num_dims);
            const scalar_t parent_split_pos = data_traits::get_coord(parent_node,parent_dim);
            
            if (curr & 1) 
            {
                // curr is left child, set upper
                point_traits::set_coord(bounds.upper,parent_dim, std::min(parent_split_pos, get_coord(bounds.upper,parent_dim)));
            } 
            else 
            {
                // curr is right child, set lower
                point_traits::set_coord(bounds.lower,parent_dim, std::max(parent_split_pos, get_coord(bounds.lower,parent_dim)));
            }
            curr = parent;
        }
        
        return bounds;
    }

    template<typename data_t, typename data_traits>
    struct ZipCompare 
    {
        explicit ZipCompare(const int dim, const data_t *nodes): dim(dim), nodes(nodes) {}
    
        inline bool operator() (const std::tuple<uint32_t, data_t> &a, const std::tuple<uint32_t, data_t> &b);

        const int dim;
        const data_t *nodes;
    };

    template<typename data_t,typename data_traits>
    void host_chooseInitialDim(box_t<typename data_traits::point_t> *d_bounds, data_t *d_nodes, int numPoints)
    {
        for (int tid=0;tid<numPoints;tid++) 
        {
            int dim = d_bounds->widestDimension();//arg_max(d_bounds->size());
            if_has_dims<data_t,data_traits,data_traits::has_explicit_dim>
            ::set_dim(d_nodes[tid],dim);
        }
    }

    inline void updateTag(int gid, uint32_t *tag, int numPoints, int L)
    {
        const int numSettled = FullBinaryTreeOf(L).numNodes();
        if (gid < numSettled) return;
        int subtree = tag[gid];
        const int pivotPos = ArrayLayoutInStep(L,numPoints).pivotPosOf(subtree);

        if (gid < pivotPos)
            subtree = BinaryTree::leftChildOf(subtree);
        else if (gid > pivotPos)
            subtree = BinaryTree::rightChildOf(subtree);
        else
            ;
        tag[gid] = subtree;
    }


    inline void host_updateTags(uint32_t *tag, int numPoints, int L)
    {
        for (int gid=0;gid<numPoints;gid++) 
            updateTag(gid,tag,numPoints,L);
    }


    template<typename data_t, typename data_traits>
    inline void updateTagAndSetDim(int gid, const box_t<typename data_traits::point_t> *d_bounds, uint32_t  *tag, data_t *d_nodes, int numPoints, int L)
    {
        using point_t      = typename data_traits::point_t;
        using point_traits = typename kdTree::point_traits<point_t>;
        using scalar_t     = typename point_traits::scalar_t;
        
        const int numSettled = FullBinaryTreeOf(L).numNodes();
        if (gid < numSettled) return;

        int subtree = tag[gid];
        box_t<typename data_traits::point_t> bounds = findBounds<data_t,data_traits>(subtree,d_bounds,d_nodes);
        const int pivotPos = ArrayLayoutInStep(L,numPoints).pivotPosOf(subtree);

        const int pivotDim = if_has_dims<data_t,data_traits,data_traits::has_explicit_dim>::get_dim(d_nodes[pivotPos],-1);
        const scalar_t pivotCoord = data_traits::get_coord(d_nodes[pivotPos],pivotDim);

        if (gid < pivotPos) 
        {
            subtree = BinaryTree::leftChildOf(subtree);
            point_traits::set_coord(bounds.upper,pivotDim,pivotCoord);
        } 
        else if (gid > pivotPos) 
        {
            subtree = BinaryTree::rightChildOf(subtree);
            point_traits::set_coord(bounds.lower,pivotDim,pivotCoord);
        } 
        else
            // point is _on_ the pivot position -> it's the root of that
            // subtree, don't change it.
            ;
        if (gid != pivotPos) 
        {
            if_has_dims<data_t,data_traits,data_traits::has_explicit_dim>
            ::set_dim(d_nodes[gid],bounds.widestDimension());
        }
        tag[gid] = subtree;
    }

    template<typename data_t, typename data_traits>
    void host_updateTagsAndSetDims(const box_t<typename data_traits::point_t> *d_bounds, uint32_t  *tag, data_t *d_nodes, int numPoints, int L)
    {
        for (int gid=0;gid<numPoints;gid++) 
            updateTagAndSetDim<data_t,data_traits> (gid, d_bounds, tag, d_nodes, numPoints, L);
    }

    template<typename data_t, typename data_traits>
    inline bool ZipCompare<data_t,data_traits>::operator() (const std::tuple<uint32_t, data_t> &a, const std::tuple<uint32_t, data_t> &b)
    {

        const auto tag_a = std::get<0>(a);
        const auto tag_b = std::get<0>(b);
        const auto pnt_a = std::get<1>(a);
        const auto pnt_b = std::get<1>(b);
        int dim
            = if_has_dims<data_t,data_traits,data_traits::has_explicit_dim>
            :: get_dim(pnt_a,/* if not: */this->dim);
        const auto coord_a = data_traits::get_coord(pnt_a,dim);
        const auto coord_b = data_traits::get_coord(pnt_b,dim);
        const bool less =
            (tag_a < tag_b)
            ||
            ((tag_a == tag_b) && (coord_a < coord_b));

        return less;
    }

    template<typename data_t, typename data_traits>
    void buildTree_host(data_t *d_points, int numPoints, box_t<typename data_traits::point_t> *worldBounds)
    {
        using point_t      = typename data_traits::point_t;
        using point_traits = kdTree::point_traits<point_t>;
        enum { num_dims   = point_traits::num_dims };
        
        // check for invalid input, and return gracefully if so
        if (numPoints < 1) return;

        std::vector<uint32_t> tags(numPoints);
        std::fill(tags.begin(),tags.end(),0);

        const int numLevels = BinaryTree::numLevelsFor(numPoints);
        const int deepestLevel = numLevels-1;
        
        if (worldBounds) 
        {
            host_computeBounds<data_t,data_traits>
            (worldBounds,d_points,numPoints);
        }
        if (data_traits::has_explicit_dim) 
        {
            if (!worldBounds)
            throw std::runtime_error
                ("builder_host: asked to build k-d tree over nodes"
                " with explicit dims, but no memory for world bounds provided");
            
            host_chooseInitialDim<data_t,data_traits>(worldBounds,d_points,numPoints);
        }
        
        /* now build each level, one after another, cycling through the
            dimensions */
        for (int level=0;level<deepestLevel;level++) 
        {
            // Create zip data for sorting
            std::vector<std::tuple<uint32_t, data_t>> zip_data(numPoints);
            for (int i = 0; i < numPoints; ++i) 
            {
                zip_data[i] = std::make_tuple(tags[i], d_points[i]);
            }
            
            std::sort(zip_data.begin(), zip_data.end(),
                    ZipCompare<data_t,data_traits>
                    ((level)%num_dims,d_points));
            
            // Extract sorted data back
            for (int i = 0; i < numPoints; ++i) 
            {
                tags[i] = std::get<0>(zip_data[i]);
                d_points[i] = std::get<1>(zip_data[i]);
            }
            
            if (data_traits::has_explicit_dim) 
            {
                host_updateTagsAndSetDims<data_t,data_traits>(worldBounds,tags.data(), d_points,numPoints,level);
            } 
            else 
            {
                host_updateTags(tags.data(),numPoints,level);
            }
        }
        
        std::vector<std::tuple<uint32_t, data_t>> zip_data(numPoints);
        for (int i = 0; i < numPoints; ++i) 
        {
            zip_data[i] = std::make_tuple(tags[i], d_points[i]);
        }
        
        std::sort(zip_data.begin(), zip_data.end(),
                ZipCompare<data_t,data_traits>
                ((deepestLevel)%num_dims,d_points));
        
        for (int i = 0; i < numPoints; ++i) 
        {
            d_points[i] = std::get<1>(zip_data[i]);
        }
    }
}


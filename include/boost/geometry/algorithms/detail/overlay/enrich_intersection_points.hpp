// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP

#include <cstddef>
#include <algorithm>
#include <map>
#include <set>
#include <vector>

#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
#  include <iostream>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#  include <boost/geometry/io/wkt/wkt.hpp>
#  define BOOST_GEOMETRY_DEBUG_IDENTIFIER
#endif

#include <boost/range.hpp>

#include <boost/geometry/iterators/ever_circling_iterator.hpp>
#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/copy_segment_point.hpp>
#include <boost/geometry/algorithms/detail/overlay/handle_colocations.hpp>
#include <boost/geometry/algorithms/detail/overlay/less_by_segment_ratio.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>
#include <boost/geometry/policies/robustness/robust_type.hpp>
#include <boost/geometry/strategies/side.hpp>
#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
#  include <boost/geometry/algorithms/detail/overlay/check_enrich.hpp>
#endif


namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{


// Sorts IP-s of this ring on segment-identifier, and if on same segment,
//  on distance.
// Then assigns for each IP which is the next IP on this segment,
// plus the vertex-index to travel to, plus the next IP
// (might be on another segment)
template
<
    bool Reverse1, bool Reverse2,
    typename Operations,
    typename Turns,
    typename Geometry1, typename Geometry2,
    typename RobustPolicy,
    typename Strategy
>
inline void enrich_sort(Operations& operations,
            Turns const& turns,
            operation_type for_operation,
            Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            RobustPolicy const& robust_policy,
            Strategy const& strategy)
{
    std::sort(boost::begin(operations),
            boost::end(operations),
            less_by_segment_ratio
                <
                    Turns,
                    typename boost::range_value<Operations>::type,
                    Geometry1, Geometry2,
                    RobustPolicy,
                    Reverse1, Reverse2
                >(turns, for_operation, geometry1, geometry2, robust_policy));
}


template <typename Operations, typename Turns>
inline void enrich_assign(Operations& operations, Turns& turns)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type op_type;
    typedef typename boost::range_iterator<Operations>::type iterator_type;


    if (operations.size() > 0)
    {
        // Assign travel-to-vertex/ip index for each turning point.
        // Iterator "next" is circular

        geometry::ever_circling_range_iterator<Operations const> next(operations);
        ++next;

        for (iterator_type it = boost::begin(operations);
             it != boost::end(operations); ++it)
        {
            turn_type& turn = turns[it->turn_index];
            op_type& op = turn.operations[it->operation_index];

            // Normal behaviour: next should point at next turn:
            if (it->turn_index == next->turn_index)
            {
                ++next;
            }

            // Cluster behaviour: next should point after cluster, unless
            // their seg_ids are not the same
            while (turn.cluster_id != -1
                   && it->turn_index != next->turn_index
                   && turn.cluster_id == turns[next->turn_index].cluster_id
                   && op.seg_id == turns[next->turn_index].operations[next->operation_index].seg_id)
            {
                ++next;
            }

            turn_type const& next_turn = turns[next->turn_index];
            op_type const& next_op = next_turn.operations[next->operation_index];

            op.enriched.travels_to_ip_index
                    = static_cast<signed_size_type>(next->turn_index);
            op.enriched.travels_to_vertex_index
                    = next->subject->seg_id.segment_index;

            if (op.seg_id.segment_index == next_op.seg_id.segment_index
                    && op.fraction < next_op.fraction)
            {
                // Next turn is located further on same segment
                // assign next_ip_index
                // (this is one not circular therefore fraction is considered)
                op.enriched.next_ip_index = static_cast<signed_size_type>(next->turn_index);
            }
        }
    }

    // DEBUG
#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    {
        for (iterator_type it = boost::begin(operations);
             it != boost::end(operations);
             ++it)
        {
            op_type& op = turns[it->turn_index]
                .operations[it->operation_index];

            std::cout << it->turn_index
                << " cl=" << turns[it->turn_index].cluster_id
                << " meth=" << method_char(turns[it->turn_index].method)
                << " seg=" << op.seg_id
                << " dst=" << op.fraction // needs define
                << " op=" << operation_char(turns[it->turn_index].operations[0].operation)
                << operation_char(turns[it->turn_index].operations[1].operation)
                << " (" << operation_char(op.operation) << ")"
                << " nxt=" << op.enriched.next_ip_index
                << " / " << op.enriched.travels_to_ip_index
                << " [vx " << op.enriched.travels_to_vertex_index << "]"
                << std::endl;
                ;
        }
    }
#endif
    // END DEBUG

}


template <typename Turns, typename MappedVector>
inline void create_map(Turns const& turns,
                operation_type for_operation,
                MappedVector& mapped_vector)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::container_type container_type;
    typedef typename MappedVector::mapped_type mapped_type;
    typedef typename boost::range_value<mapped_type>::type indexed_type;

    std::size_t index = 0;
    for (typename boost::range_iterator<Turns const>::type
            it = boost::begin(turns);
         it != boost::end(turns);
         ++it, ++index)
    {
        // Add operations on this ring, but skip discarded and non relevant
        turn_type const& turn = *it;
        if (turn.discarded)
        {
            continue;
        }
        if (for_operation != operation_union
            && turn.has(operation_blocked)
            && ! turn.has(for_operation))
        {
            // Don't include ux for intersection/difference
            // Currently it is still necessary to include ix for union
            continue;
        }

        std::size_t op_index = 0;
        for (typename boost::range_iterator<container_type const>::type
                op_it = boost::begin(turn.operations);
            op_it != boost::end(turn.operations);
            ++op_it, ++op_index)
        {
            // We should NOT skip blocked operations here
            // because they can be relevant for "the other side"
            // NOT if (op_it->operation != operation_blocked)
            ring_identifier const ring_id
                (
                    op_it->seg_id.source_index,
                    op_it->seg_id.multi_index,
                    op_it->seg_id.ring_index
                );
            mapped_vector[ring_id].push_back
                (
                    indexed_type(index, op_index, *op_it,
                        it->operations[1 - op_index].seg_id)
                );
        }
    }
}


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL



/*!
\brief All intersection points are enriched with successor information
\ingroup overlay
\tparam Turns type of intersection container
            (e.g. vector of "intersection/turn point"'s)
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\tparam Strategy side strategy type
\param turns container containing intersectionpoints
\param for_operation operation_type (union or intersection)
\param geometry1 \param_geometry
\param geometry2 \param_geometry
\param robust_policy policy to handle robustness issues
\param strategy strategy
 */
template
<
    bool Reverse1, bool Reverse2,
    overlay_type OverlayType,
    typename Turns,
    typename Clusters,
    typename Geometry1, typename Geometry2,
    typename RobustPolicy,
    typename Strategy
>
inline void enrich_intersection_points(Turns& turns,
    Clusters& clusters,
    detail::overlay::operation_type for_operation,
    Geometry1 const& geometry1, Geometry2 const& geometry2,
    RobustPolicy const& robust_policy,
    Strategy const& strategy)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type op_type;
    typedef detail::overlay::indexed_turn_operation
        <
            op_type
        > indexed_turn_operation;

    typedef std::map
        <
            ring_identifier,
            std::vector<indexed_turn_operation>
        > mapped_vector_type;

    // Holds counts of discarded and colocated cc turns, per ring
    typedef std::map
        <
            ring_identifier,
            std::size_t
        > count_per_ring_type;

    count_per_ring_type colocated_cc_map;

    bool const has_colocations
        = detail::overlay::handle_colocations<Reverse1, Reverse2>(turns,
        clusters, colocated_cc_map, for_operation, geometry1, geometry2);

    // Discard none turns, if any
    for (typename boost::range_iterator<Turns>::type
            it = boost::begin(turns);
         it != boost::end(turns);
         ++it)
    {
        if (it->both(detail::overlay::operation_none))
        {
            it->discarded = true;
        }
    }

    // Create a map of vectors of indexed operation-types to be able
    // to sort intersection points PER RING
    mapped_vector_type mapped_vector;

    detail::overlay::create_map(turns, for_operation, mapped_vector);

    if (has_colocations)
    {
        for (typename mapped_vector_type::iterator mit
            = mapped_vector.begin();
            mit != mapped_vector.end();
            ++mit)
        {
            detail::overlay::discard_lonely_uu_turns(mit->second, turns,
                mapped_vector, colocated_cc_map);
        }
    }


    // No const-iterator; contents of mapped copy is temporary,
    // and changed by enrich
    for (typename mapped_vector_type::iterator mit
        = mapped_vector.begin();
        mit != mapped_vector.end();
        ++mit)
    {
#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    std::cout << "ENRICH-sort Ring "
        << mit->first << std::endl;
#endif
        detail::overlay::enrich_sort<Reverse1, Reverse2>(
                    mit->second, turns, for_operation,
                    geometry1, geometry2,
                    robust_policy, strategy);
    }

    for (typename mapped_vector_type::iterator mit
        = mapped_vector.begin();
        mit != mapped_vector.end();
        ++mit)
    {
#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    std::cout << "ENRICH-assign Ring "
        << mit->first << std::endl;
#endif
        detail::overlay::enrich_assign(mit->second, turns);
    }

#ifdef BOOST_GEOMETRY_DEBUG_ENRICH
    //detail::overlay::check_graph(turns, for_operation);
#endif

}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_ENRICH_HPP

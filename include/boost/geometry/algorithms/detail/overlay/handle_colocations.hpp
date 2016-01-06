// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP

#include <cstddef>
#include <algorithm>
#include <map>
#include <vector>

#include <boost/range.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>
#include <boost/geometry/algorithms/detail/overlay/sort_by_side.hpp>
#include <boost/geometry/algorithms/detail/overlay/turn_info.hpp>
#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/segment_identifier.hpp>

#if defined(BOOST_GEOMETRY_DEBUG_HANDLE_COLOCATIONS)
#  include <iostream>
#  include <boost/geometry/algorithms/detail/overlay/debug_turn_info.hpp>
#  include <boost/geometry/io/wkt/wkt.hpp>
#  define BOOST_GEOMETRY_DEBUG_IDENTIFIER
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

template <typename SegmentRatio>
struct segment_fraction
{
    segment_identifier seg_id;
    SegmentRatio fraction;

    segment_fraction(segment_identifier const& id, SegmentRatio const& fr)
        : seg_id(id)
        , fraction(fr)
    {}

    segment_fraction()
    {}

    bool operator<(segment_fraction<SegmentRatio> const& other) const
    {
        return seg_id == other.seg_id
                ? fraction < other.fraction
                : seg_id < other.seg_id;
    }

};

struct turn_operation_index
{
    turn_operation_index(signed_size_type ti = -1,
                         signed_size_type oi = -1)
        : turn_index(ti)
        , op_index(oi)
    {}

    signed_size_type turn_index;
    signed_size_type op_index; // only 0,1
};


template <typename Turns>
struct less_by_fraction_and_type
{
    inline less_by_fraction_and_type(Turns const& turns)
        : m_turns(turns)
    {
    }

    inline bool operator()(turn_operation_index const& left,
                           turn_operation_index const& right) const
    {
        typedef typename boost::range_value<Turns>::type turn_type;
        typedef typename turn_type::turn_operation_type turn_operation_type;

        turn_type const& left_turn = m_turns[left.turn_index];
        turn_type const& right_turn = m_turns[right.turn_index];
        turn_operation_type const& left_op
                = left_turn.operations[left.op_index];

        turn_operation_type const& right_op
                = right_turn.operations[right.op_index];

        if (! (left_op.fraction == right_op.fraction))
        {
            return left_op.fraction < right_op.fraction;
        }

        // Order xx first - used to discard any following colocated turn
        bool const left_both_xx = left_turn.both(operation_blocked);
        bool const right_both_xx = right_turn.both(operation_blocked);
        if (left_both_xx && ! right_both_xx)
        {
            return true;
        }
        if (! left_both_xx && right_both_xx)
        {
            return false;
        }

        bool const left_both_uu = left_turn.both(operation_union);
        bool const right_both_uu = right_turn.both(operation_union);
        if (left_both_uu && ! right_both_uu)
        {
            return true;
        }
        if (! left_both_uu && right_both_uu)
        {
            return false;
        }

        turn_operation_type const& left_other_op
                = left_turn.operations[1 - left.op_index];

        turn_operation_type const& right_other_op
                = right_turn.operations[1 - right.op_index];

        // Fraction is the same, now sort on ring id, first outer ring,
        // then interior rings
        return left_other_op.seg_id < right_other_op.seg_id;
    }

private:
    Turns const& m_turns;
};

template <typename Operation, typename ClusterPerSegment>
inline signed_size_type get_cluster_id(Operation const& op, ClusterPerSegment const& cluster_per_segment)
{
    typedef typename ClusterPerSegment::key_type segment_fraction_type;

    segment_fraction_type seg_frac(op.seg_id, op.fraction);
    typename ClusterPerSegment::const_iterator it
            = cluster_per_segment.find(seg_frac);

    if (it == cluster_per_segment.end())
    {
        return -1;
    }
    return it->second;
}

template <typename Operation, typename ClusterPerSegment>
inline void add_cluster_id(Operation const& op,
    ClusterPerSegment& cluster_per_segment, signed_size_type id)
{
    typedef typename ClusterPerSegment::key_type segment_fraction_type;

    segment_fraction_type seg_frac(op.seg_id, op.fraction);

    cluster_per_segment[seg_frac] = id;
}

template <typename Turn, typename ClusterPerSegment>
inline signed_size_type add_turn_to_cluster(Turn const& turn,
        ClusterPerSegment& cluster_per_segment, signed_size_type& cluster_id)
{
    signed_size_type cid0 = get_cluster_id(turn.operations[0], cluster_per_segment);
    signed_size_type cid1 = get_cluster_id(turn.operations[1], cluster_per_segment);

    if (cid0 == -1 && cid1 == -1)
    {
        ++cluster_id;
        add_cluster_id(turn.operations[0], cluster_per_segment, cluster_id);
        add_cluster_id(turn.operations[1], cluster_per_segment, cluster_id);
        return cluster_id;
    }
    else if (cid0 == -1 && cid1 != -1)
    {
        add_cluster_id(turn.operations[0], cluster_per_segment, cid1);
        return cid1;
    }
    else if (cid0 != -1 && cid1 == -1)
    {
        add_cluster_id(turn.operations[1], cluster_per_segment, cid0);
        return cid0;
    }
    else if (cid0 == cid1)
    {
        // Both already added to same cluster, no action
        return cid0;
    }

    // Both operations.seg_id/fraction were already part of any cluster, and
    // these clusters are not the same. Merge of two clusters is necessary
    std::cout << " TODO: merge " << cid0 << " and " << cid1 << std::endl;
    return cid0;
}

/// Discards turn colocated with a uu turn, where both turns are on the same
/// rings and one is (most probably) invalid
template
<
    bool Reverse1, bool Reverse2,
    typename Turns,
    typename Geometry1,
    typename Geometry2
>
inline bool discard_colocated_uu(signed_size_type cluster_id,
        Turns const& turns,
        turn_operation_index const& ref_toi,
        turn_operation_index const& toi,
        Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename geometry::point_type<Geometry1>::type point_type;

    // Order counter clockwise to get the most right turn
    typedef sort_by_side::side_sorter
        <
            Reverse1, Reverse2, point_type, std::less<int>
        > sbs_type;

    sbs_type sbs;

    turn_type const& ref_turn = turns[ref_toi.turn_index];
    turn_type const& turn = turns[toi.turn_index];

    // Add operations, the first is the "subject" (so sorting done from there)
    BOOST_ASSERT(ref_turn.both(operation_union));
    sbs.add(ref_turn.operations[ref_toi.op_index],
            ref_toi.turn_index, ref_toi.op_index,
            geometry1, geometry2, true);
    sbs.add(ref_turn.operations[1 - ref_toi.op_index],
            ref_toi.turn_index, 1 - ref_toi.op_index,
            geometry1, geometry2, false);

    // Skip toi.op_index because ref_toi and toi have the same operation
    BOOST_ASSERT(ref_turn.operations[ref_toi.op_index].seg_id
            == turn.operations[toi.op_index].seg_id);

    sbs.add(turn.operations[1 - toi.op_index],
            toi.turn_index, 1 - toi.op_index,
            geometry1, geometry2, false);

    sbs.apply(ref_turn.point);

    // Inspect points, the first right turn has main_rank==1
    // If this is still from the same source, with no others in between, it
    // should be discarded
    signed_size_type const source_index
            = ref_turn.operations[ref_toi.op_index].seg_id.source_index;
    for (std::size_t i = 0; i < sbs.m_ranked_points.size(); i++)
    {
        const typename sbs_type::rp& ranked_point = sbs.m_ranked_points[i];
        if (ranked_point.main_rank > 1)
        {
            break;
        }
        if (ranked_point.seg_id.source_index != source_index)
        {
            // Other sources in between or collinear with it, don't discard
            return false;
        }
    }

    // Discard this turn
    return true;
}

template
<
    bool Reverse1, bool Reverse2,
    typename Turns,
    typename ClusterPerSegment,
    typename ColocatedCcMap,
    typename Operations,
    typename Geometry1,
    typename Geometry2
>
inline void handle_colocation_cluster(Turns& turns,
        signed_size_type& cluster_id,
        ClusterPerSegment& cluster_per_segment,
        ColocatedCcMap& colocated_cc_map,
        Operations const& operations,
        operation_type for_operation,
        Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type turn_operation_type;
    typedef typename ClusterPerSegment::key_type segment_fraction_type;

    std::vector<turn_operation_index>::const_iterator vit = operations.begin();

    turn_operation_index ref_toi = *vit;
    signed_size_type ref_id = -1;

    for (++vit; vit != operations.end(); ++vit)
    {
        turn_type& ref_turn = turns[ref_toi.turn_index];
        turn_operation_type const& ref_op
                = ref_turn.operations[ref_toi.op_index];

        turn_operation_index const& toi = *vit;
        turn_type& turn = turns[toi.turn_index];
        turn_operation_type const& op = turn.operations[toi.op_index];

        BOOST_ASSERT(ref_op.seg_id == op.seg_id);

        if (ref_op.fraction == op.fraction)
        {
            turn_operation_type const& ref_other_op
                    = ref_turn.operations[1 - ref_toi.op_index];
            turn_operation_type const& other_op = turn.operations[1 - toi.op_index];

            if (ref_id == -1)
            {
                ref_id = add_turn_to_cluster(ref_turn, cluster_per_segment, cluster_id);
            }
            BOOST_ASSERT(ref_id != -1);

            // ref_turn (both operations) are already added to cluster,
            // so also "op" is already added to cluster,
            // We only need to add other_op
            signed_size_type id = get_cluster_id(other_op, cluster_per_segment);
            if (id != -1 && id != ref_id)
            {
            }
            else if (id == -1)
            {
                // Add to same cluster
                add_cluster_id(other_op, cluster_per_segment, ref_id);
                id = ref_id;
            }

            // In case of colocated xx turns, all other turns may NOT be
            // followed at all. xx cannot be discarded (otherwise colocated
            // turns are followed).
            if (ref_turn.both(operation_blocked))
            {
                turn.discarded = true;
                // We can either set or not set colocated because it is not effective on blocked turns
            }

            if (for_operation == operation_union
                && ref_turn.both(operation_union)
                && ! turn.both(operation_union))
            {
                if (other_op.seg_id.multi_index == ref_other_op.seg_id.multi_index
                    && other_op.seg_id.ring_index == ref_other_op.seg_id.ring_index
                    && discard_colocated_uu<Reverse1, Reverse2>(id,
                        turns, ref_toi, toi, geometry1, geometry2))
                {
                    turn.discarded = true;
                    turn.colocated = true;
                }
                if (turn.both(operation_continue))
                {
                    turn.discarded = true;
                    turn.colocated = true;

                    // Register this to find 'lonely colocated uu turns'
                    colocated_cc_map
                        [
                           ring_identifier(op.seg_id.source_index,
                               op.seg_id.multi_index,
                               op.seg_id.ring_index)
                        ]++;
                    colocated_cc_map
                        [
                           ring_identifier(other_op.seg_id.source_index,
                               other_op.seg_id.multi_index,
                               other_op.seg_id.ring_index)
                        ]++;
                }
            }
        }
        else
        {
            // Not on same fraction on this segment
            // assign for next
            ref_toi = toi;
            ref_id = -1;
        }
    }
}

template
<
    typename Turns,
    typename Clusters,
    typename ClusterPerSegment
>
inline void assign_cluster_to_turns(Turns& turns,
        Clusters& clusters,
        ClusterPerSegment const& cluster_per_segment)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type turn_operation_type;
    typedef typename ClusterPerSegment::key_type segment_fraction_type;

    signed_size_type turn_index = 0;
    for (typename boost::range_iterator<Turns>::type it = turns.begin();
         it != turns.end(); ++it, ++turn_index)
    {
        turn_type& turn = *it;

        if (turn.discarded)
        {
            // They were processed (to create proper map) but will not be added
            // This might leave a cluster with only 1 turn, which will be fixed
            // afterwards
            continue;
        }

        for (int i = 0; i < 2; i++)
        {
            turn_operation_type const& op = turn.operations[i];
            segment_fraction_type seg_frac(op.seg_id, op.fraction);
            typename ClusterPerSegment::const_iterator it = cluster_per_segment.find(seg_frac);
            if (it != cluster_per_segment.end())
            {
                if (turn.cluster_id != -1
                        && turn.cluster_id != it->second)
                {
                    std::cout << " CONFLICT " << std::endl;
                }
                turn.cluster_id = it->second;
                clusters[turn.cluster_id].insert(turn_index);
            }
        }
    }
}

template
<
    typename Turns,
    typename Clusters
>
inline void remove_clusters(Turns& turns, Clusters& clusters)
{
    typename Clusters::iterator it = clusters.begin();
    while (it != clusters.end())
    {
        // Hold iterator and increase. We can erase cit, this keeps the
        // iterator valid (cf The standard associative-container erase idiom)
        typename Clusters::iterator current_it = it;
        ++it;

        std::set<signed_size_type> const& turn_indices = current_it->second;
        if (turn_indices.size() == 1)
        {
            signed_size_type turn_index = *turn_indices.begin();
            turns[turn_index].cluster_id = -1;
            clusters.erase(current_it);
        }
    }
}


// Checks colocated turns and flags combinations of uu/other, possibly a
// combination of a ring touching another geometry's interior ring which is
// tangential to the exterior ring

// This function can be extended to replace handle_tangencies: at each
// colocation incoming and outgoing vectors should be inspected

template
<
    bool Reverse1, bool Reverse2,
    typename Turns,
    typename Clusters,
    typename ColocatedCcMap,
    typename Geometry1,
    typename Geometry2
>
inline bool handle_colocations(Turns& turns, Clusters& clusters,
        ColocatedCcMap& colocated_cc_map, operation_type for_operation,
        Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    typedef std::map
        <
            segment_identifier,
            std::vector<turn_operation_index>
        > map_type;

    // Create and fill map on segment-identifier Map is sorted on seg_id,
    // meaning it is sorted on ring_identifier too. This means that exterior
    // rings are handled first. If there is a colocation on the exterior ring,
    // that information can be used for the interior ring too
    map_type map;

    int index = 0;
    for (typename boost::range_iterator<Turns>::type
            it = boost::begin(turns);
         it != boost::end(turns);
         ++it, ++index)
    {
        map[it->operations[0].seg_id].push_back(turn_operation_index(index, 0));
        map[it->operations[1].seg_id].push_back(turn_operation_index(index, 1));
    }

    // Check if there are multiple turns on one or more segments,
    // if not then nothing is to be done
    bool colocations = 0;
    for (typename map_type::const_iterator it = map.begin();
         it != map.end();
         ++it)
    {
        if (it->second.size() > 1u)
        {
            colocations = true;
            break;
        }
    }

    if (! colocations)
    {
        return false;
    }

    // Sort all vectors, per same segment
    less_by_fraction_and_type<Turns> less(turns);
    for (typename map_type::iterator it = map.begin();
         it != map.end(); ++it)
    {
        std::sort(it->second.begin(), it->second.end(), less);
    }

    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::segment_ratio_type segment_ratio_type;

    typedef std::map
        <
            segment_fraction<segment_ratio_type>,
            signed_size_type
        > cluster_per_segment_type;

    cluster_per_segment_type cluster_per_segment;
    signed_size_type cluster_id = 0;

    for (typename map_type::const_iterator it = map.begin();
         it != map.end(); ++it)
    {
        if (it->second.size() > 1u)
        {
            handle_colocation_cluster<Reverse1, Reverse2>(turns, cluster_id,
                cluster_per_segment, colocated_cc_map, it->second,
                for_operation, geometry1, geometry2);
        }
    }

    assign_cluster_to_turns(turns, clusters, cluster_per_segment);
    remove_clusters(turns, clusters);

#if defined(BOOST_GEOMETRY_DEBUG_HANDLE_COLOCATIONS)
    std::cout << "*** Colocations " << map.size() << std::endl;
    for (typename map_type::const_iterator it = map.begin();
         it != map.end(); ++it)
    {
        std::cout << it->first << std::endl;
        for (std::vector<turn_operation_index>::const_iterator vit
             = it->second.begin(); vit != it->second.end(); ++vit)
        {
            turn_operation_index const& toi = *vit;
            std::cout << geometry::wkt(turns[toi.turn_index].point)
                << std::boolalpha
                << " discarded=" << turns[toi.turn_index].discarded
                << " colocated=" << turns[toi.turn_index].colocated
                << " " << operation_char(turns[toi.turn_index].operations[0].operation)
                << " "  << turns[toi.turn_index].operations[0].seg_id
                << " "  << turns[toi.turn_index].operations[0].fraction
                << " // " << operation_char(turns[toi.turn_index].operations[1].operation)
                << " "  << turns[toi.turn_index].operations[1].seg_id
                << " "  << turns[toi.turn_index].operations[1].fraction
                << std::endl;
        }
    }
#endif // DEBUG

    return true;
}


struct is_turn_index
{
    is_turn_index(signed_size_type index)
        : m_index(index)
    {}

    template <typename Indexed>
    inline bool operator()(Indexed const& indexed) const
    {
        // Indexed is a indexed_turn_operation<Operation>
        return indexed.turn_index == m_index;
    }

    std::size_t m_index;
};


template
<
    typename Operations,
    typename Turns,
    typename Map,
    typename ColocatedCcMap
>
inline void discard_lonely_uu_turns(Operations& operations, Turns& turns,
                Map& map, ColocatedCcMap const& colocated_cc_map)
{
    typedef typename boost::range_value<Turns>::type turn_type;
    typedef typename turn_type::turn_operation_type op_type;

    if (operations.size() != 1)
    {
        return;
    }

    signed_size_type turn_index = operations.front().turn_index;

    turn_type& turn = turns[turn_index];
    if (! turn.both(operation_union))
    {
        return;
    }

    op_type const& op = turn.operations[operations.front().operation_index];
    ring_identifier const ring_id
        (
            op.seg_id.source_index,
            op.seg_id.multi_index,
            op.seg_id.ring_index
        );

    if (colocated_cc_map.find(ring_id) == colocated_cc_map.end())
    {
        return;
    }

    // This uu-turn, only active turn on this ring,
    // and has discarded colocated cc turns on the same ring too. Therefore it
    // should be discarded, otherwise it will be traveled twice

    turn.discarded = true;
    operations.clear();

    // Remove the turn from all other mapped items too
    is_turn_index const predicate(turn_index);
    for (typename Map::iterator it = map.begin(); it != map.end(); ++it)
    {
        Operations& ops = it->second;
        ops.erase
            (
                std::remove_if(boost::begin(ops), boost::end(ops), predicate),
                boost::end(ops)
            );
    }
}


}} // namespace detail::overlay
#endif //DOXYGEN_NO_DETAIL


}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_HANDLE_COLOCATIONS_HPP

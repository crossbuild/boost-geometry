// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2010-2015 Barend Gehrels, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <string>

// If defined, tests are run without rescaling-to-integer or robustness policy
// This multi_union currently contains no tests for double which then fail
// #define BOOST_GEOMETRY_NO_ROBUSTNESS

#include "test_union.hpp"
#include <algorithms/test_overlay.hpp>
#include <algorithms/overlay/multi_overlay_cases.hpp>

#include <boost/geometry/algorithms/correct.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/within.hpp>

#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/multi_linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>

#include <boost/geometry/io/wkt/read.hpp>


template <typename Ring, typename Polygon, typename MultiPolygon>
void test_areal()
{
    test_one<Polygon, MultiPolygon, MultiPolygon>("simplex_multi",
        case_multi_simplex[0], case_multi_simplex[1],
        1, 0, 20, 14.58);

    test_one<Polygon, Polygon, MultiPolygon>("simplex_multi_p_mp",
        case_single_simplex, case_multi_simplex[0],
        1, 0, 20, 14.58);
    test_one<Polygon, MultiPolygon, Polygon>("simplex_multi_mp_p",
        case_multi_simplex[0], case_single_simplex,
        1, 0, 20, 14.58);

    test_one<Polygon, Ring, MultiPolygon>("simplex_multi_r_mp",
        case_single_simplex, case_multi_simplex[0],
        1, 0, 20, 14.58);
    test_one<Ring, MultiPolygon, Polygon>("simplex_multi_mp_r",
        case_multi_simplex[0], case_single_simplex,
        1, 0, 20, 14.58);


    // Normal test cases
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_no_ip",
        case_multi_no_ip[0], case_multi_no_ip[1],
        4, 0, 16, 66.5);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_multi_2",
        case_multi_2[0], case_multi_2[1],
        3, 0, 16, 59.1);

    // Constructed cases for multi/touch/equal/etc
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_61_multi",
        case_61_multi[0], case_61_multi[1],
        1, 0, 11, 4.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_62_multi",
        case_62_multi[0], case_62_multi[1],
        2, 0, 10, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_63_multi",
        case_63_multi[0], case_63_multi[1],
        2, 0, 10, 2.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_64_multi",
        case_64_multi[0], case_64_multi[1],
        1, 0, 9, 3.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_65_multi",
        case_65_multi[0], case_65_multi[1],
        3, 0, 15, 4.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_66_multi",
        case_66_multi[0], case_66_multi[1],
        3, 0, 23, 7.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_72_multi",
        case_72_multi[0], case_72_multi[1],
        1, 0, 13, 10.65);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_75_multi",
        case_75_multi[0], case_75_multi[1],
        5, 0, 25, 5.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_76_multi",
        case_76_multi[0], case_76_multi[1],
        5, 0, 31, 8.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_89_multi",
        case_89_multi[0], case_89_multi[1],
        1, 0, 13, 6);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_101_multi",
        case_101_multi[0], case_101_multi[1],
        1, 0, 32, 22.25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_103_multi",
        case_103_multi[0], case_103_multi[1],
        1, 0, 7, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_104_multi",
        case_104_multi[0], case_104_multi[1],
        1, 0, 8, 25);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_105_multi",
        case_105_multi[0], case_105_multi[1],
        1, 0, 5, 25);

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_1",
        case_recursive_boxes_1[0], case_recursive_boxes_1[1],
        1, 1, 36, 97.0);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_2",
        case_recursive_boxes_2[0], case_recursive_boxes_2[1],
        1, 0, 14, 100.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_3",
        case_recursive_boxes_3[0], case_recursive_boxes_3[1],
        17, 0, 159, 56.5); // Area from SQL Server

    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_4",
        case_recursive_boxes_4[0], case_recursive_boxes_4[1],
        1, 1, 42, 96.75);
    test_one<Polygon, MultiPolygon, MultiPolygon>("case_recursive_boxes_5",
        case_recursive_boxes_5[0], case_recursive_boxes_5[1],
        3, 2, 110, 70.0);

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_a",
         ggl_list_20120915_h2[0], ggl_list_20120915_h2[1],
         1, 0, 12, 23.0); // Area from SQL Server
    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20120915_h2_b",
         ggl_list_20120915_h2[0], ggl_list_20120915_h2[2],
         1, 0, 12, 23.0); // Area from SQL Server

    test_one<Polygon, MultiPolygon, MultiPolygon>("ggl_list_20140212_sybren",
         ggl_list_20140212_sybren[0], ggl_list_20140212_sybren[1],
         2, 0, 16, 0.002471626);

    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_9081",
        ticket_9081[0], ticket_9081[1],
#if defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
        3,
#else
        4,
#endif
        0, 31, 0.2187385);

    test_one<Polygon, MultiPolygon, MultiPolygon>("ticket_10803",
        ticket_10803[0], ticket_10803[1],
        1, 0, 9, 2663736.07038);
}

// Test cases (generic)
template <typename Point, bool ClockWise, bool Closed>
void test_all()
{

    typedef bg::model::ring<Point, ClockWise, Closed> ring;
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;
    test_areal<ring, polygon, multi_polygon>();
}

// Test cases for integer coordinates / ccw / open
template <typename Point, bool ClockWise, bool Closed>
void test_specific()
{
    typedef bg::model::polygon<Point, ClockWise, Closed> polygon;
    typedef bg::model::multi_polygon<polygon> multi_polygon;

    ut_settings settings;
    settings.test_validity = true;

    test_one<polygon, multi_polygon, multi_polygon>("ticket_10803",
        ticket_10803[0], ticket_10803[1],
        1, 0, 9, 2664270, settings);
}


int test_main(int, char* [])
{
    test_all<bg::model::d2::point_xy<double>, true, true>();
    test_all<bg::model::d2::point_xy<double>, false, false>();

    test_specific<bg::model::d2::point_xy<int>, false, false>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)
    test_all<bg::model::d2::point_xy<float>, true, true>();

#if defined(HAVE_TTMATH)
    std::cout << "Testing TTMATH" << std::endl;
    test_all<bg::model::d2::point_xy<ttmath_big> >();
#endif

#endif

    return 0;
}


include(GoogleTest)

set(sources
  geometry_tests.cc
  point_tests.cc 
  quad_test.cc
)

add_executable(lf.geometry.test ${sources})
target_link_libraries(lf.geometry.test
  PUBLIC Eigen3::Eigen Boost::boost GTest::gtest_main
  lf.base
  lf.geometry
  lf.geometry.test_utils
  lf.io
  lf.quad
  lf.refinement
)

gtest_discover_tests(lf.geometry.test)

set(parametrization_breakdown_sources parametrization_breakdown.cc)
add_executable(
        examples.geometry.parametrization_breakdown ${parametrization_breakdown_sources}
)
target_link_libraries(examples.geometry.parametrization_breakdown PUBLIC
        Eigen3::Eigen
        Boost::filesystem Boost::system
        lf.geometry lf.quad lf.refinement
)
add_custom_target(examples.geometry.parametrization_breakdown_run COMMAND examples.geometry.parametrization_breakdown)
add_dependencies(examples_run examples.geometry.parametrization_breakdown_run)

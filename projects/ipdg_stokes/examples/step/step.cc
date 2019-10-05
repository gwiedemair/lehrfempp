/**
 * @file step.cc
 * @brief Solves the problem of computing the flow over a step
 */

#include <boost/filesystem.hpp>
#include <iostream>
#include <string>
#include <vector>

#include <lf/assemble/assemble.h>
#include <lf/assemble/coomatrix.h>
#include <lf/assemble/dofhandler.h>
#include <lf/base/base.h>
#include <lf/io/gmsh_reader.h>
#include <lf/io/io.h>
#include <lf/io/vtk_writer.h>
#include <lf/mesh/hybrid2d/mesh.h>
#include <lf/mesh/hybrid2d/mesh_factory.h>
#include <lf/quad/quad.h>
#include <lf/refinement/refinement.h>

#include <build_system_matrix.h>
#include <mesh_hierarchy_function.h>
#include <norms.h>
#include <piecewise_const_element_matrix_provider.h>
#include <piecewise_const_element_vector_provider.h>
#include <solution_to_mesh_data_set.h>

/**
 * @brief Concatenate objects defining an operator<<(std::ostream&)
 * @param args A variadic pack of objects implementing
 * `operator<<(std::ostream&)`
 * @returns A string with the objects concatenated
 */
template <typename... Args>
static std::string concat(Args &&... args) {
  std::ostringstream ss;
  (ss << ... << args);
  return ss.str();
}

/**
 * @brief Compute the analytic flow velocity of the poiseuille flow
 * @param h Half the distance between the two plates
 * @param flowrate The flowrate
 * @param y The y position at which to get the flow velocity. It must be in the
 * interval [-h, h]
 * @returns The flow velocity at point y
 */
double poiseuilleVelocity(double h, double flowrate, double y) {
  const double u_max = flowrate * 3 / 4 / h;
  return u_max * (1 - y * y / h / h);
}

/**
 * @brief Solves the PDE on a given mesh
 * @param mesh A shared pointer to the mesh on which to solve the PDE
 * @param dofh The dofhandler used for the simulation
 * @param flowrate The flowrate across the step
 * @param modified_penalty If true, use the modified penalty term, else use the
 * original one
 * @returns A vector of basis function coefficients for the solution of the PDE
 */
Eigen::VectorXd solveStep(const std::shared_ptr<const lf::mesh::Mesh> &mesh,
                          const lf::assemble::DofHandler &dofh, double flowrate,
                          bool modified_penalty) {
  // No volume forces are present
  auto f = [](const Eigen::Vector2d & /*unused*/) -> Eigen::Vector2d {
    return Eigen::Vector2d::Zero();
  };
  // Enforce a Poiseuille in- and outflow and no-slip boundary conditions at the
  // tube boundaries
  auto dirichlet_funct = [&](const lf::mesh::Entity &edge) -> Eigen::Vector2d {
    static constexpr double eps = 1e-10;
    const auto geom = edge.Geometry();
    const auto vertices = geom->Global(edge.RefEl().NodeCoords());
    const Eigen::Vector2d midpoint = vertices.rowwise().sum() / 2;
    Eigen::Vector2d v;
    if (vertices(0, 0) >= -3 - eps && vertices(0, 0) <= -3 + eps &&
        vertices(0, 1) >= -3 - eps && vertices(0, 1) <= -3 + eps) {
      // The edge is part of the inflow boundary
      v << poiseuilleVelocity(0.5, flowrate, midpoint[1] - 0.5), 0;
      return v;
    }
    if (vertices(0, 0) >= 3 - eps && vertices(0, 0) <= 3 + eps &&
        vertices(0, 1) >= 3 - eps && vertices(0, 1) <= 3 + eps) {
      // The edge is part of the outflow boundary
      v << poiseuilleVelocity(1, flowrate, midpoint[1]), 0;
      return v;
    }
    return Eigen::Vector2d::Zero();
  };

  // Solve the system using sparse LU
  const auto [A, rhs, offset_function] =
      projects::ipdg_stokes::assemble::buildSystemMatrixInOutFlow(
          mesh, dofh, f, dirichlet_funct, 1,
          lf::quad::make_TriaQR_MidpointRule(), modified_penalty);
  lf::io::VtkWriter writer(mesh, "offset_function.vtk");
  writer.WriteCellData("v",
                       projects::ipdg_stokes::post_processing::extractVelocity(
                           mesh, dofh, offset_function));
  writer.WritePointData(
      "c",
      projects::ipdg_stokes::post_processing::extractBasisFunctionCoefficients(
          mesh, dofh, offset_function));
  auto As = A.makeSparse();
  Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
  solver.compute(As);
  return solver.solve(rhs) + offset_function;
}

/**
 * @brief stores information to recover convergence properties
 */
struct ProblemSolution {
  std::shared_ptr<const lf::mesh::Mesh> mesh;
  std::shared_ptr<const lf::assemble::DofHandler> dofh;
  Eigen::SparseMatrix<double> A;
  Eigen::SparseMatrix<double> A_modified;
  Eigen::VectorXd rhs;
  Eigen::VectorXd solution;
  Eigen::VectorXd solution_modified;
};

/**
 * @brief Prints the L2 and DG norm errors to stdout
 */
int main() {
  const unsigned refinement_level = 6;
  const double flowrate = 1;

  // Read the mesh from the gmsh file
  boost::filesystem::path meshpath = __FILE__;
  meshpath = meshpath.parent_path() / "step.msh";
  std::unique_ptr<lf::mesh::MeshFactory> factory =
      std::make_unique<lf::mesh::hybrid2d::MeshFactory>(2);
  lf::io::GmshReader reader(std::move(factory), meshpath.string());
  auto mesh0 = reader.mesh();

  // Generate a mesh hierarchy by regular refinement
  const auto mesh_hierarchy =
      lf::refinement::GenerateMeshHierarchyByUniformRefinemnt(mesh0,
                                                              refinement_level);

  std::vector<ProblemSolution> solutions(refinement_level + 1);
  for (lf::base::size_type lvl = 0; lvl < refinement_level + 1; ++lvl) {
    const auto &mesh = mesh_hierarchy->getMesh(lvl);
    auto &sol = solutions[lvl];
    sol.mesh = mesh;
    sol.dofh = std::shared_ptr<const lf::assemble::DofHandler>(
        new lf::assemble::UniformFEDofHandler(
            mesh, {{lf::base::RefEl::kPoint(), 1},
                   {lf::base::RefEl::kSegment(), 1}}));
    // No volume forces are present
    auto f = [](const Eigen::Vector2d & /*unused*/) -> Eigen::Vector2d {
      return Eigen::Vector2d::Zero();
    };
    // Enforce a Poiseuille in- and outflow and no-slip boundary conditions at
    // the tube boundaries
    auto dirichlet_funct =
        [&](const lf::mesh::Entity &edge) -> Eigen::Vector2d {
      static constexpr double eps = 1e-10;
      const auto geom = edge.Geometry();
      const auto vertices = geom->Global(edge.RefEl().NodeCoords());
      const Eigen::Vector2d midpoint = vertices.rowwise().sum() / 2;
      Eigen::Vector2d v;
      if (vertices(0, 0) >= -3 - eps && vertices(0, 0) <= -3 + eps &&
          vertices(0, 1) >= -3 - eps && vertices(0, 1) <= -3 + eps) {
        // The edge is part of the inflow boundary
        v << poiseuilleVelocity(0.5, flowrate, midpoint[1] - 0.5), 0;
        return v;
      }
      if (vertices(0, 0) >= 3 - eps && vertices(0, 0) <= 3 + eps &&
          vertices(0, 1) >= 3 - eps && vertices(0, 1) <= 3 + eps) {
        // The edge is part of the outflow boundary
        v << poiseuilleVelocity(1, flowrate, midpoint[1]), 0;
        return v;
      }
      return Eigen::Vector2d::Zero();
    };
    const auto [A, rhs, offset_function] =
        projects::ipdg_stokes::assemble::buildSystemMatrixInOutFlow(
            sol.mesh, *(sol.dofh), f, dirichlet_funct, 1,
            lf::quad::make_TriaQR_MidpointRule(), false);
    sol.A = A.makeSparse();
    sol.rhs = rhs;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
    solver.compute(sol.A);
    sol.solution = solver.solve(rhs) + offset_function;
    const auto [A_modified, rhs_modified, offset_function_modified] =
        projects::ipdg_stokes::assemble::buildSystemMatrixInOutFlow(
            sol.mesh, *(sol.dofh), f, dirichlet_funct, 1,
            lf::quad::make_TriaQR_MidpointRule(), true);
    sol.A_modified = A_modified.makeSparse();
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver_modified(
        sol.A_modified);
    sol.solution_modified =
        solver_modified.solve(rhs_modified) + offset_function_modified;
  }

  // Bring the solutions on the meshes to the finest mesh
  std::map<lf::base::size_type,
           std::function<Eigen::Vector2d(const lf::mesh::Entity &,
                                         const Eigen::Vector2d &)>>
      velocity;
  std::map<lf::base::size_type,
           std::function<Eigen::Matrix2d(const lf::mesh::Entity &,
                                         const Eigen::Vector2d &)>>
      gradient;
  std::map<lf::base::size_type,
           std::function<Eigen::Vector2d(const lf::mesh::Entity &,
                                         const Eigen::Vector2d &)>>
      velocity_modified;
  std::map<lf::base::size_type,
           std::function<Eigen::Matrix2d(const lf::mesh::Entity &,
                                         const Eigen::Vector2d &)>>
      gradient_modified;
  for (lf::base::size_type lvl = 0; lvl < refinement_level + 1; ++lvl) {
    const auto v = projects::ipdg_stokes::post_processing::extractVelocity(
        solutions[lvl].mesh, *(solutions[lvl].dofh), solutions[lvl].solution);
    const auto v_modified =
        projects::ipdg_stokes::post_processing::extractVelocity(
            solutions[lvl].mesh, *(solutions[lvl].dofh),
            solutions[lvl].solution_modified);
    velocity[lvl] = [v](const lf::mesh::Entity &entity, const Eigen::Vector2d &
                        /*unused*/) -> Eigen::Vector2d { return v(entity); };
    gradient[lvl] =
        [](const lf::mesh::Entity & /*unused*/, const Eigen::Vector2d &
           /*unused*/) -> Eigen::Matrix2d { return Eigen::Matrix2d::Zero(); };
    velocity_modified[lvl] =
        [v_modified](
            const lf::mesh::Entity &entity, const Eigen::Vector2d &
            /*unused*/) -> Eigen::Vector2d { return v_modified(entity); };
    gradient_modified[lvl] =
        [](const lf::mesh::Entity & /*unused*/, const Eigen::Vector2d &
           /*unused*/) -> Eigen::Matrix2d { return Eigen::Matrix2d::Zero(); };
  }
  auto fine_velocity =
      projects::ipdg_stokes::post_processing::bringToFinestMesh(*mesh_hierarchy,
                                                                velocity);
  auto fine_gradient =
      projects::ipdg_stokes::post_processing::bringToFinestMesh(*mesh_hierarchy,
                                                                gradient);
  auto fine_velocity_modified =
      projects::ipdg_stokes::post_processing::bringToFinestMesh(
          *mesh_hierarchy, velocity_modified);
  auto fine_gradient_modified =
      projects::ipdg_stokes::post_processing::bringToFinestMesh(
          *mesh_hierarchy, gradient_modified);

  // Perform post processing on the data
  lf::io::VtkWriter writer(solutions.back().mesh, "result.vtk");
  for (lf::base::size_type lvl = 0; lvl < refinement_level; ++lvl) {
    writer.WriteCellData(concat("v_", solutions[lvl].mesh->NumEntities(2)),
                         *lf::mesh::utils::make_LambdaMeshDataSet(
                             [&](const lf::mesh::Entity &e) {
                               return fine_velocity[lvl](
                                   e, Eigen::Vector2d::Zero());
                             }));
    writer.WriteCellData(
        concat("v_modified", solutions[lvl].mesh->NumEntities(2)),
        *lf::mesh::utils::make_LambdaMeshDataSet(
            [&](const lf::mesh::Entity &e) {
              return fine_velocity_modified[lvl](e, Eigen::Vector2d::Zero());
            }));
    auto diff_v = [&](const lf::mesh::Entity &entity,
                      const Eigen::Vector2d &x) -> Eigen::Vector2d {
      return fine_velocity[lvl](entity, x) -
             fine_velocity[refinement_level](entity, x);
    };
    auto diff_v_modified = [&](const lf::mesh::Entity &entity,
                               const Eigen::Vector2d &x) -> Eigen::Vector2d {
      return fine_velocity_modified[lvl](entity, x) -
             fine_velocity_modified[refinement_level](entity, x);
    };
    auto diff_g = [&](const lf::mesh::Entity &entity,
                      const Eigen::Vector2d &x) -> Eigen::Matrix2d {
      return fine_gradient[lvl](entity, x) -
             fine_gradient[refinement_level](entity, x);
    };
    auto diff_g_modified = [&](const lf::mesh::Entity &entity,
                               const Eigen::Vector2d &x) -> Eigen::Matrix2d {
      return fine_gradient_modified[lvl](entity, x) -
             fine_gradient_modified[refinement_level](entity, x);
    };
    const double L2 = projects::ipdg_stokes::post_processing::L2norm(
        solutions[refinement_level].mesh, diff_v, 0);
    const double DG = projects::ipdg_stokes::post_processing::DGnorm(
        solutions[refinement_level].mesh, diff_v, diff_g, 0);
    const double L2_modified = projects::ipdg_stokes::post_processing::L2norm(
        solutions[refinement_level].mesh, diff_v_modified, 0);
    const double DG_modified = projects::ipdg_stokes::post_processing::DGnorm(
        solutions[refinement_level].mesh, diff_v_modified, diff_g_modified, 0);
    std::cout << lvl << ' ' << solutions[lvl].mesh->NumEntities(2) << ' ' << L2
              << ' ' << DG << ' ' << L2_modified << ' ' << DG_modified
              << std::endl;
  }

  return 0;
}
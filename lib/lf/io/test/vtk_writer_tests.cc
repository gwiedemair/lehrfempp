/**
 * @file
 * @brief Test the vtk writer implementation.
 * @author Raffael Casagrande
 * @date   2018-07-14 07:52:25
 * @copyright MIT License
 */

#include <gtest/gtest.h>
#include <lf/io/io.h>
#include <lf/io/test_utils/read_mesh.h>
#include "lf/mesh/utils/lambda_mesh_data_set.h"

namespace lf::io::test {

template <class T>
using FieldDataArray = VtkFile::FieldDataArray<T>;

template <class T>
using ScalarData = VtkFile::ScalarData<T>;

template <class T>
using VectorData = VtkFile::VectorData<T>;

TEST(lf_io_VtkWriter, writeVtkFile) {
  VtkFile vtk_file;
  vtk_file.header = "this is my test header :)";
  vtk_file.unstructured_grid.points = {
      {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  vtk_file.unstructured_grid.cells = {{0, 1, 3, 4}, {1, 2, 3}};
  vtk_file.unstructured_grid.cell_types = {VtkFile::CellType::VTK_QUAD,
                                           VtkFile::CellType::VTK_TRIANGLE};

  vtk_file.field_data.push_back(FieldDataArray<int>("array0", {0, 1, 2}));
  vtk_file.field_data.push_back(FieldDataArray<float>("array1", {-1, 0, 1}));
  vtk_file.field_data.push_back(FieldDataArray<double>("array2", {0, 1, 2}));

  vtk_file.point_data.push_back(ScalarData<char>("char", {-2, -1, 0, 1, 2}));
  vtk_file.point_data.push_back(
      ScalarData<unsigned char>("uchar", {1, 2, 3, 4, 5}));
  vtk_file.point_data.push_back(ScalarData<short>("short", {-2, -1, 0, 1, 2}));
  vtk_file.point_data.push_back(
      ScalarData<unsigned short>("ushort", {1, 2, 3, 4, 5}));
  vtk_file.point_data.push_back(ScalarData<int>("int", {-2, -1, 0, 1, 2}));
  vtk_file.point_data.push_back(
      ScalarData<unsigned int>("uint", {1, 2, 3, 4, 5}));
  // vtk_file.point_data.push_back(ScalarData<long>("long", {-2, -1, 0, 1, 2}));
  // vtk_file.point_data.push_back(
  //     ScalarData<unsigned long>("ulong", {1, 2, 3, 4, 5}));
  vtk_file.point_data.push_back(ScalarData<float>("float", {-2, -1, 0, 1, 2}));
  vtk_file.point_data.push_back(
      ScalarData<double>("double", {-2, -1, 0, 1, 2}));

  vtk_file.point_data.push_back(VectorData<float>(
      "vfloat", {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}}));
  vtk_file.point_data.push_back(VectorData<double>(
      "vdouble", {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}}));

  vtk_file.cell_data.push_back(ScalarData<char>("char", {0, 1}));
  vtk_file.cell_data.push_back(ScalarData<unsigned char>("uchar", {0, 1}));
  vtk_file.cell_data.push_back(ScalarData<short>("short", {0, 1}));
  vtk_file.cell_data.push_back(ScalarData<unsigned short>("ushort", {0, 1}));
  vtk_file.cell_data.push_back(ScalarData<int>("int", {0, 1}));
  vtk_file.cell_data.push_back(ScalarData<unsigned int>("uint", {0, 1}));
  // vtk_file.cell_data.push_back(ScalarData<long>("long", {0, 1}));
  // vtk_file.cell_data.push_back(ScalarData<unsigned long>("ulong", {0, 1}));
  vtk_file.cell_data.push_back(
      VectorData<float>("vfloat", {{0, 0, 0}, {-1, 0, 0}}));
  vtk_file.cell_data.push_back(
      VectorData<double>("vdouble", {{0, 0, 0}, {-1, 0, 0}}));

  WriteToFile(vtk_file, "all_features.vtk");

  vtk_file.format = VtkFile::Format::BINARY;
  WriteToFile(vtk_file, "all_features_binary.vtk");
}

TEST(lf_io_VtkWriter, vtkFilewriteOnlyMesh) {
  VtkFile vtk_file;
  vtk_file.header = "this is my test header :)";
  vtk_file.format = VtkFile::Format::ASCII;
  vtk_file.unstructured_grid.points = {
      {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {1, 1, 0}, {0, 1, 0}};
  vtk_file.unstructured_grid.cells = {{0, 1, 3, 4}, {1, 2, 3}};
  vtk_file.unstructured_grid.cell_types = {VtkFile::CellType::VTK_QUAD,
                                           VtkFile::CellType::VTK_TRIANGLE};

  WriteToFile(vtk_file, "only_mesh.vtk");

  vtk_file.format = VtkFile::Format::BINARY;
  WriteToFile(vtk_file, "only_mesh_binary.vtk");
}

TEST(lf_io_VtkWriter, twoElementMeshCodim0NoData) {
  auto reader = test_utils::getGmshReader("two_element_hybrid_2d.msh", 2);

  // write mesh:
  VtkWriter writer(reader.mesh(), "two_element_no_data.vtk");
}

TEST(lf_io_VtkWriter, twoElementMeshCodim1NoData) {
  auto reader = test_utils::getGmshReader("two_element_hybrid_2d.msh", 2);

  // write mesh:
  VtkWriter writer(reader.mesh(), "two_element_1d_nodata.vtk", 1);
}

TEST(lf_io_VtkWriter, twoElementMeshCodim0AllData) {
  auto reader = test_utils::getGmshReader("two_element_hybrid_2d.msh", 2);

  // write mesh:
  VtkWriter writer(reader.mesh(), "two_element.vtk");

  writer.WriteGlobalData("global_zeros", std::vector<int>{0, 0, 0});
  writer.WriteGlobalData("global_ones", std::vector<double>{1., 1., 1.});
  writer.WriteGlobalData("global_twos", std::vector<float>{2., 2., 2.});

  auto zero = Eigen::VectorXd::Zero(0);

  writer.WritePointData(
      "point_uchar", mesh::utils::LambdaMeshDataSet([&](const auto& e) {
        return static_cast<unsigned char>(reader.mesh()->Index(e));
      }));
  writer.WritePointData("point_char",
                        mesh::utils::LambdaMeshDataSet([&](const auto& e) {
                          return static_cast<char>(reader.mesh()->Index(e));
                        }));
  writer.WritePointData(
      "point_uint", mesh::utils::LambdaMeshDataSet([&](const auto& e) {
        return static_cast<unsigned int>(reader.mesh()->Index(e));
      }));
  writer.WritePointData("point_int",
                        mesh::utils::LambdaMeshDataSet([&](const auto& e) {
                          return static_cast<int>(reader.mesh()->Index(e));
                        }));
  writer.WritePointData("point_float",
                        mesh::utils::LambdaMeshDataSet([&](const auto& e) {
                          return static_cast<float>(reader.mesh()->Index(e));
                        }));
  writer.WritePointData("point_double",
                        mesh::utils::LambdaMeshDataSet([&](const auto& e) {
                          return static_cast<double>(reader.mesh()->Index(e));
                        }));
  writer.WritePointData("point_vectord2",
                        mesh::utils::LambdaMeshDataSet([&](const auto& e) {
                          return e.Geometry()->Global(zero);
                        }));
}

}  // namespace lf::io::test

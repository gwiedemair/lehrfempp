#ifndef __7a3f1903d42141a3b1135e8e5ad72c1c
#define __7a3f1903d42141a3b1135e8e5ad72c1c

#include <lf/base/base.h>
#include "lf/mesh/entity.h"
#include "lf/mesh/mesh_interface.h"

namespace lf::mesh::hybrid2d {

class Mesh;

/**
 * @brief classes for topological entities in a 2D hybrid mesh
 * @tparam CODIM the co-dimension of the entity object \f$\in\{0,1,2\}\f$
 *
 * This class template can be used to instantiate the four different topological
 * entities occurring in 2D hybrid meshes: Points, Edges, Triangles, and
 * Quadrilaterals.
 * @note Every `Entity` object owns a smart pointer to an associated geometry
 * object.
 *
 */
template <char CODIM>
// NOLINTNEXTLINE(hicpp-member-init)

class Entity : public mesh::Entity {
  using size_type = mesh::Mesh::size_type;

 public:
  /** @brief default constructors, needed by std::vector */
  Entity() = default;

  Entity(const Entity&) = delete;
  Entity(Entity&&) noexcept = default;
  Entity& operator=(const Entity&) = delete;
  Entity& operator=(Entity&&) noexcept = default;

  // constructor, is called from Mesh
  explicit Entity(
      Mesh* mesh, size_type index,

      /**
       * @brief constructor, is called from MeshFactory
       * @param mesh pointer to global hybrid mesh object
       * @param index index of the entity to be created; will usually be
       * retrieved via the `Index()` method of `Mesh`
       * @param geometry pointer to a geometry object providing the shape of the
       * entity
       * @param sub_entities indices of the sub-entities in the entity arrays of
       * the global mesh
       *
       * @note Note that you need to create a suitable geometry object for the
       * entity before you can initialize the entity object itseld.
       */
      std::unique_ptr<geometry::Geometry>&& geometry,
      std::array<std::vector<size_type>, 2 - CODIM> sub_entities)
      : mesh_(mesh),
        index_(index),
        geometry_(std::move(geometry)),
        sub_entities_(std::move(sub_entities)) {}

  char Codim() const override { return CODIM; }

  base::RandomAccessRange<const mesh::Entity> SubEntities(
      char rel_codim) const override;

  geometry::Geometry* Geometry() const override { return geometry_.get(); }

  base::RefEl RefEl() const override {
    switch (CODIM) {
      case 0:
        return sub_entities_[0].size() == 3 ? base::RefEl::kTria()
                                            : base::RefEl::kQuad();
      case 1:
        return base::RefEl::kSegment();
      case 2:
        return base::RefEl::kPoint();
      default:
        LF_VERIFY_MSG(false, "codim out of range.");
    }
  }

  bool operator==(const mesh::Entity& rhs) const override {
    return this == &rhs;
  }

  ~Entity() override = default;

 private:
  Mesh* mesh_ = nullptr;  // pointer to global hybrid 2D mesh object
  size_type index_ = -1;  // zero-based index of this entity.
  std::unique_ptr<geometry::Geometry> geometry_;
  std::array<std::vector<size_type>, 2 - CODIM> sub_entities_;

  friend class Mesh;

}; // class Mesh

}  // namespace lf::mesh::hybrid2d






#include "mesh.h"

namespace lf::mesh::hybrid2d {

template <char CODIM>
base::RandomAccessRange<const mesh::Entity> Entity<CODIM>::SubEntities(
    char rel_codim) const {
  switch (2 - CODIM - rel_codim) {
    case 2:
      // This case is relevant only for CODIM = 0 and codim =0,
      // that is for cells; return ourselves as the only element of the range
      return {this, this + 1};
    case 1:
      // This case is visited, if
      // (i) either the entity is an edge (CODIM = 1)
      return {base::make_DereferenceLambdaRandomAccessIterator(
                  sub_entities_[rel_codim - 1].begin(),
                  [&](auto i) -> const mesh::Entity& {
                    return mesh_->entities1_[*i];
                  }),
              base::make_DereferenceLambdaRandomAccessIterator(
                  sub_entities_[rel_codim - 1].end(),
                  [&](auto i) -> const mesh::Entity& {
                    return mesh_->entities1_[*i];
                  })};
    case 0:
      return {base::make_DereferenceLambdaRandomAccessIterator(
                  sub_entities_[rel_codim - 1].begin(),
                  [&](auto i) -> const mesh::Entity& {
                    return mesh_->entities2_[*i];
                  }),
              base::make_DereferenceLambdaRandomAccessIterator(
                  sub_entities_[rel_codim - 1].end(),
                  [&](auto i) -> const mesh::Entity& {
                    return mesh_->entities2_[*i];
                  })};
    default:
      LF_VERIFY_MSG(false, "codim is out of bounds.");
  }
}

}  // namespace lf::mesh::hybrid2d

#endif  // __7a3f1903d42141a3b1135e8e5ad72c1c

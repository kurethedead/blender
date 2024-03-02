/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 */

#pragma once

#include "DRW_render.hh"

#include "BLI_map.hh"
#include "BLI_vector.hh"
#include "GPU_material.h"

#include "fast64_sync.hh"

namespace blender::fast64 {

class Instance;

/* -------------------------------------------------------------------- */
/** \name MaterialKey
 *
 * \{ */

/*
Cached GPUMaterials are stored on blender::Material.gpumaterial.
The uuid of the GPUMaterial is checked to see if a cached material is present.
Thus, the uuid must be unique across all render engines.
As of Blender 4.0. eevee uses 13 bits for its uuid (see eevee/eevee_private.h, the enum with VAR_...)
eevee_next uses 9 bits for its uuid (see eevee_next/eevee_material.hh, shader_uuid_from_material_type())
Thus, we set an arbitrary bit outside that range so that fast64 cached material uuids don't clash with eeevee/eevee_next.
Note that the uuid only represents geometry/pipeline permutations in eevee, and that actual shader changes will regenerate cache.
Thus fast64 will really only have one uuid value ever being used, since it only renders meshes in a simple forward pipeline.
*/
#define FAST64_SHADER_FLAG (1 << 31)

enum eMaterialPipeline {
  MAT_PIPE_FORWARD = 0,
};

enum eMaterialGeometry {
  /* These maps directly to object types. */
  MAT_GEOM_MESH = 0,
  MAT_GEOM_CURVES,

  /* These maps to special shader. */
  MAT_GEOM_WORLD,
};

//static inline bool geometry_type_has_surface(eMaterialGeometry geometry_type)
//{
//  return geometry_type < MAT_GEOM_VOLUME;
//}

static inline void material_type_from_shader_uuid(uint64_t shader_uuid,
                                                  eMaterialGeometry &geometry_type)
{
  const uint64_t geometry_mask = (FAST64_SHADER_FLAG - 1u);
  geometry_type = static_cast<eMaterialGeometry>(shader_uuid & geometry_mask);
}

static inline uint64_t shader_uuid_from_material_type(eMaterialGeometry geometry_type)
{
  return geometry_type | FAST64_SHADER_FLAG;
}

static inline eMaterialGeometry to_material_geometry(const Object *ob)
{
  switch (ob->type) {
    case OB_CURVES:
      return MAT_GEOM_CURVES;
    default:
      return MAT_GEOM_MESH;
  }
}

/**
 * Unique key to identify each material in the hash-map.
 * This is above the shader binning.
 */
struct MaterialKey {
  ::Material *mat;
  uint64_t options;

  MaterialKey(::Material *mat_,
              eMaterialGeometry geometry)
      : mat(mat_)
  {
    options = shader_uuid_from_material_type(geometry);
  }

  uint64_t hash() const
  {
    return uint64_t(mat) + options;
  }

  bool operator<(const MaterialKey &k) const
  {
    if (mat == k.mat) {
      return options < k.options;
    }
    return mat < k.mat;
  }

  bool operator==(const MaterialKey &k) const
  {
    return (mat == k.mat) && (options == k.options);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name ShaderKey
 *
 * \{ */

/**
 * Key used to find the sub-pass that already renders objects with the same shader.
 * This avoids the cost associated with shader switching.
 * This is below the material binning.
 * Should only include pipeline options that are not baked in the shader itself.
 */
struct ShaderKey {
  GPUShader *shader;
  uint64_t options;

  ShaderKey(GPUMaterial *gpumat, eMaterialGeometry geometry)
  {
    shader = GPU_material_get_shader(gpumat);
    options = shader_uuid_from_material_type(geometry);
  }

  uint64_t hash() const
  {
    return uint64_t(shader) + options;
  }

  bool operator<(const ShaderKey &k) const
  {
    return (shader == k.shader) ? (options < k.options) : (shader < k.shader);
  }

  bool operator==(const ShaderKey &k) const
  {
    return (shader == k.shader) && (options == k.options);
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material
 *
 * \{ */

struct MaterialPass {
  GPUMaterial *gpumat;
  PassMain::Sub *sub_pass;
};

struct Material {
  bool is_transparent;
  MaterialPass shading;
};

struct MaterialArray {
  Vector<Material> materials;
  Vector<GPUMaterial *> gpu_materials;
};

class MaterialModule {
 public:

  int64_t queued_shaders_count = 0;
  int64_t queued_optimize_shaders_count = 0;

 private:
  Instance &inst_;

  Map<MaterialKey, Material> material_map_;
  Map<ShaderKey, PassMain::Sub *> shader_map_;

  MaterialArray material_array_;

  ::Material *error_mat_; // for errors
  ::Material *unknown_mat_; // for materials not renderable in this context

 public:
  MaterialModule(Instance &inst);
  ~MaterialModule();

  void begin_sync();

  /**
   * Returned Material references are valid until the next call to this function or material_get().
   */
  MaterialArray &material_array_get(Object *ob);
  /**
   * Returned Material references are valid until the next call to this function or
   * material_array_get().
   */
  Material &material_get(Object *ob, int mat_nr, eMaterialGeometry geometry_type);

 private:
  Material &material_sync(Object *ob,
                          ::Material *blender_mat,
                          eMaterialGeometry geometry_type,
                          );

  /** Return correct material or empty default material if slot is empty. */
  ::Material *material_from_slot(Object *ob, int slot);
  Material material_create(Object *ob,
                                 ::Material *blender_mat,
                                 eMaterialGeometry geometry_type);
};

/** \} */

}  // namespace blender::fast64

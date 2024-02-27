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

enum eMaterialPipeline {
  MAT_PIPE_FORWARD = 0,
};

enum eMaterialGeometry {
  /* These maps directly to object types. */
  MAT_GEOM_MESH = 0,
  MAT_GEOM_CURVES,
  MAT_GEOM_GPENCIL,

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
  const uint64_t geometry_mask = ((1u << 4u) - 1u);
  geometry_type = static_cast<eMaterialGeometry>(shader_uuid & geometry_mask);
}

static inline uint64_t shader_uuid_from_material_type(
    eMaterialGeometry geometry_type,
    char blend_flags = 0)
{
  BLI_assert(geometry_type < (1 << 4));
  uint64_t transparent_shadows = blend_flags & MA_BL_TRANSPARENT_SHADOW ? 1 : 0;
  return geometry_type | (transparent_shadows << 10);
}

static inline eClosureBits shader_closure_bits_from_flag(const GPUMaterial *gpumat)
{
  eClosureBits closure_bits = eClosureBits(0);
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_DIFFUSE)) {
    closure_bits |= CLOSURE_DIFFUSE;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSPARENT)) {
    closure_bits |= CLOSURE_TRANSPARENCY;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_TRANSLUCENT)) {
    closure_bits |= CLOSURE_TRANSLUCENT;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_EMISSION)) {
    closure_bits |= CLOSURE_EMISSION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_GLOSSY)) {
    closure_bits |= CLOSURE_REFLECTION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SUBSURFACE)) {
    closure_bits |= CLOSURE_SSS;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_REFRACT)) {
    closure_bits |= CLOSURE_REFRACTION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_HOLDOUT)) {
    closure_bits |= CLOSURE_HOLDOUT;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_AO)) {
    closure_bits |= CLOSURE_AMBIENT_OCCLUSION;
  }
  if (GPU_material_flag_get(gpumat, GPU_MATFLAG_SHADER_TO_RGBA)) {
    closure_bits |= CLOSURE_SHADER_TO_RGBA;
  }
  return closure_bits;
}

static inline eMaterialGeometry to_material_geometry(const Object *ob)
{
  switch (ob->type) {
    case OB_CURVES:
      return MAT_GEOM_CURVES;
    case OB_GPENCIL_LEGACY:
      return MAT_GEOM_GPENCIL;
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
              eMaterialGeometry geometry,
              short visibility_flags)
      : mat(mat_)
  {
    options = shader_uuid_from_material_type(geometry, mat_->blend_flag);
    //options = (options << 1) | (visibility_flags & OB_HIDE_SHADOW ? 0 : 1);
    //options = (options << 1) | (visibility_flags & OB_HIDE_PROBE_CUBEMAP ? 0 : 1);
    //options = (options << 1) | (visibility_flags & OB_HIDE_PROBE_PLANAR ? 0 : 1);
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

  ShaderKey(GPUMaterial *gpumat)
  {
    shader = GPU_material_get_shader(gpumat);
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
  bool is_alpha_blend_transparent;
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

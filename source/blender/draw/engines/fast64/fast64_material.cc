/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 */

#include "DNA_material_types.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_node.hh"
#include "NOD_shader.h"

#include "fast64_instance.hh"

#include "fast64_material.hh"

namespace blender::fast64 {

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material
 *
 * \{ */

MaterialModule::MaterialModule(Instance &inst) : inst_(inst)
{
  {
    // BKE_id_new_nomain() creates a new datablock without putting it into Main database
    error_mat_ = (::Material *)BKE_id_new_nomain(ID_MA, "FAST64 default error");
    // TODO: get f3d properties, set it to something simple like unlit magenta
  }
  {
    unknown_mat_ = (::Material *)BKE_id_new_nomain(ID_MA, "FAST64 default unknown");
    // TODO: same as above
  }
}

MaterialModule::~MaterialModule()
{
  BKE_id_free(nullptr, error_mat_);
  BKE_id_free(nullptr, unknown_mat_);
}

void MaterialModule::begin_sync()
{
  material_map_.clear();
  shader_map_.clear();
}

MaterialPass MaterialModule::material_pass_get(Object *ob,
                                               ::Material *blender_mat,
                                               eMaterialGeometry geometry_type)
{
  PassMain::Sub matpass = MaterialPass();

  // TODO: we only need a single shader, but static_shader_get() returns GPUShader
  matpass.gpumat = inst_.shaders.material_shader_get(blender_mat, geometry_type, F3D_MESH); // if we ever have more shaders, pull info from blender_mat

  /* Returned material should be ready to be drawn. */
  BLI_assert(GPU_material_status(matpass.gpumat) == GPU_MAT_SUCCESS);

  inst_.manager->register_layer_attributes(matpass.gpumat);

  //if (GPU_material_recalc_flag_get(matpass.gpumat)) {
    /* TODO(Miguel Pozo): This is broken, it consumes the flag,
     * but GPUMats can be shared across viewports. */
    //inst_.sampling.reset();
  //}

  const bool is_transparent = GPU_material_flag_get(matpass.gpumat, GPU_MATFLAG_TRANSPARENT);
  if (is_transparent) {
    /* Sub pass is generated later, so that we can sort by distance. */
    matpass.sub_pass = nullptr;
  }
  else {
    ShaderKey shader_key(matpass.gpumat, geometry_type);

    PassMain::Sub *shader_sub = shader_map_.lookup_or_add_cb(shader_key, [&]() {
      /* First time encountering this shader. Create a sub that will contain materials using it. */
      return inst_.pipelines.material_add(
          ob, blender_mat, matpass.gpumat);
    });

    if (shader_sub != nullptr) {
      /* Create a sub for this material as `shader_sub` is for sharing shader between materials. */
      matpass.sub_pass = &shader_sub->sub(GPU_material_get_name(matpass.gpumat));

      // TODO: This calls shader_set() every time - unnecessary?
      matpass.sub_pass->material_set(*inst_.manager, matpass.gpumat);
    }
    else {
      matpass.sub_pass = nullptr;
    }
  }

  return matpass;
}

Material &MaterialModule::material_sync(Object *ob,
                                        ::Material *blender_mat,
                                        eMaterialGeometry geometry_type)
{
  MaterialKey material_key(blender_mat, geometry_type);

  Material &mat = material_map_.lookup_or_add_cb(material_key, [&]() {
    Material mat;
    mat.shading = material_pass_get(ob, blender_mat, geometry_type);
    mat.is_transparent = GPU_material_flag_get(mat.shading.gpumat,
                                                           GPU_MATFLAG_TRANSPARENT);
    return mat;
  });

  if (mat.is_transparent) {
    /* Transparent needs to use one sub pass per object to support reordering.
     * NOTE: Pre-pass needs to be created first in order to be sorted first. */
    mat.shading.sub_pass = inst_.pipelines.forward.material_transparent_add(
        ob, blender_mat, mat.shading.gpumat);
  }
  return mat;
}

::Material *MaterialModule::material_from_slot(Object *ob, int slot)
{
  if (ob->base_flag & BASE_HOLDOUT) {
    return BKE_material_default_holdout();
  }
  ::Material *ma = BKE_object_material_get(ob, slot + 1);
  if (ma == nullptr) {
    if (ob->type == OB_VOLUME) {
      return BKE_material_default_volume();
    }
    return BKE_material_default_surface();
  }
  return ma;
}

MaterialArray &MaterialModule::material_array_get(Object *ob)
{
  material_array_.materials.clear();
  material_array_.gpu_materials.clear();

  const int materials_len = DRW_cache_object_material_count_get(ob);

  for (auto i : IndexRange(materials_len)) {
    ::Material *blender_mat = material_from_slot(ob, i);
    Material &mat = material_sync(ob, blender_mat, to_material_geometry(ob));
    /* \note: Perform a whole copy since next material_sync() can move the Material memory location
     * (i.e: because of its container growing) */
    material_array_.materials.append(mat);
    material_array_.gpu_materials.append(mat.shading.gpumat);
  }
  return material_array_;
}

Material &MaterialModule::material_get(Object *ob,
                                       int mat_nr,
                                       eMaterialGeometry geometry_type)
{
  ::Material *blender_mat = material_from_slot(ob, mat_nr);
  Material &mat = material_sync(ob, blender_mat, geometry_type);
  return mat;
}

/** \} */

}  // namespace blender::fast64

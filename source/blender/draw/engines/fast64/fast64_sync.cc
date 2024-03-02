/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Converts the different renderable object types to drawcalls.
 */

#include "fast64_engine.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "draw_common.hh"
#include "draw_sculpt.hh"

#include "fast64_instance.hh"

namespace blender::fast64 {

/* -------------------------------------------------------------------- */
/** \name Recalc
 *
 * \{ */

void SyncModule::view_update()
{
  if (DEG_id_type_updated(inst_.depsgraph, ID_WO)) {
    world_updated_ = true;
  }
}

ObjectHandle &SyncModule::sync_object(const ObjectRef &ob_ref)
{
  ObjectKey key(ob_ref.object);

  ObjectHandle &handle = ob_handles.lookup_or_add_cb(key, [&]() {
    ObjectHandle new_handle;
    new_handle.object_key = key;
    return new_handle;
  });

  handle.recalc = inst_.get_recalc_flags(ob_ref);

  return handle;
}

WorldHandle SyncModule::sync_world()
{
  WorldHandle handle;
  handle.recalc = world_updated_ ? int(ID_RECALC_SHADING) : 0;
  world_updated_ = false;
  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

static inline void geometry_call(PassMain::Sub *sub_pass,
                                 GPUBatch *geom,
                                 ResourceHandle resource_handle)
{
  if (sub_pass != nullptr) {
    sub_pass->draw(geom, resource_handle);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

void SyncModule::sync_mesh(Object *ob,
                           ObjectHandle &ob_handle,
                           ResourceHandle res_handle,
                           const ObjectRef &ob_ref)
{
  MaterialArray &material_array = inst_.materials.material_array_get(ob);

  GPUBatch **mat_geom = DRW_cache_object_surface_material_get(
      ob, material_array.gpu_materials.data(), material_array.gpu_materials.size());

  if (mat_geom == nullptr) {
    return;
  }

  if ((ob->dt < OB_SOLID) && !DRW_state_is_scene_render()) {
    /** NOTE:
     * FAST64 doesn't render meshes with bounds or wire display type in the viewport,
     * but Cycles does. */
    return;
  }

  //bool is_alpha_blend = false;
  //float inflate_bounds = 0.0f;
  for (auto i : material_array.gpu_materials.index_range()) {
    GPUBatch *geom = mat_geom[i];
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[i];
    GPUMaterial *gpu_material = material_array.gpu_materials[i];

    //geometry_call(material.capture.sub_pass, geom, res_handle);
    //geometry_call(material.overlap_masking.sub_pass, geom, res_handle);
    //geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shading.sub_pass, geom, res_handle);
    //geometry_call(material.shadow.sub_pass, geom, res_handle);

    //geometry_call(material.planar_probe_prepass.sub_pass, geom, res_handle);
    //geometry_call(material.planar_probe_shading.sub_pass, geom, res_handle);
    //geometry_call(material.reflection_probe_prepass.sub_pass, geom, res_handle);
    //geometry_call(material.reflection_probe_shading.sub_pass, geom, res_handle);

    //is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    ::Material *mat = GPU_material_get_material(gpu_material);
    //inst_.cryptomatte.sync_material(mat);

    //if (GPU_material_has_displacement_output(gpu_material)) {
    //  inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    //}
  }

  //if (inflate_bounds != 0.0f) {
  //  inst_.manager->update_handle_bounds(res_handle, ob_ref, inflate_bounds);
  //}

  // This is where vertex attributes are obtained?
  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  //inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
  //inst_.cryptomatte.sync_object(ob, res_handle);
}

bool SyncModule::sync_sculpt(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             const ObjectRef &ob_ref)
{
  bool pbvh_draw = BKE_sculptsession_use_pbvh_draw(ob, inst_.rv3d) && !DRW_state_is_image_render();
  /* Needed for mesh cache validation, to prevent two copies of
   * of vertex color arrays from being sent to the GPU (e.g.
   * when switching from fast64 to workbench).
   */
  if (ob_ref.object->sculpt && ob_ref.object->sculpt->pbvh) {
    BKE_pbvh_is_drawing_set(ob_ref.object->sculpt->pbvh, pbvh_draw);
  }

  if (!pbvh_draw) {
    return false;
  }

  MaterialArray &material_array = inst_.materials.material_array_get(ob);

  bool is_alpha_blend = false;
  float inflate_bounds = 0.0f;
  for (SculptBatch &batch :
       sculpt_batches_per_material_get(ob_ref.object, material_array.gpu_materials))
  {
    GPUBatch *geom = batch.batch;
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[batch.material_slot];

    //geometry_call(material.capture.sub_pass, geom, res_handle);
    //geometry_call(material.overlap_masking.sub_pass, geom, res_handle);
    //geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shading.sub_pass, geom, res_handle);
    //geometry_call(material.shadow.sub_pass, geom, res_handle);

    //geometry_call(material.planar_probe_prepass.sub_pass, geom, res_handle);
    //geometry_call(material.planar_probe_shading.sub_pass, geom, res_handle);
    //geometry_call(material.reflection_probe_prepass.sub_pass, geom, res_handle);
    //geometry_call(material.reflection_probe_shading.sub_pass, geom, res_handle);

    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    GPUMaterial *gpu_material = material_array.gpu_materials[batch.material_slot];
    ::Material *mat = GPU_material_get_material(gpu_material);
    //inst_.cryptomatte.sync_material(mat);

    //if (GPU_material_has_displacement_output(gpu_material)) {
    //  inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    //}
  }

  /* Use a valid bounding box. The PBVH module already does its own culling, but a valid */
  /* bounding box is still needed for directional shadow tile-map bounds computation. */
  const Bounds<float3> bounds = BKE_pbvh_bounding_box(ob_ref.object->sculpt->pbvh);
  const float3 center = math::midpoint(bounds.min, bounds.max);
  const float3 half_extent = bounds.max - center + inflate_bounds;
  inst_.manager->update_handle_bounds(res_handle, center, half_extent);

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  //inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
  //inst_.cryptomatte.sync_object(ob, res_handle);

  return true;
}

/** \} */

}  // namespace blender::fast64

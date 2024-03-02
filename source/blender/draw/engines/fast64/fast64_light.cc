/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * The light module manages light data buffers and light culling system.
 */

#include "draw_debug.hh"

#include "fast64_instance.hh"

#include "fast64_light.hh"

#include "BLI_math_rotation.h"

namespace blender::fast64 {

/* -------------------------------------------------------------------- */
/** \name LightData
 * \{ */

static eLightType to_light_type(short blender_light_type)
{
  switch (blender_light_type) {
    case LA_LOCAL:
      return LIGHT_POINT;
    case LA_SUN:
      return LIGHT_SUN;
    default:
      return LIGHT_SUN; // we only support point or sun lights
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

void Light::sync(const Object *ob)
{
  // get blender light
  const ::Light *la = (const ::Light *)ob->data;
  float scale[3];

  BLI_assert_msg(la->type == LA_SUN || la->type == LA_LOCAL, "LightModule should only sync directional/point lights");

  //float max_power = max_fff(la->r, la->g, la->b) * fabsf(la->energy / 100.0f);
  //float surface_max_power = max_ff(la->diff_fac, la->spec_fac) * max_power;
  //float volume_max_power = la->volume_fac * max_power;

  //float influence_radius_surface = attenuation_radius_get(la, threshold, surface_max_power);
  //float influence_radius_volume = attenuation_radius_get(la, threshold, volume_max_power);

  //this->influence_radius_max = max_ff(influence_radius_surface, influence_radius_volume);
  //this->influence_radius_invsqr_surface = 1.0f / square_f(max_ff(influence_radius_surface, 1e-8f));
  //this->influence_radius_invsqr_volume = 1.0f / square_f(max_ff(influence_radius_volume, 1e-8f));

  float4x4 object_mat;
  normalize_m4_m4_ex(object_mat.ptr(), ob->object_to_world, scale);
  
  /* Make sure we have consistent handedness (in case of negatively scaled Z axis). */
  float3 cross = math::cross(float3(object_mat[0]), float3(object_mat[1]));
  if (math::dot(cross, float3(object_mat[2])) < 0.0f) {
    negate_v3(object_mat[1]);
  }

  this->color = float4(float3(&la->r) * la->energy, 1);
  this->direction = float4(math::normalize(math::transform_direction(object_mat, float3(0,0,-1))), 0);
  this->position = float4(object_mat.location(), 0);
  this->type = to_light_type(la->type);

  // TODO: attenuation, specular

  //this->initialized = true;
}

void Light::debug_draw()
{
#ifndef NDEBUG
  // 10? just a random value
  drw_debug_sphere(float3(position), 10, float4(0.8f, 0.3f, 0.0f, 1.0f));
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

LightModule::~LightModule()
{
};

void LightModule::begin_sync()
{
  use_scene_lights_ = inst_.use_scene_lights();
}

void LightModule::sync_light(const Object *ob, ObjectHandle &handle)
{
  if (use_scene_lights_ == false) {
    return;
  }
  const ::Light *la = (const ::Light *)ob->data;

  // Ignore spot/area lights
  if(la->type != LA_SUN && la->type != LA_LOCAL) {
    return;
  }

  Light &light = light_map_.lookup_or_add_default(handle.object_key);
  light.used = true;

  // if light object need recalculation, we re-sync light
  if (handle.recalc != 0) {
    light.sync(ob);
  }
}

void LightModule::end_sync()
{
  float* ambient = inst_.scene->fast64.ambient_light;
  light_buf_.ambient = float4(ambient[0], ambient[1], ambient[2], ambient[3]);
  light_buf_.lightCount = 0;
  auto it_end = light_map_.items().end();
  for (auto it = light_map_.items().begin(); it != it_end; ++it) {
    Light &light = (*it).value;

    // Remove deleted light objects
    if (!light.used) {
      light_map_.remove(it);
      continue;
    }

    // We want to stop at limit, but still process lights over limit in case they were removed
    if(light_buf_.lightCount < MAX_LIGHTS) { // TODO: replace with query of scene on instance
      light_buf_.lights[light_buf_.lightCount++] = light;
    }

    /* Untag for next sync. */
    light.used = false;
  }
  /* This scene data buffer is then immutable after this point. */
  light_buf_.push_update();
}

/** \} */

}  // namespace blender::fast64

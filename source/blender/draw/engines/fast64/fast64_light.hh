/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * The light module manages light data buffers.
 *
 */

#pragma once

#include "BLI_bitmap.h"
#include "BLI_vector.hh"
#include "DNA_light_types.h"

#include "fast64_camera.hh"
//#include "fast64_sampling.hh"
#include "fast64_shader.hh"
#include "fast64_shader_shared.hh"
#include "fast64_sync.hh"

namespace blender::fast64 {

class Instance;

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

struct Light : public LightData, NonCopyable {
 public:
  // We keep track of lights in module
  // After processing list of lights, we mark all as unusued
  // On next tick, we re-iterate over all light objects and mark existing light as used if object still exists
  // All non-used lights afterward are removed
  bool used = false;

 public:
  Light()
  {
    /* Avoid valgrind warning. */
    this->type = LIGHT_SUN;
  }

  /* Only used for debugging. */
#ifndef NDEBUG
  Light(Light &&other)
  {
    *static_cast<LightData *>(this) = other;
    this->used = other.used;
  }

  ~Light()
  {
  }
#endif

  void sync(const Object *ob);

  void debug_draw();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

/**
 * The light module manages light data buffers and light culling system.
 */
class LightModule {

 private:

  Instance &inst_;

  /** Map of light objects data. Converted to flat array each frame. */
  Map<ObjectKey, Light> light_map_;
  LightDataBuf light_buf_ = {"Lights"};
  bool use_scene_lights_ = false;

 public:
  LightModule(Instance &inst) : inst_(inst){};
  ~LightModule();

  void begin_sync();
  void sync_light(const Object *ob, ObjectHandle &handle);
  void end_sync();
};

/** \} */

}  // namespace blender::fast64

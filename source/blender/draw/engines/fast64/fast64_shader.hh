/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * Shader module that manage shader libraries, deferred compilation,
 * and static shader usage.
 */

#pragma once

#include <array>
#include <string>

#include "BLI_string_ref.hh"
#include "DRW_render.hh"
#include "GPU_material.h"
#include "GPU_shader.h"

#include "fast64_material.hh"
#include "fast64_sync.hh"

namespace blender::fast64 {

/* Keep alphabetical order and clean prefix. */
enum eShaderType {
  F3D_MESH = 0,
  FILM_FRAG,
  FILM_COMP,
  MAX_SHADER_TYPE,
};

/**
 * Shader module. shared between instances.
 */
class ShaderModule {
 private:
  std::array<GPUShader *, MAX_SHADER_TYPE> shaders_;

  /** Shared shader module across all engine instances. */
  static ShaderModule *g_shader_module;

 public:
  ShaderModule();
  ~ShaderModule();

  GPUShader *static_shader_get(eShaderType shader_type);

  /** Only to be used by Instance constructor. */
  static ShaderModule *module_get();
  static void module_free();

 private:
  const char *static_shader_create_info_name_get(eShaderType shader_type);
};

}  // namespace blender::fast64

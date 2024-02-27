/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shared structures, enums & defines between C++ and GLSL.
 * Can also include some math functions but they need to be simple enough to be valid in both
 * language.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO
#  pragma once

#  include "BLI_memory_utils.hh"
#  include "DRW_gpu_wrapper.hh"

#  include "draw_manager.hh"
#  include "draw_pass.hh"

//#  include "fast64_defines.hh"

#  include "GPU_shader_shared.h"

namespace blender::fast64 {

using namespace draw;

constexpr GPUSamplerState no_filter = GPUSamplerState::default_sampler();
constexpr GPUSamplerState with_filter = {GPU_SAMPLER_FILTERING_LINEAR};

#endif

#define UBO_MIN_MAX_SUPPORTED_SIZE 1 << 14
#define MAX_LIGHTS 9 // Note : actual max may be lower depending on f3d version

enum eLightType : uint32_t {
  LIGHT_SUN = 0u,
  LIGHT_POINT = 1u,
};


/* -------------------------------------------------------------------- */
/** \name Debug Mode
 * \{ */

/* Reserved range is 1-30. */
enum eDebugMode : uint32_t {
  DEBUG_NONE = 0u,
  DEBUG_VERTEX_COLORS
};

/** \} */

// Note - vec3 are stored as float4 for packing, do I need to do this?

struct F3DState {
  float4 prim_color;
  // TODO: everything
};
BLI_STATIC_ASSERT_ALIGN(F3DState, 16)

// contains all params for directional and point lights
struct LightData {
  float4 color;
  float4 direction;
  float4 position;

  // TODO: Add these to blender light struct in DNA files
  float kc; // constant attenuation for point lighting
  float kl; // linear attenuation for point lighting
  float kq; // quadratic attenuation for point lighting
  float specular;

  eLightType type;
  int _pad0, _pad1, _pad2;
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

struct LightsData {
  float4 ambient;
  LightData lights[MAX_LIGHTS];
  int lightCount;
  int _pad1, _pad2, _pad3; 
};
BLI_STATIC_ASSERT_ALIGN(LightsData, 16)

/* -------------------------------------------------------------------- */
/** \name Uniform Data
 * \{ */

/* Combines data from several modules to avoid wasting binding slots. */
struct UniformData {
  F3DState f3d_state;
  LightData light_data;
};
BLI_STATIC_ASSERT_ALIGN(UniformData, 16)

/** \} */


/* -------------------------------------------------------------------- */
/** \name Camera
 * \{ */

enum eCameraType : uint32_t {
  CAMERA_PERSP = 0u,
  CAMERA_ORTHO = 1u,
  CAMERA_PANO_EQUIRECT = 2u,
  CAMERA_PANO_EQUISOLID = 3u,
  CAMERA_PANO_EQUIDISTANT = 4u,
  CAMERA_PANO_MIRROR = 5u
};

static inline bool is_panoramic(eCameraType type)
{
  return type > CAMERA_ORTHO;
}

struct CameraData {
  /* View Matrices of the camera, not from any view! */
  float4x4 persmat;
  float4x4 persinv;
  float4x4 viewmat;
  float4x4 viewinv;
  float4x4 winmat;
  float4x4 wininv;
  /** Camera UV scale and bias. */
  float2 uv_scale;
  float2 uv_bias;
  /** Panorama parameters. */
  float2 equirect_scale;
  float2 equirect_scale_inv;
  float2 equirect_bias;
  float fisheye_fov;
  float fisheye_lens;
  /** Clipping distances. */
  float clip_near;
  float clip_far;
  eCameraType type;
  /** World space distance between view corners at unit distance from camera. */
  float screen_diagonal_length;
  float _pad0;
  float _pad1;
  float _pad2;

  bool1 initialized;

#ifdef __cplusplus
  /* Small constructor to allow detecting new buffers. */
  CameraData() : initialized(false){};
#endif
};
BLI_STATIC_ASSERT_ALIGN(CameraData, 16)

struct FilmData {
  /** Size of the film in pixels. */
  int2 extent;
  /** Offset to convert from Display space to Film space, in pixels. */
  int2 offset;
  /** Size of the render buffers when rendering the main views, in pixels. */
  int2 render_extent;
  /** Offset to convert from Film space to Render space, in pixels. */
  int2 render_offset;
  /**
   * Sub-pixel offset applied to the window matrix.
   * NOTE: In final film pixel unit.
   * NOTE: Positive values makes the view translate in the negative axes direction.
   * NOTE: The origin is the center of the lower left film pixel of the area covered by a render
   * pixel if using scaled resolution rendering.
   */
  float2 subpixel_offset;
  /** Scaling factor to convert texel to uvs. */
  float2 extent_inv;
  /** Is true if accumulation of non-filtered passes is needed. */
  bool1 has_data;
  /** Controlled by user in lookdev mode or by render settings. */
  float background_opacity;
  /** True if we bypass the accumulation and directly output the accumulation buffer. */
  bool1 display_only;
  /** Scaling factor for scaled resolution rendering. */
  int scaling_factor;

  float4 _pad0;
  
};
BLI_STATIC_ASSERT_ALIGN(FilmData, 16)

/* __cplusplus is true when compiling with MSL, so ensure we are not inside a shader. */
#if defined(__cplusplus) && !defined(GPU_SHADER)

using CameraDataBuf = draw::UniformBuffer<CameraData>;
using UniformDataBuf = draw::UniformBuffer<UniformData>;
//using LightDataBuf = draw::StorageArrayBuffer<LightData, LIGHT_CHUNK>;
using LightDataBuf = draw::UniformBuffer<LightsData>;

}  // namespace blender::fast64
#endif

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_camera_types.h"
#include "DRW_render.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"

#include "fast64_defines.hh"
#include "fast64_enums.hh"
#include "fast64_shader_shared.h"

#include "GPU_capabilities.h"

extern "C" DrawEngineType draw_engine_fast64;

namespace blender::fast64 {

using namespace draw;

class StaticShader : NonCopyable {
 private:
  std::string info_name_;
  GPUShader *shader_ = nullptr;

 public:
  StaticShader(std::string info_name) : info_name_(info_name) {}

  StaticShader() = default;
  StaticShader(StaticShader &&other) = default;
  StaticShader &operator=(StaticShader &&other) = default;

  ~StaticShader()
  {
    DRW_SHADER_FREE_SAFE(shader_);
  }

  GPUShader *get()
  {
    if (!shader_) {
      BLI_assert(!info_name_.empty());
      shader_ = GPU_shader_create_from_info_name(info_name_.c_str());
    }
    return shader_;
  }
};

class ShaderCache {
 private:
  static ShaderCache *static_cache;

  StaticShader shading_[pipeline_type_len];

 public:
  static ShaderCache &get();
  static void release();

  ShaderCache();

  GPUShader *shader_get(ePipelineType pipeline_type)
  {
    return prepass_[int(pipeline_type)].get();
  }
  StaticShader film = {"fast64_film"};
};

struct Material {
  float3 base_color = float3(0);
  /* Packed data into a int. Decoded in the shader. */
  uint packed_data = 0;

  Material();
  Material(float3 color);
  Material(::Object &ob, bool random = false);
  Material(::Material &mat);

  static uint32_t pack_data(float metallic, float roughness, float alpha);

  bool is_transparent();
};

void get_material_image(Object *ob,
                        int material_index,
                        ::Image *&image,
                        ImageUser *&iuser,
                        GPUSamplerState &sampler_state);

struct SceneState {
  Scene *scene = nullptr;

  Object *camera_object = nullptr;
  Camera *camera = nullptr;
  float4x4 view_projection_matrix = float4x4::identity();
  int2 resolution = int2(0);

  eContextObjectMode object_mode = CTX_MODE_OBJECT;

  View3DShading shading = {};
  eLightingType lighting_type = eLightingType::STUDIO;
  bool xray_mode = false;

  DRWState cull_state = DRW_STATE_NO_DRAW;
  Vector<float4> clip_planes = {};

  float4 background_color = float4(0);

  bool draw_cavity = false;
  bool draw_curvature = false;
  bool draw_shadows = false;
  bool draw_outline = false;
  bool draw_dof = false;
  bool draw_aa = false;

  bool draw_object_id = false;

  int sample = 0;
  int samples_len = 0;
  bool reset_taa_next_sample = false;
  bool render_finished = false;

  bool overlays_enabled = false;

  /* Used when material_type == eMaterialType::SINGLE */
  Material material_override = Material(float3(1.0f));
  /* When r == -1.0 the shader uses the vertex color */
  Material material_attribute_color = Material(float3(-1.0f));

  void init(Object *camera_ob = nullptr);
};

struct ObjectState {
  eV3DShadingColorType color_type = V3D_SHADING_SINGLE_COLOR;
  bool sculpt_pbvh = false;
  ::Image *image_paint_override = nullptr;
  GPUSamplerState override_sampler_state = GPUSamplerState::default_sampler();
  bool draw_shadow = false;
  bool use_per_material_batches = false;

  ObjectState(const SceneState &scene_state, Object *ob);
};

struct SceneResources {
  static const int jitter_tx_size = 64;

  StringRefNull current_matcap = {};
  Texture matcap_tx = "matcap_tx";

  TextureFromPool object_id_tx = "wb_object_id_tx";

  TextureRef color_tx;
  TextureRef depth_tx;
  TextureRef depth_in_front_tx;

  Framebuffer clear_fb = {"Clear Main"};
  Framebuffer clear_in_front_fb = {"Clear In Front"};

  StorageVectorBuffer<Material> material_buf = {"material_buf"};
  UniformBuffer<WorldData> world_buf = {};
  UniformArrayBuffer<float4, 6> clip_planes_buf;

  Texture jitter_tx = "wb_jitter_tx";

  void init(const SceneState &scene_state);
  void load_jitter_tx(int total_samples);
};

class MeshPass : public PassMain {
 private:
  using TextureSubPassKey = std::pair<GPUTexture *, eGeometryType>;

  Map<TextureSubPassKey, PassMain::Sub *> texture_subpass_map_ = {};

  PassMain::Sub *passes_[geometry_type_len][shader_type_len] = {{nullptr}};

  bool is_empty_ = false;

 public:
  MeshPass(const char *name);

  /* TODO: Move to draw::Pass */
  bool is_empty() const;

  void init_pass(SceneResources &resources, DRWState state, int clip_planes);
  void init_subpasses(ePipelineType pipeline, eLightingType lighting, bool clip);

  PassMain::Sub &get_subpass(eGeometryType geometry_type,
                             ::Image *image = nullptr,
                             GPUSamplerState sampler_state = GPUSamplerState::default_sampler(),
                             ImageUser *iuser = nullptr);
};

enum class StencilBits : uint8_t {
  BACKGROUND = 0,
  OBJECT = 1u << 0,
  OBJECT_IN_FRONT = 1u << 1,
};

class OpaquePass {
 public:
  PassSimple forward_ps_ = {"Opaque.Forward"};
  Framebuffer forward_fb = {"Opaque.Forward"};
  Framebuffer clear_fb = {"Opaque.Clear"};

  void sync(const SceneState &scene_state, SceneResources &resources);
  void draw(Manager &manager,
            View &view,
            SceneResources &resources,
            int2 resolution,
            class ShadowPass *shadow_pass);
  bool is_empty() const;
};

class TransparentPass {
 public:
  TextureFromPool accumulation_tx = {"accumulation_accumulation_tx"};
  TextureFromPool reveal_tx = {"accumulation_reveal_tx"};
  Framebuffer transparent_fb = {};

  MeshPass accumulation_ps_ = {"Transparent.Accumulation"};
  MeshPass accumulation_in_front_ps_ = {"Transparent.AccumulationInFront"};
  PassSimple resolve_ps_ = {"Transparent.Resolve"};
  Framebuffer resolve_fb = {};

  void sync(const SceneState &scene_state, SceneResources &resources);
  void draw(Manager &manager, View &view, SceneResources &resources, int2 resolution);
  bool is_empty() const;
};

#define DEBUG_SHADOW_VOLUME 0

}  // namespace blender::fast64

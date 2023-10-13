/* SPDX-FileCopyrightText: 2023 Blender Developers
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Animation data-block functionality.
 */
#pragma once

#ifndef __cplusplus
#  error This is a C++ header.
#endif

#include "DNA_anim_types.h"

#include "BLI_set.hh"

struct AnimationEvalContext;
struct FCurve;
struct ID;
struct PointerRNA;

namespace blender::animrig {

class Layer;
class Strip;
class Output;

/* Use an alias for the stable index type. */
using output_index_t = decltype(::AnimationOutput::stable_index);

class Animation : public ::Animation {
 public:
  Animation() = default;
  Animation(const Animation &other) = default;
  ~Animation() = default;

  /* Animation Layers access. */
  blender::Span<const Layer *> layers() const;
  blender::MutableSpan<Layer *> layers();
  const Layer *layer(int64_t index) const;
  Layer *layer(int64_t index);

  Layer *layer_add(const char *name);

  /* Animation Output access. */
  blender::Span<const Output *> outputs() const;
  blender::MutableSpan<Output *> outputs();
  const Output *output(int64_t index) const;
  Output *output(int64_t index);

  Output *output_for_stable_index(output_index_t stable_index);
  Output *output_for_fallback(const char *fallback);

  Output *output_add();
  bool assign_id(Output &output, ID *animated_id);
  void unassign_id(ID *animated_id);

  /* Find the output with the same stable index.
   * If that is not available, use the fallback string. */
  Output *find_suitable_output_for(const ID *animated_id);

 private:
  Output &output_allocate_();
};
static_assert(sizeof(Animation) == sizeof(::Animation),
              "DNA struct and its C++ wrapper must have the same size");

class Layer : public ::AnimationLayer {
 public:
  Layer() = default;
  Layer(const Layer &other) = default;
  ~Layer() = default;

  /* Strip access. */
  blender::Span<const Strip *> strips() const;
  blender::MutableSpan<Strip *> strips();
  const Strip *strip(int64_t index) const;
  Strip *strip(int64_t index);

  Strip *strip_add(eAnimationStrip_type strip_type);
};
static_assert(sizeof(Layer) == sizeof(::AnimationLayer),
              "DNA struct and its C++ wrapper must have the same size");

class Output : public ::AnimationOutput {
 public:
  Output() = default;
  Output(const Output &other) = default;
  ~Output() = default;

  /**
   * Assign the ID to this Output.
   *
   * \return Whether this was possible. If the Output was already bound to a
   * specific ID type, and `animated_id` is of a different type, it will be
   * refused. If the ID type cannot be animated at all, false is also returned.
   */
  bool assign_id(ID *animated_id);

  bool is_suitable_for(const ID *animated_id) const;
};
static_assert(sizeof(Output) == sizeof(::AnimationOutput),
              "DNA struct and its C++ wrapper must have the same size");

class Output_runtime {
 public:
  Set<ID *> ids;
};

class Strip : public ::AnimationStrip {
 public:
  Strip() = default;
  Strip(const Strip &other) = default;
  ~Strip() = default;

  // TODO: add? Maybe?
  // template<typename T> bool is() const;
  template<typename T> T &as();

  bool contains_frame(float frame_time) const;
};
static_assert(sizeof(Strip) == sizeof(::AnimationStrip),
              "DNA struct and its C++ wrapper must have the same size");

class KeyframeStrip : public ::KeyframeAnimationStrip {
 public:
  KeyframeStrip() = default;
  KeyframeStrip(const KeyframeStrip &other) = default;
  ~KeyframeStrip() = default;

  /* Strip access. */
  blender::Span<const ChannelsForOutput *> channels_for_output() const;
  blender::MutableSpan<ChannelsForOutput *> channels_for_output();
  const ChannelsForOutput *channel_for_output(int64_t index) const;
  ChannelsForOutput *channel_for_output(int64_t index);

  /**
   * Find the animation channels for this output.
   *
   * \return nullptr if there is none yet for this output.
   */
  const ChannelsForOutput *chans_for_out(const Output &out) const;
  ChannelsForOutput *chans_for_out(const Output &out);
  const ChannelsForOutput *chans_for_out(output_index_t output_stable_index) const;
  ChannelsForOutput *chans_for_out(output_index_t output_stable_index);

  /**
   * Add the animation channels for this output.
   *
   * Should only be called when there is no `ChannelsForOutput` for this output yet.
   */
  ChannelsForOutput *chans_for_out_add(const Output &out);

  /**
   * Find an FCurve for this output + RNA path + array index combination.
   *
   * If it cannot be found, `nullptr` is returned.
   */
  FCurve *fcurve_find(const Output &out, const char *rna_path, int array_index);

  /**
   * Find an FCurve for this output + RNA path + array index combination.
   *
   * If it cannot be found, a new one is created.
   */
  FCurve *fcurve_find_or_create(const Output &out, const char *rna_path, int array_index);
};
static_assert(sizeof(KeyframeStrip) == sizeof(::KeyframeAnimationStrip),
              "DNA struct and its C++ wrapper must have the same size");

template<> KeyframeStrip &Strip::as<KeyframeStrip>();

class ChannelsForOutput : public ::AnimationChannelsForOutput {
 public:
  ChannelsForOutput() = default;
  ChannelsForOutput(const ChannelsForOutput &other) = default;
  ~ChannelsForOutput() = default;

  /* FCurves access. */
  blender::Span<const FCurve *> fcurves() const;
  blender::MutableSpan<FCurve *> fcurves();
  const FCurve *fcurve(int64_t index) const;
  FCurve *fcurve(int64_t index);
};
static_assert(sizeof(ChannelsForOutput) == sizeof(::AnimationChannelsForOutput),
              "DNA struct and its C++ wrapper must have the same size");

FCurve *keyframe_insert(KeyframeStrip &key_strip,
                        const Output &out,
                        const char *rna_path,
                        int array_index,
                        float value,
                        float time,
                        eBezTriple_KeyframeType keytype);

/**
 * Assign the animation to the ID.
 *
 * This will will make a best-effort guess as to which output to use, in this
 * order;
 *
 * - By stable index.
 * - By fallback string.
 * - Add a new Output for this ID.
 *
 * \return false if the assignment was not possible.
 */
bool assign_animation(Animation &anim, ID *animated_id);

/**
 * Ensure that this ID is no longer animated.
 */
void unassign_animation(ID *animated_id);

}  // namespace blender::animrig

/* Wrap functions for the DNA structs. */

inline blender::animrig::Animation &Animation::wrap()
{
  return *reinterpret_cast<blender::animrig::Animation *>(this);
}
inline const blender::animrig::Animation &Animation::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Animation *>(this);
}

inline blender::animrig::Layer &AnimationLayer::wrap()
{
  return *reinterpret_cast<blender::animrig::Layer *>(this);
}
inline const blender::animrig::Layer &AnimationLayer::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Layer *>(this);
}

inline blender::animrig::Output &AnimationOutput::wrap()
{
  return *reinterpret_cast<blender::animrig::Output *>(this);
}
inline const blender::animrig::Output &AnimationOutput::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Output *>(this);
}

inline blender::animrig::Strip &AnimationStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::Strip *>(this);
}
inline const blender::animrig::Strip &AnimationStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::Strip *>(this);
}

inline blender::animrig::KeyframeStrip &KeyframeAnimationStrip::wrap()
{
  return *reinterpret_cast<blender::animrig::KeyframeStrip *>(this);
}
inline const blender::animrig::KeyframeStrip &KeyframeAnimationStrip::wrap() const
{
  return *reinterpret_cast<const blender::animrig::KeyframeStrip *>(this);
}

inline blender::animrig::ChannelsForOutput &AnimationChannelsForOutput::wrap()
{
  return *reinterpret_cast<blender::animrig::ChannelsForOutput *>(this);
}
inline const blender::animrig::ChannelsForOutput &AnimationChannelsForOutput::wrap() const
{
  return *reinterpret_cast<const blender::animrig::ChannelsForOutput *>(this);
}
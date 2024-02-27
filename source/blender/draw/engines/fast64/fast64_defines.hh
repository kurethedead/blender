/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fast64
 *
 * List of defines that are shared with the GPUShaderCreateInfos. We do this to avoid
 * dragging larger headers into the createInfo pipeline which would cause problems.
 */

#ifndef GPU_SHADER
#  pragma once
#endif

/* Film. */
#define FILM_GROUP_SIZE 16

/* Uniform Buffers. */
/* Slot 0 is GPU_NODE_TREE_UBO_SLOT. */
#define UNIFORM_BUF_SLOT 1

/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>

#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

/****************************************************************************
 *  Exports
 ****************************************************************************/

/****************************************************************************
 *
 ****************************************************************************/
#define yv12_align_addr(addr, align) \
  (void *)(((size_t)(addr) + ((align)-1)) & (size_t) - (align))

// TODO(jkoleszar): Maybe replace this with struct aom_image

int aom_free_frame_buffer(YV12_BUFFER_CONFIG *ybf) {
  if (ybf) {
    if (ybf->buffer_alloc_sz > 0) {
      aom_free(ybf->buffer_alloc);
    }
    if (ybf->y_buffer_8bit) aom_free(ybf->y_buffer_8bit);

    /* buffer_alloc isn't accessed by most functions.  Rather y_buffer,
      u_buffer and v_buffer point to buffer_alloc and are used.  Clear out
      all of this so that a freed pointer isn't inadvertently used */
    memset(ybf, 0, sizeof(YV12_BUFFER_CONFIG));
  } else {
    return -1;
  }

  return 0;
}

int aom_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                             int ss_x, int ss_y, int use_highbitdepth,
                             int border, int byte_alignment,
                             aom_codec_frame_buffer_t *fb,
                             aom_get_frame_buffer_cb_fn_t cb, void *cb_priv) {
  if (ybf) {
    const int aom_byte_align = (byte_alignment == 0) ? 1 : byte_alignment;
    const int aligned_width = (width + 7) & ~7;
    const int aligned_height = (height + 7) & ~7;
    const int y_stride = ((aligned_width + 2 * border) + 31) & ~31;
    const uint64_t yplane_size =
        (aligned_height + 2 * border) * (uint64_t)y_stride + byte_alignment;
    const int uv_width = aligned_width >> ss_x;
    const int uv_height = aligned_height >> ss_y;
    const int uv_stride = y_stride >> ss_x;
    const int uv_border_w = border >> ss_x;
    const int uv_border_h = border >> ss_y;
    const uint64_t uvplane_size =
        (uv_height + 2 * uv_border_h) * (uint64_t)uv_stride + byte_alignment;

    const uint64_t frame_size =
        (1 + use_highbitdepth) * (yplane_size + 2 * uvplane_size);

    uint8_t *buf = NULL;

    if (cb != NULL) {
      const int align_addr_extra_size = 31;
      const uint64_t external_frame_size = frame_size + align_addr_extra_size;

      assert(fb != NULL);

      if (external_frame_size != (size_t)external_frame_size) return -1;

      // Allocation to hold larger frame, or first allocation.
      if (cb(cb_priv, (size_t)external_frame_size, fb) < 0) return -1;

      if (fb->data == NULL || fb->size < external_frame_size) return -1;

      ybf->buffer_alloc = (uint8_t *)yv12_align_addr(fb->data, 32);

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
      // This memset is needed for fixing the issue of using uninitialized
      // value in msan test. It will cause a perf loss, so only do this for
      // msan test.
      memset(ybf->buffer_alloc, 0, (int)frame_size);
#endif
#endif
    } else if (frame_size > (size_t)ybf->buffer_alloc_sz) {
      // Allocation to hold larger frame, or first allocation.
      aom_free(ybf->buffer_alloc);
      ybf->buffer_alloc = NULL;

      if (frame_size != (size_t)frame_size) return -1;

      ybf->buffer_alloc = (uint8_t *)aom_memalign(32, (size_t)frame_size);
      if (!ybf->buffer_alloc) return -1;

      ybf->buffer_alloc_sz = (size_t)frame_size;

      // This memset is needed for fixing valgrind error from C loop filter
      // due to access uninitialized memory in frame border. It could be
      // removed if border is totally removed.
      memset(ybf->buffer_alloc, 0, ybf->buffer_alloc_sz);
    }

    /* Only support allocating buffers that have a border that's a multiple
     * of 32. The border restriction is required to get 16-byte alignment of
     * the start of the chroma rows without introducing an arbitrary gap
     * between planes, which would break the semantics of things like
     * aom_img_set_rect(). */
    if (border & 0x1f) return -3;

    ybf->y_crop_width = width;
    ybf->y_crop_height = height;
    ybf->y_width = aligned_width;
    ybf->y_height = aligned_height;
    ybf->y_stride = y_stride;

    ybf->uv_crop_width = (width + ss_x) >> ss_x;
    ybf->uv_crop_height = (height + ss_y) >> ss_y;
    ybf->uv_width = uv_width;
    ybf->uv_height = uv_height;
    ybf->uv_stride = uv_stride;

    ybf->border = border;
    ybf->frame_size = (size_t)frame_size;
    ybf->subsampling_x = ss_x;
    ybf->subsampling_y = ss_y;

    buf = ybf->buffer_alloc;
    if (use_highbitdepth) {
      // Store uint16 addresses when using 16bit framebuffers
      buf = CONVERT_TO_BYTEPTR(ybf->buffer_alloc);
      ybf->flags = YV12_FLAG_HIGHBITDEPTH;
    } else {
      ybf->flags = 0;
    }

    ybf->y_buffer = (uint8_t *)yv12_align_addr(
        buf + (border * y_stride) + border, aom_byte_align);
    ybf->u_buffer = (uint8_t *)yv12_align_addr(
        buf + yplane_size + (uv_border_h * uv_stride) + uv_border_w,
        aom_byte_align);
    ybf->v_buffer =
        (uint8_t *)yv12_align_addr(buf + yplane_size + uvplane_size +
                                       (uv_border_h * uv_stride) + uv_border_w,
                                   aom_byte_align);

    ybf->use_external_refernce_buffers = 0;

    if (use_highbitdepth) {
      if (ybf->y_buffer_8bit) aom_free(ybf->y_buffer_8bit);
      ybf->y_buffer_8bit = (uint8_t *)aom_memalign(32, (size_t)yplane_size);
      if (!ybf->y_buffer_8bit) return -1;
    } else {
      assert(!ybf->y_buffer_8bit);
    }

    ybf->corrupted = 0; /* assume not corrupted by errors */
    return 0;
  }
  return -2;
}

int aom_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                           int ss_x, int ss_y, int use_highbitdepth, int border,
                           int byte_alignment) {
  if (ybf) {
    aom_free_frame_buffer(ybf);
    return aom_realloc_frame_buffer(ybf, width, height, ss_x, ss_y,
                                    use_highbitdepth, border, byte_alignment,
                                    NULL, NULL, NULL);
  }
  return -2;
}

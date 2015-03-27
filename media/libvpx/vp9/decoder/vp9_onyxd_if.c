/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include "vp9/common/vp9_onyxc_int.h"
#if CONFIG_VP9_POSTPROC
#include "vp9/common/vp9_postproc.h"
#endif
#include "vp9/decoder/vp9_onyxd.h"
#include "vp9/decoder/vp9_onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "vp9/common/vp9_alloccommon.h"
#include "vp9/common/vp9_loopfilter.h"
#include "vp9/common/vp9_quant_common.h"
#include "vpx_scale/vpx_scale.h"
#include "vp9/common/vp9_systemdependent.h"
#include "vpx_ports/vpx_timer.h"
#include "vp9/decoder/vp9_decodeframe.h"
#include "vp9/decoder/vp9_detokenize.h"
#include "vp9/decoder/vp9_dthread.h"
#include "./vpx_scale_rtcd.h"

#define WRITE_RECON_BUFFER 0
#if WRITE_RECON_BUFFER == 1
static void recon_write_yuv_frame(const char *name,
                                  const YV12_BUFFER_CONFIG *s,
                                  int w, int _h) {
  FILE *yuv_file = fopen(name, "ab");
  const uint8_t *src = s->y_buffer;
  int h = _h;

  do {
    fwrite(src, w, 1,  yuv_file);
    src += s->y_stride;
  } while (--h);

  src = s->u_buffer;
  h = (_h + 1) >> 1;
  w = (w + 1) >> 1;

  do {
    fwrite(src, w, 1,  yuv_file);
    src += s->uv_stride;
  } while (--h);

  src = s->v_buffer;
  h = (_h + 1) >> 1;

  do {
    fwrite(src, w, 1, yuv_file);
    src += s->uv_stride;
  } while (--h);

  fclose(yuv_file);
}
#endif
#if WRITE_RECON_BUFFER == 2
void write_dx_frame_to_file(YV12_BUFFER_CONFIG *frame, int this_frame) {
  // write the frame
  FILE *yframe;
  int i;
  char filename[255];

  snprintf(filename, sizeof(filename)-1, "dx\\y%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->y_height; i++)
    fwrite(frame->y_buffer + i * frame->y_stride,
           frame->y_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename)-1, "dx\\u%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->u_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
  snprintf(filename, sizeof(filename)-1, "dx\\v%04d.raw", this_frame);
  yframe = fopen(filename, "wb");

  for (i = 0; i < frame->uv_height; i++)
    fwrite(frame->v_buffer + i * frame->uv_stride,
           frame->uv_width, 1, yframe);

  fclose(yframe);
}
#endif

void vp9_initialize_dec() {
  static int init_done = 0;

  if (!init_done) {
    vp9_initialize_common();
    vp9_init_quant_tables();
    init_done = 1;
  }
}

static void init_macroblockd(VP9D_COMP *const pbi) {
  MACROBLOCKD *xd = &pbi->mb;
  struct macroblockd_plane *const pd = xd->plane;
  int i;

  for (i = 0; i < MAX_MB_PLANE; ++i)
    pd[i].dqcoeff = pbi->dqcoeff[i];
}

VP9D_PTR vp9_create_decompressor(VP9D_CONFIG *oxcf) {
  VP9D_COMP *const pbi = vpx_memalign(32, sizeof(VP9D_COMP));
  VP9_COMMON *const cm = pbi ? &pbi->common : NULL;

  if (!cm)
    return NULL;

  vp9_zero(*pbi);

  // Initialize the references to not point to any frame buffers.
  memset(&cm->ref_frame_map, -1, sizeof(cm->ref_frame_map));

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;
    vp9_remove_decompressor(pbi);
    return NULL;
  }

  cm->error.setjmp = 1;
  vp9_initialize_dec();

  vp9_create_common(cm);

  pbi->oxcf = *oxcf;
  pbi->ready_for_new_data = 1;
  cm->current_video_frame = 0;

  // vp9_init_dequantizer() is first called here. Add check in
  // frame_init_dequantizer() to avoid unnecessary calling of
  // vp9_init_dequantizer() for every frame.
  vp9_init_dequantizer(cm);

  vp9_loop_filter_init(cm);

  cm->error.setjmp = 0;
  pbi->decoded_key_frame = 0;

  init_macroblockd(pbi);

  vp9_worker_init(&pbi->lf_worker);

  return pbi;
}

void vp9_remove_decompressor(VP9D_PTR ptr) {
  int i;
  VP9D_COMP *const pbi = (VP9D_COMP *)ptr;

  if (!pbi)
    return;

  vp9_remove_common(&pbi->common);
  vp9_worker_end(&pbi->lf_worker);
  vpx_free(pbi->lf_worker.data1);
  for (i = 0; i < pbi->num_tile_workers; ++i) {
    VP9Worker *const worker = &pbi->tile_workers[i];
    vp9_worker_end(worker);
    vpx_free(worker->data1);
    vpx_free(worker->data2);
  }
  vpx_free(pbi->tile_workers);

  if (pbi->num_tile_workers) {
    VP9_COMMON *const cm = &pbi->common;
    const int sb_rows =
        mi_cols_aligned_to_sb(cm->mi_rows) >> MI_BLOCK_SIZE_LOG2;
    VP9LfSync *const lf_sync = &pbi->lf_row_sync;

    vp9_loop_filter_dealloc(lf_sync, sb_rows);
  }

  vpx_free(pbi->mi_streams);
  vpx_free(pbi->above_context[0]);
  vpx_free(pbi->above_seg_context);
  vpx_free(pbi);
}

static int equal_dimensions(const YV12_BUFFER_CONFIG *a,
                            const YV12_BUFFER_CONFIG *b) {
    return a->y_height == b->y_height && a->y_width == b->y_width &&
           a->uv_height == b->uv_height && a->uv_width == b->uv_width;
}

vpx_codec_err_t vp9_copy_reference_dec(VP9D_PTR ptr,
                                       VP9_REFFRAME ref_frame_flag,
                                       YV12_BUFFER_CONFIG *sd) {
  VP9D_COMP *pbi = (VP9D_COMP *) ptr;
  VP9_COMMON *cm = &pbi->common;

  /* TODO(jkoleszar): The decoder doesn't have any real knowledge of what the
   * encoder is using the frame buffers for. This is just a stub to keep the
   * vpxenc --test-decode functionality working, and will be replaced in a
   * later commit that adds VP9-specific controls for this functionality.
   */
  if (ref_frame_flag == VP9_LAST_FLAG) {
    const YV12_BUFFER_CONFIG *const cfg =
        &cm->frame_bufs[cm->ref_frame_map[0]].buf;
    if (!equal_dimensions(cfg, sd))
      vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                         "Incorrect buffer dimensions");
    else
      vp8_yv12_copy_frame(cfg, sd);
  } else {
    vpx_internal_error(&cm->error, VPX_CODEC_ERROR,
                       "Invalid reference frame");
  }

  return cm->error.error_code;
}


vpx_codec_err_t vp9_set_reference_dec(VP9D_PTR ptr, VP9_REFFRAME ref_frame_flag,
                                      YV12_BUFFER_CONFIG *sd) {
  VP9D_COMP *pbi = (VP9D_COMP *) ptr;
  VP9_COMMON *cm = &pbi->common;
  RefBuffer *ref_buf = NULL;

  /* TODO(jkoleszar): The decoder doesn't have any real knowledge of what the
   * encoder is using the frame buffers for. This is just a stub to keep the
   * vpxenc --test-decode functionality working, and will be replaced in a
   * later commit that adds VP9-specific controls for this functionality.
   */
  if (ref_frame_flag == VP9_LAST_FLAG) {
    ref_buf = &cm->frame_refs[0];
  } else if (ref_frame_flag == VP9_GOLD_FLAG) {
    ref_buf = &cm->frame_refs[1];
  } else if (ref_frame_flag == VP9_ALT_FLAG) {
    ref_buf = &cm->frame_refs[2];
  } else {
    vpx_internal_error(&pbi->common.error, VPX_CODEC_ERROR,
                       "Invalid reference frame");
    return pbi->common.error.error_code;
  }

  if (!equal_dimensions(ref_buf->buf, sd)) {
    vpx_internal_error(&pbi->common.error, VPX_CODEC_ERROR,
                       "Incorrect buffer dimensions");
  } else {
    int *ref_fb_ptr = &ref_buf->idx;

    // Find an empty frame buffer.
    const int free_fb = get_free_fb(cm);
    // Decrease ref_count since it will be increased again in
    // ref_cnt_fb() below.
    cm->frame_bufs[free_fb].ref_count--;

    // Manage the reference counters and copy image.
    ref_cnt_fb(cm->frame_bufs, ref_fb_ptr, free_fb);
    ref_buf->buf = &cm->frame_bufs[*ref_fb_ptr].buf;
    vp8_yv12_copy_frame(sd, ref_buf->buf);
  }

  return pbi->common.error.error_code;
}


int vp9_get_reference_dec(VP9D_PTR ptr, int index, YV12_BUFFER_CONFIG **fb) {
  VP9D_COMP *pbi = (VP9D_COMP *) ptr;
  VP9_COMMON *cm = &pbi->common;

  if (index < 0 || index >= REF_FRAMES)
    return -1;

  *fb = &cm->frame_bufs[cm->ref_frame_map[index]].buf;
  return 0;
}

/* If any buffer updating is signaled it should be done here. */
static void swap_frame_buffers(VP9D_COMP *pbi) {
  int ref_index = 0, mask;
  VP9_COMMON *const cm = &pbi->common;

  for (mask = pbi->refresh_frame_flags; mask; mask >>= 1) {
    if (mask & 1) {
      const int old_idx = cm->ref_frame_map[ref_index];
      ref_cnt_fb(cm->frame_bufs, &cm->ref_frame_map[ref_index],
                 cm->new_fb_idx);
      if (old_idx >= 0 && cm->frame_bufs[old_idx].ref_count == 0)
        cm->release_fb_cb(cm->cb_priv,
                          &cm->frame_bufs[old_idx].raw_frame_buffer);
    }
    ++ref_index;
  }

  cm->frame_to_show = get_frame_new_buffer(cm);
  cm->frame_bufs[cm->new_fb_idx].ref_count--;

  // Invalidate these references until the next frame starts.
  for (ref_index = 0; ref_index < 3; ref_index++)
    cm->frame_refs[ref_index].idx = INT_MAX;
}

int vp9_receive_compressed_data(VP9D_PTR ptr,
                                size_t size, const uint8_t **psource,
                                int64_t time_stamp) {
  VP9D_COMP *pbi = (VP9D_COMP *) ptr;
  VP9_COMMON *cm = &pbi->common;
  const uint8_t *source = *psource;
  int retcode = 0;

  /*if(pbi->ready_for_new_data == 0)
      return -1;*/

  if (ptr == 0)
    return -1;

  cm->error.error_code = VPX_CODEC_OK;

  pbi->source = source;
  pbi->source_sz = size;

  if (pbi->source_sz == 0) {
    /* This is used to signal that we are missing frames.
     * We do not know if the missing frame(s) was supposed to update
     * any of the reference buffers, but we act conservative and
     * mark only the last buffer as corrupted.
     *
     * TODO(jkoleszar): Error concealment is undefined and non-normative
     * at this point, but if it becomes so, [0] may not always be the correct
     * thing to do here.
     */
    if (cm->frame_refs[0].idx != INT_MAX)
      cm->frame_refs[0].buf->corrupted = 1;
  }

  // Check if the previous frame was a frame without any references to it.
  if (cm->new_fb_idx >= 0 && cm->frame_bufs[cm->new_fb_idx].ref_count == 0)
    cm->release_fb_cb(cm->cb_priv,
                      &cm->frame_bufs[cm->new_fb_idx].raw_frame_buffer);
  cm->new_fb_idx = get_free_fb(cm);

  if (setjmp(cm->error.jmp)) {
    cm->error.setjmp = 0;

    /* We do not know if the missing frame(s) was supposed to update
     * any of the reference buffers, but we act conservative and
     * mark only the last buffer as corrupted.
     *
     * TODO(jkoleszar): Error concealment is undefined and non-normative
     * at this point, but if it becomes so, [0] may not always be the correct
     * thing to do here.
     */
    if (cm->frame_refs[0].idx != INT_MAX)
      cm->frame_refs[0].buf->corrupted = 1;

    if (cm->frame_bufs[cm->new_fb_idx].ref_count > 0)
      cm->frame_bufs[cm->new_fb_idx].ref_count--;

    return -1;
  }

  cm->error.setjmp = 1;

  retcode = vp9_decode_frame(pbi, psource);

  if (retcode < 0) {
    cm->error.error_code = VPX_CODEC_ERROR;
    cm->error.setjmp = 0;
    if (cm->frame_bufs[cm->new_fb_idx].ref_count > 0)
      cm->frame_bufs[cm->new_fb_idx].ref_count--;
    return retcode;
  }

  swap_frame_buffers(pbi);

#if WRITE_RECON_BUFFER == 2
  if (cm->show_frame)
    write_dx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame);
  else
    write_dx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 1000);
#endif

  if (!pbi->do_loopfilter_inline) {
    // If multiple threads are used to decode tiles, then we use those threads
    // to do parallel loopfiltering.
    if (pbi->num_tile_workers) {
      vp9_loop_filter_frame_mt(pbi, cm, &pbi->mb, cm->lf.filter_level, 0, 0);
    } else {
      vp9_loop_filter_frame(cm, &pbi->mb, cm->lf.filter_level, 0, 0);
    }
  }

#if WRITE_RECON_BUFFER == 2
  if (cm->show_frame)
    write_dx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 2000);
  else
    write_dx_frame_to_file(cm->frame_to_show,
                           cm->current_video_frame + 3000);
#endif

#if WRITE_RECON_BUFFER == 1
  if (cm->show_frame)
    recon_write_yuv_frame("recon.yuv", cm->frame_to_show,
                          cm->width, cm->height);
#endif

  vp9_clear_system_state();

  cm->last_width = cm->width;
  cm->last_height = cm->height;

  if (!cm->show_existing_frame)
    cm->last_show_frame = cm->show_frame;
  if (cm->show_frame) {
    if (!cm->show_existing_frame) {
      // current mip will be the prev_mip for the next frame
      MODE_INFO *temp = cm->prev_mip;
      MODE_INFO **temp2 = cm->prev_mi_grid_base;
      cm->prev_mip = cm->mip;
      cm->mip = temp;
      cm->prev_mi_grid_base = cm->mi_grid_base;
      cm->mi_grid_base = temp2;

      // update the upper left visible macroblock ptrs
      cm->mi = cm->mip + cm->mode_info_stride + 1;
      cm->prev_mi = cm->prev_mip + cm->mode_info_stride + 1;
      cm->mi_grid_visible = cm->mi_grid_base + cm->mode_info_stride + 1;
      cm->prev_mi_grid_visible = cm->prev_mi_grid_base +
                                 cm->mode_info_stride + 1;

      pbi->mb.mi_8x8 = cm->mi_grid_visible;
      pbi->mb.mi_8x8[0] = cm->mi;
    }
    cm->current_video_frame++;
  }

  pbi->ready_for_new_data = 0;
  pbi->last_time_stamp = time_stamp;
  pbi->source_sz = 0;

  cm->error.setjmp = 0;
  return retcode;
}

int vp9_get_raw_frame(VP9D_PTR ptr, YV12_BUFFER_CONFIG *sd,
                      int64_t *time_stamp, int64_t *time_end_stamp,
                      vp9_ppflags_t *flags) {
  int ret = -1;
  VP9D_COMP *pbi = (VP9D_COMP *) ptr;

  if (pbi->ready_for_new_data == 1)
    return ret;

  /* ie no raw frame to show!!! */
  if (pbi->common.show_frame == 0)
    return ret;

  pbi->ready_for_new_data = 1;
  *time_stamp = pbi->last_time_stamp;
  *time_end_stamp = 0;

#if CONFIG_VP9_POSTPROC
  ret = vp9_post_proc_frame(&pbi->common, sd, flags);
#else

  if (pbi->common.frame_to_show) {
    *sd = *pbi->common.frame_to_show;
    sd->y_width = pbi->common.width;
    sd->y_height = pbi->common.height;
    sd->uv_width = sd->y_width >> pbi->common.subsampling_x;
    sd->uv_height = sd->y_height >> pbi->common.subsampling_y;

    ret = 0;
  } else {
    ret = -1;
  }

#endif /*!CONFIG_POSTPROC*/
  vp9_clear_system_state();
  return ret;
}

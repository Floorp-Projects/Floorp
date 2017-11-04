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

#ifndef AV1_DECODER_DECODER_H_
#define AV1_DECODER_DECODER_H_

#include "./aom_config.h"

#include "aom/aom_codec.h"
#include "aom_dsp/bitreader.h"
#include "aom_scale/yv12config.h"
#include "aom_util/aom_thread.h"

#include "av1/common/thread_common.h"
#include "av1/common/onyxc_int.h"
#include "av1/decoder/dthread.h"
#if CONFIG_ACCOUNTING
#include "av1/decoder/accounting.h"
#endif
#if CONFIG_INSPECTION
#include "av1/decoder/inspection.h"
#endif

#if CONFIG_PVQ
#include "aom_dsp/entdec.h"
#include "av1/decoder/decint.h"
#include "av1/encoder/encodemb.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// TODO(hkuang): combine this with TileWorkerData.
typedef struct TileData {
  AV1_COMMON *cm;
  aom_reader bit_reader;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
  /* dqcoeff are shared by all the planes. So planes must be decoded serially */
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[MAX_TX_SQUARE]);
#if CONFIG_PVQ
  /* forward transformed predicted image, a reference for PVQ */
  DECLARE_ALIGNED(16, tran_low_t, pvq_ref_coeff[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
#endif
#if CONFIG_CFL
  CFL_CTX cfl;
#endif
  DECLARE_ALIGNED(16, FRAME_CONTEXT, tctx);
  DECLARE_ALIGNED(16, uint8_t, color_index_map[2][MAX_SB_SQUARE]);
#if CONFIG_MRC_TX
  DECLARE_ALIGNED(16, uint8_t, mrc_mask[MAX_SB_SQUARE]);
#endif  // CONFIG_MRC_TX
} TileData;

typedef struct TileWorkerData {
  struct AV1Decoder *pbi;
  aom_reader bit_reader;
  FRAME_COUNTS counts;
  DECLARE_ALIGNED(16, MACROBLOCKD, xd);
  /* dqcoeff are shared by all the planes. So planes must be decoded serially */
  DECLARE_ALIGNED(16, tran_low_t, dqcoeff[MAX_TX_SQUARE]);
#if CONFIG_PVQ
  /* forward transformed predicted image, a reference for PVQ */
  DECLARE_ALIGNED(16, tran_low_t, pvq_ref_coeff[OD_TXSIZE_MAX * OD_TXSIZE_MAX]);
#endif
#if CONFIG_CFL
  CFL_CTX cfl;
#endif
  FRAME_CONTEXT tctx;
  DECLARE_ALIGNED(16, uint8_t, color_index_map[2][MAX_SB_SQUARE]);
#if CONFIG_MRC_TX
  DECLARE_ALIGNED(16, uint8_t, mrc_mask[MAX_SB_SQUARE]);
#endif  // CONFIG_MRC_TX
  struct aom_internal_error_info error_info;
} TileWorkerData;

typedef struct TileBufferDec {
  const uint8_t *data;
  size_t size;
  const uint8_t *raw_data_end;  // The end of the raw tile buffer in the
                                // bit stream.
  int col;                      // only used with multi-threaded decoding
} TileBufferDec;

typedef struct AV1Decoder {
  DECLARE_ALIGNED(16, MACROBLOCKD, mb);

  DECLARE_ALIGNED(16, AV1_COMMON, common);

  int ready_for_new_data;

  int refresh_frame_flags;

  // TODO(hkuang): Combine this with cur_buf in macroblockd as they are
  // the same.
  RefCntBuffer *cur_buf;  //  Current decoding frame buffer.

  AVxWorker *frame_worker_owner;  // frame_worker that owns this pbi.
  AVxWorker lf_worker;
  AVxWorker *tile_workers;
  TileWorkerData *tile_worker_data;
  TileInfo *tile_worker_info;
  int num_tile_workers;

  TileData *tile_data;
  int allocated_tiles;

  TileBufferDec tile_buffers[MAX_TILE_ROWS][MAX_TILE_COLS];

  AV1LfSync lf_row_sync;

  aom_decrypt_cb decrypt_cb;
  void *decrypt_state;

  int allow_lowbitdepth;
  int max_threads;
  int inv_tile_order;
  int need_resync;   // wait for key/intra-only frame.
  int hold_ref_buf;  // hold the reference buffer.

  int tile_size_bytes;
#if CONFIG_EXT_TILE
  int tile_col_size_bytes;
  int dec_tile_row, dec_tile_col;  // always -1 for non-VR tile encoding
#endif                             // CONFIG_EXT_TILE
#if CONFIG_ACCOUNTING
  int acct_enabled;
  Accounting accounting;
#endif
  size_t uncomp_hdr_size;       // Size of the uncompressed header
  size_t first_partition_size;  // Size of the compressed header
  int tg_size;                  // Number of tiles in the current tilegroup
  int tg_start;                 // First tile in the current tilegroup
  int tg_size_bit_offset;
#if CONFIG_INSPECTION
  aom_inspect_cb inspect_cb;
  void *inspect_ctx;
#endif
} AV1Decoder;

int av1_receive_compressed_data(struct AV1Decoder *pbi, size_t size,
                                const uint8_t **dest);

int av1_get_raw_frame(struct AV1Decoder *pbi, YV12_BUFFER_CONFIG *sd);

int av1_get_frame_to_show(struct AV1Decoder *pbi, YV12_BUFFER_CONFIG *frame);

aom_codec_err_t av1_copy_reference_dec(struct AV1Decoder *pbi, int idx,
                                       YV12_BUFFER_CONFIG *sd);

aom_codec_err_t av1_set_reference_dec(AV1_COMMON *cm, int idx,
                                      YV12_BUFFER_CONFIG *sd);

static INLINE uint8_t read_marker(aom_decrypt_cb decrypt_cb,
                                  void *decrypt_state, const uint8_t *data) {
  if (decrypt_cb) {
    uint8_t marker;
    decrypt_cb(decrypt_state, data, &marker, 1);
    return marker;
  }
  return *data;
}

// This function is exposed for use in tests, as well as the inlined function
// "read_marker".
aom_codec_err_t av1_parse_superframe_index(const uint8_t *data, size_t data_sz,
                                           uint32_t sizes[8], int *count,
                                           int *index_size,
                                           aom_decrypt_cb decrypt_cb,
                                           void *decrypt_state);

struct AV1Decoder *av1_decoder_create(BufferPool *const pool);

void av1_decoder_remove(struct AV1Decoder *pbi);

static INLINE void decrease_ref_count(int idx, RefCntBuffer *const frame_bufs,
                                      BufferPool *const pool) {
  if (idx >= 0) {
    --frame_bufs[idx].ref_count;
    // A worker may only get a free framebuffer index when calling get_free_fb.
    // But the private buffer is not set up until finish decoding header.
    // So any error happens during decoding header, the frame_bufs will not
    // have valid priv buffer.
    if (frame_bufs[idx].ref_count == 0 &&
        frame_bufs[idx].raw_frame_buffer.priv) {
      pool->release_fb_cb(pool->cb_priv, &frame_bufs[idx].raw_frame_buffer);
    }
  }
}

#if CONFIG_EXT_REFS || CONFIG_TEMPMV_SIGNALING
static INLINE int dec_is_ref_frame_buf(AV1Decoder *const pbi,
                                       RefCntBuffer *frame_buf) {
  AV1_COMMON *const cm = &pbi->common;
  int i;
  for (i = 0; i < INTER_REFS_PER_FRAME; ++i) {
    RefBuffer *const ref_frame = &cm->frame_refs[i];
    if (ref_frame->idx == INVALID_IDX) continue;
    if (frame_buf == &cm->buffer_pool->frame_bufs[ref_frame->idx]) break;
  }
  return (i < INTER_REFS_PER_FRAME);
}
#endif  // CONFIG_EXT_REFS

#define ACCT_STR __func__
static INLINE int av1_read_uniform(aom_reader *r, int n) {
  const int l = get_unsigned_bits(n);
  const int m = (1 << l) - n;
  const int v = aom_read_literal(r, l - 1, ACCT_STR);
  assert(l != 0);
  if (v < m)
    return v;
  else
    return (v << 1) - m + aom_read_literal(r, 1, ACCT_STR);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_DECODER_DECODER_H_

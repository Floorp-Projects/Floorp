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

#include <limits.h>
#include <math.h>

#include "av1/common/seg_common.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/segmentation.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/system_state.h"

struct CYCLIC_REFRESH {
  // Percentage of blocks per frame that are targeted as candidates
  // for cyclic refresh.
  int percent_refresh;
  // Maximum q-delta as percentage of base q.
  int max_qdelta_perc;
  // Superblock starting index for cycling through the frame.
  int sb_index;
  // Controls how long block will need to wait to be refreshed again, in
  // excess of the cycle time, i.e., in the case of all zero motion, block
  // will be refreshed every (100/percent_refresh + time_for_refresh) frames.
  int time_for_refresh;
  // Target number of (8x8) blocks that are set for delta-q.
  int target_num_seg_blocks;
  // Actual number of (8x8) blocks that were applied delta-q.
  int actual_num_seg1_blocks;
  int actual_num_seg2_blocks;
  // RD mult. parameters for segment 1.
  int rdmult;
  // Cyclic refresh map.
  int8_t *map;
  // Map of the last q a block was coded at.
  uint8_t *last_coded_q_map;
  // Thresholds applied to the projected rate/distortion of the coding block,
  // when deciding whether block should be refreshed.
  int64_t thresh_rate_sb;
  int64_t thresh_dist_sb;
  // Threshold applied to the motion vector (in units of 1/8 pel) of the
  // coding block, when deciding whether block should be refreshed.
  int16_t motion_thresh;
  // Rate target ratio to set q delta.
  double rate_ratio_qdelta;
  // Boost factor for rate target ratio, for segment CR_SEGMENT_ID_BOOST2.
  int rate_boost_fac;
  double low_content_avg;
  int qindex_delta[3];
};

CYCLIC_REFRESH *av1_cyclic_refresh_alloc(int mi_rows, int mi_cols) {
  size_t last_coded_q_map_size;
  CYCLIC_REFRESH *const cr = aom_calloc(1, sizeof(*cr));
  if (cr == NULL) return NULL;

  cr->map = aom_calloc(mi_rows * mi_cols, sizeof(*cr->map));
  if (cr->map == NULL) {
    av1_cyclic_refresh_free(cr);
    return NULL;
  }
  last_coded_q_map_size = mi_rows * mi_cols * sizeof(*cr->last_coded_q_map);
  cr->last_coded_q_map = aom_malloc(last_coded_q_map_size);
  if (cr->last_coded_q_map == NULL) {
    av1_cyclic_refresh_free(cr);
    return NULL;
  }
  assert(MAXQ <= 255);
  memset(cr->last_coded_q_map, MAXQ, last_coded_q_map_size);

  return cr;
}

void av1_cyclic_refresh_free(CYCLIC_REFRESH *cr) {
  if (cr != NULL) {
    aom_free(cr->map);
    aom_free(cr->last_coded_q_map);
    aom_free(cr);
  }
}

// Check if we should turn off cyclic refresh based on bitrate condition.
static int apply_cyclic_refresh_bitrate(const AV1_COMMON *cm,
                                        const RATE_CONTROL *rc) {
  // Turn off cyclic refresh if bits available per frame is not sufficiently
  // larger than bit cost of segmentation. Segment map bit cost should scale
  // with number of seg blocks, so compare available bits to number of blocks.
  // Average bits available per frame = avg_frame_bandwidth
  // Number of (8x8) blocks in frame = mi_rows * mi_cols;
  const float factor = 0.25;
  const int number_blocks = cm->mi_rows * cm->mi_cols;
  // The condition below corresponds to turning off at target bitrates:
  // (at 30fps), ~12kbps for CIF, 36kbps for VGA, 100kps for HD/720p.
  // Also turn off at very small frame sizes, to avoid too large fraction of
  // superblocks to be refreshed per frame. Threshold below is less than QCIF.
  if (rc->avg_frame_bandwidth < factor * number_blocks ||
      number_blocks / 64 < 5)
    return 0;
  else
    return 1;
}

// Check if this coding block, of size bsize, should be considered for refresh
// (lower-qp coding). Decision can be based on various factors, such as
// size of the coding block (i.e., below min_block size rejected), coding
// mode, and rate/distortion.
static int candidate_refresh_aq(const CYCLIC_REFRESH *cr,
                                const MB_MODE_INFO *mbmi, int64_t rate,
                                int64_t dist, int bsize) {
  MV mv = mbmi->mv[0].as_mv;
  // Reject the block for lower-qp coding if projected distortion
  // is above the threshold, and any of the following is true:
  // 1) mode uses large mv
  // 2) mode is an intra-mode
  // Otherwise accept for refresh.
  if (dist > cr->thresh_dist_sb &&
      (mv.row > cr->motion_thresh || mv.row < -cr->motion_thresh ||
       mv.col > cr->motion_thresh || mv.col < -cr->motion_thresh ||
       !is_inter_block(mbmi)))
    return CR_SEGMENT_ID_BASE;
  else if (bsize >= BLOCK_16X16 && rate < cr->thresh_rate_sb &&
           is_inter_block(mbmi) && mbmi->mv[0].as_int == 0 &&
           cr->rate_boost_fac > 10)
    // More aggressive delta-q for bigger blocks with zero motion.
    return CR_SEGMENT_ID_BOOST2;
  else
    return CR_SEGMENT_ID_BOOST1;
}

// Compute delta-q for the segment.
static int compute_deltaq(const AV1_COMP *cpi, int q, double rate_factor) {
  const CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const RATE_CONTROL *const rc = &cpi->rc;
  int deltaq =
      av1_compute_qdelta_by_rate(rc, cpi->common.frame_type, q, rate_factor,
                                 cpi->common.seq_params.bit_depth);
  if ((-deltaq) > cr->max_qdelta_perc * q / 100) {
    deltaq = -cr->max_qdelta_perc * q / 100;
  }
  return deltaq;
}

// For the just encoded frame, estimate the bits, incorporating the delta-q
// from non-base segment. For now ignore effect of multiple segments
// (with different delta-q). Note this function is called in the postencode
// (called from rc_update_rate_correction_factors()).
int av1_cyclic_refresh_estimate_bits_at_q(const AV1_COMP *cpi,
                                          double correction_factor) {
  const AV1_COMMON *const cm = &cpi->common;
  const CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  int estimated_bits;
  int mbs = cm->MBs;
  int num8x8bl = mbs << 2;
  // Weight for non-base segments: use actual number of blocks refreshed in
  // previous/just encoded frame. Note number of blocks here is in 8x8 units.
  double weight_segment1 = (double)cr->actual_num_seg1_blocks / num8x8bl;
  double weight_segment2 = (double)cr->actual_num_seg2_blocks / num8x8bl;
  // Take segment weighted average for estimated bits.
  estimated_bits =
      (int)((1.0 - weight_segment1 - weight_segment2) *
                av1_estimate_bits_at_q(cm->frame_type, cm->base_qindex, mbs,
                                       correction_factor,
                                       cm->seq_params.bit_depth) +
            weight_segment1 * av1_estimate_bits_at_q(
                                  cm->frame_type,
                                  cm->base_qindex + cr->qindex_delta[1], mbs,
                                  correction_factor, cm->seq_params.bit_depth) +
            weight_segment2 * av1_estimate_bits_at_q(
                                  cm->frame_type,
                                  cm->base_qindex + cr->qindex_delta[2], mbs,
                                  correction_factor, cm->seq_params.bit_depth));
  return estimated_bits;
}

// Prior to encoding the frame, estimate the bits per mb, for a given q = i and
// a corresponding delta-q (for segment 1). This function is called in the
// rc_regulate_q() to set the base qp index.
// Note: the segment map is set to either 0/CR_SEGMENT_ID_BASE (no refresh) or
// to 1/CR_SEGMENT_ID_BOOST1 (refresh) for each superblock, prior to encoding.
int av1_cyclic_refresh_rc_bits_per_mb(const AV1_COMP *cpi, int i,
                                      double correction_factor) {
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  int bits_per_mb;
  int num8x8bl = cm->MBs << 2;
  // Weight for segment prior to encoding: take the average of the target
  // number for the frame to be encoded and the actual from the previous frame.
  double weight_segment =
      (double)((cr->target_num_seg_blocks + cr->actual_num_seg1_blocks +
                cr->actual_num_seg2_blocks) >>
               1) /
      num8x8bl;
  // Compute delta-q corresponding to qindex i.
  int deltaq = compute_deltaq(cpi, i, cr->rate_ratio_qdelta);
  // Take segment weighted average for bits per mb.
  bits_per_mb =
      (int)((1.0 - weight_segment) *
                av1_rc_bits_per_mb(cm->frame_type, i, correction_factor,
                                   cm->seq_params.bit_depth) +
            weight_segment * av1_rc_bits_per_mb(cm->frame_type, i + deltaq,
                                                correction_factor,
                                                cm->seq_params.bit_depth));
  return bits_per_mb;
}

// Prior to coding a given prediction block, of size bsize at (mi_row, mi_col),
// check if we should reset the segment_id, and update the cyclic_refresh map
// and segmentation map.
void av1_cyclic_refresh_update_segment(const AV1_COMP *cpi,
                                       MB_MODE_INFO *const mbmi, int mi_row,
                                       int mi_col, BLOCK_SIZE bsize,
                                       int64_t rate, int64_t dist, int skip) {
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  const int bw = mi_size_wide[bsize];
  const int bh = mi_size_high[bsize];
  const int xmis = AOMMIN(cm->mi_cols - mi_col, bw);
  const int ymis = AOMMIN(cm->mi_rows - mi_row, bh);
  const int block_index = mi_row * cm->mi_cols + mi_col;
  const int refresh_this_block =
      candidate_refresh_aq(cr, mbmi, rate, dist, bsize);
  // Default is to not update the refresh map.
  int new_map_value = cr->map[block_index];
  int x = 0;
  int y = 0;

  // If this block is labeled for refresh, check if we should reset the
  // segment_id.
  if (cyclic_refresh_segment_id_boosted(mbmi->segment_id)) {
    mbmi->segment_id = refresh_this_block;
    // Reset segment_id if will be skipped.
    if (skip) mbmi->segment_id = CR_SEGMENT_ID_BASE;
  }

  // Update the cyclic refresh map, to be used for setting segmentation map
  // for the next frame. If the block  will be refreshed this frame, mark it
  // as clean. The magnitude of the -ve influences how long before we consider
  // it for refresh again.
  if (cyclic_refresh_segment_id_boosted(mbmi->segment_id)) {
    new_map_value = -cr->time_for_refresh;
  } else if (refresh_this_block) {
    // Else if it is accepted as candidate for refresh, and has not already
    // been refreshed (marked as 1) then mark it as a candidate for cleanup
    // for future time (marked as 0), otherwise don't update it.
    if (cr->map[block_index] == 1) new_map_value = 0;
  } else {
    // Leave it marked as block that is not candidate for refresh.
    new_map_value = 1;
  }

  // Update entries in the cyclic refresh map with new_map_value, and
  // copy mbmi->segment_id into global segmentation map.
  for (y = 0; y < ymis; y++)
    for (x = 0; x < xmis; x++) {
      int map_offset = block_index + y * cm->mi_cols + x;
      cr->map[map_offset] = new_map_value;
      cpi->segmentation_map[map_offset] = mbmi->segment_id;
      // Inter skip blocks were clearly not coded at the current qindex, so
      // don't update the map for them. For cases where motion is non-zero or
      // the reference frame isn't the previous frame, the previous value in
      // the map for this spatial location is not entirely correct.
      if ((!is_inter_block(mbmi) || !skip) &&
          mbmi->segment_id <= CR_SEGMENT_ID_BOOST2) {
        cr->last_coded_q_map[map_offset] = clamp(
            cm->base_qindex + cr->qindex_delta[mbmi->segment_id], 0, MAXQ);
      } else if (is_inter_block(mbmi) && skip &&
                 mbmi->segment_id <= CR_SEGMENT_ID_BOOST2) {
        cr->last_coded_q_map[map_offset] =
            AOMMIN(clamp(cm->base_qindex + cr->qindex_delta[mbmi->segment_id],
                         0, MAXQ),
                   cr->last_coded_q_map[map_offset]);
      }
    }
}

// Update the actual number of blocks that were applied the segment delta q.
void av1_cyclic_refresh_postencode(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  unsigned char *const seg_map = cpi->segmentation_map;
  int mi_row, mi_col;
  cr->actual_num_seg1_blocks = 0;
  cr->actual_num_seg2_blocks = 0;
  for (mi_row = 0; mi_row < cm->mi_rows; mi_row++)
    for (mi_col = 0; mi_col < cm->mi_cols; mi_col++) {
      if (cyclic_refresh_segment_id(seg_map[mi_row * cm->mi_cols + mi_col]) ==
          CR_SEGMENT_ID_BOOST1)
        cr->actual_num_seg1_blocks++;
      else if (cyclic_refresh_segment_id(
                   seg_map[mi_row * cm->mi_cols + mi_col]) ==
               CR_SEGMENT_ID_BOOST2)
        cr->actual_num_seg2_blocks++;
    }
}

// Set golden frame update interval, for 1 pass CBR mode.
void av1_cyclic_refresh_set_golden_update(AV1_COMP *const cpi) {
  RATE_CONTROL *const rc = &cpi->rc;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  // Set minimum gf_interval for GF update to a multiple (== 2) of refresh
  // period. Depending on past encoding stats, GF flag may be reset and update
  // may not occur until next baseline_gf_interval.
  if (cr->percent_refresh > 0)
    rc->baseline_gf_interval = 4 * (100 / cr->percent_refresh);
  else
    rc->baseline_gf_interval = 40;
}

// Update some encoding stats (from the just encoded frame). If this frame's
// background has high motion, refresh the golden frame. Otherwise, if the
// golden reference is to be updated check if we should NOT update the golden
// ref.
void av1_cyclic_refresh_check_golden_update(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  int mi_row, mi_col;
  double fraction_low = 0.0;
  int low_content_frame = 0;

  MB_MODE_INFO **mi;
  RATE_CONTROL *const rc = &cpi->rc;
  const int rows = cm->mi_rows, cols = cm->mi_cols;
  int cnt1 = 0, cnt2 = 0;
  int force_gf_refresh = 0;

  for (mi_row = 0; mi_row < rows; mi_row++) {
    mi = cm->mi_grid_visible + mi_row * cm->mi_stride;

    for (mi_col = 0; mi_col < cols; mi_col++) {
      int16_t abs_mvr = mi[0]->mv[0].as_mv.row >= 0
                            ? mi[0]->mv[0].as_mv.row
                            : -1 * mi[0]->mv[0].as_mv.row;
      int16_t abs_mvc = mi[0]->mv[0].as_mv.col >= 0
                            ? mi[0]->mv[0].as_mv.col
                            : -1 * mi[0]->mv[0].as_mv.col;

      // Calculate the motion of the background.
      if (abs_mvr <= 16 && abs_mvc <= 16) {
        cnt1++;
        if (abs_mvr == 0 && abs_mvc == 0) cnt2++;
      }
      mi++;

      // Accumulate low_content_frame.
      if (cr->map[mi_row * cols + mi_col] < 1) low_content_frame++;
    }
  }

  // For video conference clips, if the background has high motion in current
  // frame because of the camera movement, set this frame as the golden frame.
  // Use 70% and 5% as the thresholds for golden frame refreshing.
  if (cnt1 * 10 > (70 * rows * cols) && cnt2 * 20 < cnt1) {
    av1_cyclic_refresh_set_golden_update(cpi);
    rc->frames_till_gf_update_due = rc->baseline_gf_interval;

    if (rc->frames_till_gf_update_due > rc->frames_to_key)
      rc->frames_till_gf_update_due = rc->frames_to_key;
    cpi->refresh_golden_frame = 1;
    force_gf_refresh = 1;
  }

  fraction_low = (double)low_content_frame / (rows * cols);
  // Update average.
  cr->low_content_avg = (fraction_low + 3 * cr->low_content_avg) / 4;
  if (!force_gf_refresh && cpi->refresh_golden_frame == 1) {
    // Don't update golden reference if the amount of low_content for the
    // current encoded frame is small, or if the recursive average of the
    // low_content over the update interval window falls below threshold.
    if (fraction_low < 0.8 || cr->low_content_avg < 0.7)
      cpi->refresh_golden_frame = 0;
    // Reset for next internal.
    cr->low_content_avg = fraction_low;
  }
}

// Update the segmentation map, and related quantities: cyclic refresh map,
// refresh sb_index, and target number of blocks to be refreshed.
// The map is set to either 0/CR_SEGMENT_ID_BASE (no refresh) or to
// 1/CR_SEGMENT_ID_BOOST1 (refresh) for each superblock.
// Blocks labeled as BOOST1 may later get set to BOOST2 (during the
// encoding of the superblock).
static void cyclic_refresh_update_map(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  unsigned char *const seg_map = cpi->segmentation_map;
  int i, block_count, bl_index, sb_rows, sb_cols, sbs_in_frame;
  int xmis, ymis, x, y;
  memset(seg_map, CR_SEGMENT_ID_BASE, cm->mi_rows * cm->mi_cols);
  sb_cols =
      (cm->mi_cols + cm->seq_params.mib_size - 1) / cm->seq_params.mib_size;
  sb_rows =
      (cm->mi_rows + cm->seq_params.mib_size - 1) / cm->seq_params.mib_size;
  sbs_in_frame = sb_cols * sb_rows;
  // Number of target blocks to get the q delta (segment 1).
  block_count = cr->percent_refresh * cm->mi_rows * cm->mi_cols / 100;
  // Set the segmentation map: cycle through the superblocks, starting at
  // cr->mb_index, and stopping when either block_count blocks have been found
  // to be refreshed, or we have passed through whole frame.
  if (cr->sb_index >= sbs_in_frame) cr->sb_index = 0;
  assert(cr->sb_index < sbs_in_frame);
  i = cr->sb_index;
  cr->target_num_seg_blocks = 0;
  do {
    int sum_map = 0;
    // Get the mi_row/mi_col corresponding to superblock index i.
    int sb_row_index = (i / sb_cols);
    int sb_col_index = i - sb_row_index * sb_cols;
    int mi_row = sb_row_index * cm->seq_params.mib_size;
    int mi_col = sb_col_index * cm->seq_params.mib_size;
    int qindex_thresh =
        cpi->oxcf.content == AOM_CONTENT_SCREEN
            ? av1_get_qindex(&cm->seg, CR_SEGMENT_ID_BOOST2, cm->base_qindex)
            : 0;
    assert(mi_row >= 0 && mi_row < cm->mi_rows);
    assert(mi_col >= 0 && mi_col < cm->mi_cols);
    bl_index = mi_row * cm->mi_cols + mi_col;
    // Loop through all MI blocks in superblock and update map.
    xmis = AOMMIN(cm->mi_cols - mi_col, cm->seq_params.mib_size);
    ymis = AOMMIN(cm->mi_rows - mi_row, cm->seq_params.mib_size);
    for (y = 0; y < ymis; y++) {
      for (x = 0; x < xmis; x++) {
        const int bl_index2 = bl_index + y * cm->mi_cols + x;
        // If the block is as a candidate for clean up then mark it
        // for possible boost/refresh (segment 1). The segment id may get
        // reset to 0 later if block gets coded anything other than GLOBALMV.
        if (cr->map[bl_index2] == 0) {
          if (cr->last_coded_q_map[bl_index2] > qindex_thresh) sum_map++;
        } else if (cr->map[bl_index2] < 0) {
          cr->map[bl_index2]++;
        }
      }
    }
    // Enforce constant segment over superblock.
    // If segment is at least half of superblock, set to 1.
    if (sum_map >= xmis * ymis / 2) {
      for (y = 0; y < ymis; y++)
        for (x = 0; x < xmis; x++) {
          seg_map[bl_index + y * cm->mi_cols + x] = CR_SEGMENT_ID_BOOST1;
        }
      cr->target_num_seg_blocks += xmis * ymis;
    }
    i++;
    if (i == sbs_in_frame) {
      i = 0;
    }
  } while (cr->target_num_seg_blocks < block_count && i != cr->sb_index);
  cr->sb_index = i;
}

// Set cyclic refresh parameters.
void av1_cyclic_refresh_update_parameters(AV1_COMP *const cpi) {
  const RATE_CONTROL *const rc = &cpi->rc;
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  cr->percent_refresh = 10;
  cr->max_qdelta_perc = 50;
  cr->time_for_refresh = 0;
  // Use larger delta-qp (increase rate_ratio_qdelta) for first few (~4)
  // periods of the refresh cycle, after a key frame.
  if (rc->frames_since_key < 4 * cr->percent_refresh)
    cr->rate_ratio_qdelta = 3.0;
  else
    cr->rate_ratio_qdelta = 2.0;
  // Adjust some parameters for low resolutions at low bitrates.
  if (cm->width <= 352 && cm->height <= 288 && rc->avg_frame_bandwidth < 3400) {
    cr->motion_thresh = 4;
    cr->rate_boost_fac = 10;
  } else {
    cr->motion_thresh = 32;
    cr->rate_boost_fac = 17;
  }
}

// Setup cyclic background refresh: set delta q and segmentation map.
void av1_cyclic_refresh_setup(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const RATE_CONTROL *const rc = &cpi->rc;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  struct segmentation *const seg = &cm->seg;
  const int apply_cyclic_refresh = apply_cyclic_refresh_bitrate(cm, rc);
  int resolution_change =
      cm->prev_frame && (cm->width != cm->prev_frame->width ||
                         cm->height != cm->prev_frame->height);
  if (resolution_change) {
    memset(cpi->segmentation_map, 0, cm->mi_rows * cm->mi_cols);
    av1_clearall_segfeatures(seg);
    aom_clear_system_state();
    av1_disable_segmentation(seg);
    return;
  }
  if (cm->current_video_frame == 0) cr->low_content_avg = 0.0;
  // Don't apply refresh on key frame or enhancement layer frames.
  if (!apply_cyclic_refresh || cm->frame_type == KEY_FRAME) {
    // Set segmentation map to 0 and disable.
    unsigned char *const seg_map = cpi->segmentation_map;
    memset(seg_map, 0, cm->mi_rows * cm->mi_cols);
    av1_disable_segmentation(&cm->seg);
    if (cm->frame_type == KEY_FRAME) {
      memset(cr->last_coded_q_map, MAXQ,
             cm->mi_rows * cm->mi_cols * sizeof(*cr->last_coded_q_map));
      cr->sb_index = 0;
    }
    return;
  } else {
    int qindex_delta = 0;
    int qindex2;
    const double q =
        av1_convert_qindex_to_q(cm->base_qindex, cm->seq_params.bit_depth);
    aom_clear_system_state();
    // Set rate threshold to some multiple (set to 2 for now) of the target
    // rate (target is given by sb64_target_rate and scaled by 256).
    cr->thresh_rate_sb = ((int64_t)(rc->sb64_target_rate) << 8) << 2;
    // Distortion threshold, quadratic in Q, scale factor to be adjusted.
    // q will not exceed 457, so (q * q) is within 32bit; see:
    // av1_convert_qindex_to_q(), av1_ac_quant(), ac_qlookup*[].
    cr->thresh_dist_sb = ((int64_t)(q * q)) << 2;

    // Set up segmentation.
    // Clear down the segment map.
    av1_enable_segmentation(&cm->seg);
    av1_clearall_segfeatures(seg);

    // Note: setting temporal_update has no effect, as the seg-map coding method
    // (temporal or spatial) is determined in
    // av1_choose_segmap_coding_method(),
    // based on the coding cost of each method. For error_resilient mode on the
    // last_frame_seg_map is set to 0, so if temporal coding is used, it is
    // relative to 0 previous map.
    // seg->temporal_update = 0;

    // Segment BASE "Q" feature is disabled so it defaults to the baseline Q.
    av1_disable_segfeature(seg, CR_SEGMENT_ID_BASE, SEG_LVL_ALT_Q);
    // Use segment BOOST1 for in-frame Q adjustment.
    av1_enable_segfeature(seg, CR_SEGMENT_ID_BOOST1, SEG_LVL_ALT_Q);
    // Use segment BOOST2 for more aggressive in-frame Q adjustment.
    av1_enable_segfeature(seg, CR_SEGMENT_ID_BOOST2, SEG_LVL_ALT_Q);

    // Set the q delta for segment BOOST1.
    qindex_delta = compute_deltaq(cpi, cm->base_qindex, cr->rate_ratio_qdelta);
    cr->qindex_delta[1] = qindex_delta;

    // Compute rd-mult for segment BOOST1.
    qindex2 = clamp(cm->base_qindex + cm->y_dc_delta_q + qindex_delta, 0, MAXQ);

    cr->rdmult = av1_compute_rd_mult(cpi, qindex2);

    av1_set_segdata(seg, CR_SEGMENT_ID_BOOST1, SEG_LVL_ALT_Q, qindex_delta);

    // Set a more aggressive (higher) q delta for segment BOOST2.
    qindex_delta = compute_deltaq(
        cpi, cm->base_qindex,
        AOMMIN(CR_MAX_RATE_TARGET_RATIO,
               0.1 * cr->rate_boost_fac * cr->rate_ratio_qdelta));
    cr->qindex_delta[2] = qindex_delta;
    av1_set_segdata(seg, CR_SEGMENT_ID_BOOST2, SEG_LVL_ALT_Q, qindex_delta);

    // Update the segmentation and refresh map.
    cyclic_refresh_update_map(cpi);
  }
}

int av1_cyclic_refresh_get_rdmult(const CYCLIC_REFRESH *cr) {
  return cr->rdmult;
}

void av1_cyclic_refresh_reset_resize(AV1_COMP *const cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
  memset(cr->map, 0, cm->mi_rows * cm->mi_cols);
  cr->sb_index = 0;
  cpi->refresh_golden_frame = 1;
}

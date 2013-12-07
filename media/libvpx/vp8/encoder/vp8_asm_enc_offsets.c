/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/asm_offsets.h"
#include "vpx_config.h"
#include "block.h"
#include "vp8/common/blockd.h"
#include "onyx_int.h"
#include "treewriter.h"
#include "tokenize.h"

BEGIN

/* regular quantize */
DEFINE(vp8_block_coeff,                         offsetof(BLOCK, coeff));
DEFINE(vp8_block_zbin,                          offsetof(BLOCK, zbin));
DEFINE(vp8_block_round,                         offsetof(BLOCK, round));
DEFINE(vp8_block_quant,                         offsetof(BLOCK, quant));
DEFINE(vp8_block_quant_fast,                    offsetof(BLOCK, quant_fast));
DEFINE(vp8_block_zbin_extra,                    offsetof(BLOCK, zbin_extra));
DEFINE(vp8_block_zrun_zbin_boost,               offsetof(BLOCK, zrun_zbin_boost));
DEFINE(vp8_block_quant_shift,                   offsetof(BLOCK, quant_shift));

DEFINE(vp8_blockd_qcoeff,                       offsetof(BLOCKD, qcoeff));
DEFINE(vp8_blockd_dequant,                      offsetof(BLOCKD, dequant));
DEFINE(vp8_blockd_dqcoeff,                      offsetof(BLOCKD, dqcoeff));
DEFINE(vp8_blockd_eob,                          offsetof(BLOCKD, eob));

/* subtract */
DEFINE(vp8_block_base_src,                      offsetof(BLOCK, base_src));
DEFINE(vp8_block_src,                           offsetof(BLOCK, src));
DEFINE(vp8_block_src_diff,                      offsetof(BLOCK, src_diff));
DEFINE(vp8_block_src_stride,                    offsetof(BLOCK, src_stride));

DEFINE(vp8_blockd_predictor,                    offsetof(BLOCKD, predictor));

/* pack tokens */
DEFINE(vp8_writer_lowvalue,                     offsetof(vp8_writer, lowvalue));
DEFINE(vp8_writer_range,                        offsetof(vp8_writer, range));
DEFINE(vp8_writer_count,                        offsetof(vp8_writer, count));
DEFINE(vp8_writer_pos,                          offsetof(vp8_writer, pos));
DEFINE(vp8_writer_buffer,                       offsetof(vp8_writer, buffer));
DEFINE(vp8_writer_buffer_end,                   offsetof(vp8_writer, buffer_end));
DEFINE(vp8_writer_error,                        offsetof(vp8_writer, error));

DEFINE(tokenextra_token,                        offsetof(TOKENEXTRA, Token));
DEFINE(tokenextra_extra,                        offsetof(TOKENEXTRA, Extra));
DEFINE(tokenextra_context_tree,                 offsetof(TOKENEXTRA, context_tree));
DEFINE(tokenextra_skip_eob_node,                offsetof(TOKENEXTRA, skip_eob_node));
DEFINE(TOKENEXTRA_SZ,                           sizeof(TOKENEXTRA));

DEFINE(vp8_extra_bit_struct_sz,                 sizeof(vp8_extra_bit_struct));

DEFINE(vp8_token_value,                         offsetof(vp8_token, value));
DEFINE(vp8_token_len,                           offsetof(vp8_token, Len));

DEFINE(vp8_extra_bit_struct_tree,               offsetof(vp8_extra_bit_struct, tree));
DEFINE(vp8_extra_bit_struct_prob,               offsetof(vp8_extra_bit_struct, prob));
DEFINE(vp8_extra_bit_struct_len,                offsetof(vp8_extra_bit_struct, Len));
DEFINE(vp8_extra_bit_struct_base_val,           offsetof(vp8_extra_bit_struct, base_val));

DEFINE(vp8_comp_tplist,                         offsetof(VP8_COMP, tplist));
DEFINE(vp8_comp_common,                         offsetof(VP8_COMP, common));
DEFINE(vp8_comp_bc ,                            offsetof(VP8_COMP, bc));
DEFINE(vp8_writer_sz ,                          sizeof(vp8_writer));

DEFINE(tokenlist_start,                         offsetof(TOKENLIST, start));
DEFINE(tokenlist_stop,                          offsetof(TOKENLIST, stop));
DEFINE(TOKENLIST_SZ,                            sizeof(TOKENLIST));

DEFINE(vp8_common_mb_rows,                      offsetof(VP8_COMMON, mb_rows));

END

/* add asserts for any offset that is not supported by assembly code
 * add asserts for any size that is not supported by assembly code

 * These are used in vp8cx_pack_tokens.  They are hard coded so if their sizes
 * change they will have to be adjusted.
 */

#if HAVE_EDSP
ct_assert(TOKENEXTRA_SZ, sizeof(TOKENEXTRA) == 8)
ct_assert(vp8_extra_bit_struct_sz, sizeof(vp8_extra_bit_struct) == 16)
#endif

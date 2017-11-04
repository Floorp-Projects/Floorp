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

// Inspect Decoder
// ================
//
// This is a simple decoder loop that writes JSON stats to stdout. This tool
// can also be compiled with Emscripten and used as a library.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./args.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "aom/aom_decoder.h"
#include "./aom_config.h"
#if CONFIG_ACCOUNTING
#include "../av1/decoder/accounting.h"
#endif
#include "../av1/decoder/inspection.h"
#include "aom/aomdx.h"

#include "../tools_common.h"
#include "../video_reader.h"
// #include "av1/av1_dx_iface.c"
#include "../av1/common/onyxc_int.h"

#include "../video_common.h"

// Max JSON buffer size.
const int MAX_BUFFER = 1024 * 1024 * 32;

typedef enum {
  ACCOUNTING_LAYER = 1,
  BLOCK_SIZE_LAYER = 1 << 1,
  TRANSFORM_SIZE_LAYER = 1 << 2,
  TRANSFORM_TYPE_LAYER = 1 << 3,
  MODE_LAYER = 1 << 4,
  SKIP_LAYER = 1 << 5,
  FILTER_LAYER = 1 << 6,
  CDEF_LAYER = 1 << 7,
  REFERENCE_FRAME_LAYER = 1 << 8,
  MOTION_VECTORS_LAYER = 1 << 9,
  UV_MODE_LAYER = 1 << 10,
  CFL_LAYER = 1 << 11,
  ALL_LAYERS = (1 << 12) - 1
} LayerType;

static LayerType layers = 0;

static int stop_after = 0;
static int compress = 0;

static const arg_def_t limit_arg =
    ARG_DEF(NULL, "limit", 1, "Stop decoding after n frames");
static const arg_def_t dump_all_arg = ARG_DEF("A", "all", 0, "Dump All");
static const arg_def_t compress_arg =
    ARG_DEF("x", "compress", 0, "Compress JSON using RLE");
static const arg_def_t dump_accounting_arg =
    ARG_DEF("a", "accounting", 0, "Dump Accounting");
static const arg_def_t dump_block_size_arg =
    ARG_DEF("bs", "blockSize", 0, "Dump Block Size");
static const arg_def_t dump_motion_vectors_arg =
    ARG_DEF("mv", "motionVectors", 0, "Dump Motion Vectors");
static const arg_def_t dump_transform_size_arg =
    ARG_DEF("ts", "transformSize", 0, "Dump Transform Size");
static const arg_def_t dump_transform_type_arg =
    ARG_DEF("tt", "transformType", 0, "Dump Transform Type");
static const arg_def_t dump_mode_arg = ARG_DEF("m", "mode", 0, "Dump Mode");
static const arg_def_t dump_uv_mode_arg =
    ARG_DEF("uvm", "uv_mode", 0, "Dump UV Intra Prediction Modes");
static const arg_def_t dump_skip_arg = ARG_DEF("s", "skip", 0, "Dump Skip");
static const arg_def_t dump_filter_arg =
    ARG_DEF("f", "filter", 0, "Dump Filter");
static const arg_def_t dump_cdef_arg = ARG_DEF("c", "cdef", 0, "Dump CDEF");
#if CONFIG_CFL
static const arg_def_t dump_cfl_arg =
    ARG_DEF("cfl", "chroma_from_luma", 0, "Dump Chroma from Luma Alphas");
#endif
static const arg_def_t dump_reference_frame_arg =
    ARG_DEF("r", "referenceFrame", 0, "Dump Reference Frame");
static const arg_def_t usage_arg = ARG_DEF("h", "help", 0, "Help");

static const arg_def_t *main_args[] = { &limit_arg,
                                        &dump_all_arg,
                                        &compress_arg,
#if CONFIG_ACCOUNTING
                                        &dump_accounting_arg,
#endif
                                        &dump_block_size_arg,
                                        &dump_transform_size_arg,
                                        &dump_transform_type_arg,
                                        &dump_mode_arg,
                                        &dump_uv_mode_arg,
                                        &dump_skip_arg,
                                        &dump_filter_arg,
#if CONFIG_CDEF
                                        &dump_cdef_arg,
#endif
#if CONFIG_CFL
                                        &dump_cfl_arg,
#endif
                                        &dump_reference_frame_arg,
                                        &dump_motion_vectors_arg,
                                        &usage_arg,
                                        NULL };
#define ENUM(name) \
  { #name, name }
#define LAST_ENUM \
  { NULL, 0 }
typedef struct map_entry {
  const char *name;
  int value;
} map_entry;

const map_entry refs_map[] = { ENUM(INTRA_FRAME),  ENUM(LAST_FRAME),
#if CONFIG_EXT_REFS
                               ENUM(LAST2_FRAME),  ENUM(LAST3_FRAME),
                               ENUM(GOLDEN_FRAME), ENUM(BWDREF_FRAME),
                               ENUM(ALTREF_FRAME),
#else
                               ENUM(GOLDEN_FRAME), ENUM(ALTREF_FRAME),
#endif
                               LAST_ENUM };

const map_entry block_size_map[] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  ENUM(BLOCK_2X2),    ENUM(BLOCK_2X4),    ENUM(BLOCK_4X2),
#endif
  ENUM(BLOCK_4X4),    ENUM(BLOCK_4X8),    ENUM(BLOCK_8X4),
  ENUM(BLOCK_8X8),    ENUM(BLOCK_8X16),   ENUM(BLOCK_16X8),
  ENUM(BLOCK_16X16),  ENUM(BLOCK_16X32),  ENUM(BLOCK_32X16),
  ENUM(BLOCK_32X32),  ENUM(BLOCK_32X64),  ENUM(BLOCK_64X32),
  ENUM(BLOCK_64X64),
#if CONFIG_EXT_PARTITION
  ENUM(BLOCK_64X128), ENUM(BLOCK_128X64), ENUM(BLOCK_128X128),
#endif
  ENUM(BLOCK_4X16),   ENUM(BLOCK_16X4),   ENUM(BLOCK_8X32),
  ENUM(BLOCK_32X8),   ENUM(BLOCK_16X64),  ENUM(BLOCK_64X16),
#if CONFIG_EXT_PARTITION
  ENUM(BLOCK_32X128), ENUM(BLOCK_128X32),
#endif
  LAST_ENUM
};

const map_entry tx_size_map[] = {
#if CONFIG_CHROMA_2X2
  ENUM(TX_2X2),
#endif
  ENUM(TX_4X4),   ENUM(TX_8X8),   ENUM(TX_16X16), ENUM(TX_32X32),
#if CONFIG_TX64X64
  ENUM(TX_64X64),
#endif
  ENUM(TX_4X8),   ENUM(TX_8X4),   ENUM(TX_8X16),  ENUM(TX_16X8),
  ENUM(TX_16X32), ENUM(TX_32X16),
#if CONFIG_TX64X64
  ENUM(TX_32X64), ENUM(TX_64X32),
#endif  // CONFIG_TX64X64
  ENUM(TX_4X16),  ENUM(TX_16X4),  ENUM(TX_8X32),  ENUM(TX_32X8),
  LAST_ENUM
};

const map_entry tx_type_map[] = { ENUM(DCT_DCT),
                                  ENUM(ADST_DCT),
                                  ENUM(DCT_ADST),
                                  ENUM(ADST_ADST),
#if CONFIG_EXT_TX
                                  ENUM(FLIPADST_DCT),
                                  ENUM(DCT_FLIPADST),
                                  ENUM(FLIPADST_FLIPADST),
                                  ENUM(ADST_FLIPADST),
                                  ENUM(FLIPADST_ADST),
                                  ENUM(IDTX),
                                  ENUM(V_DCT),
                                  ENUM(H_DCT),
                                  ENUM(V_ADST),
                                  ENUM(H_ADST),
                                  ENUM(V_FLIPADST),
                                  ENUM(H_FLIPADST),
#endif
                                  LAST_ENUM };

const map_entry prediction_mode_map[] = {
  ENUM(DC_PRED),       ENUM(V_PRED),        ENUM(H_PRED),
  ENUM(D45_PRED),      ENUM(D135_PRED),     ENUM(D117_PRED),
  ENUM(D153_PRED),     ENUM(D207_PRED),     ENUM(D63_PRED),
  ENUM(SMOOTH_PRED),
#if CONFIG_SMOOTH_HV
  ENUM(SMOOTH_V_PRED), ENUM(SMOOTH_H_PRED),
#endif  // CONFIG_SMOOTH_HV
  ENUM(TM_PRED),       ENUM(NEARESTMV),     ENUM(NEARMV),
  ENUM(ZEROMV),        ENUM(NEWMV),         ENUM(NEAREST_NEARESTMV),
  ENUM(NEAR_NEARMV),   ENUM(NEAREST_NEWMV), ENUM(NEW_NEARESTMV),
  ENUM(NEAR_NEWMV),    ENUM(NEW_NEARMV),    ENUM(ZERO_ZEROMV),
  ENUM(NEW_NEWMV),     ENUM(INTRA_INVALID), LAST_ENUM
};

#if CONFIG_CFL
const map_entry uv_prediction_mode_map[] = {
  ENUM(UV_DC_PRED),       ENUM(UV_V_PRED),
  ENUM(UV_H_PRED),        ENUM(UV_D45_PRED),
  ENUM(UV_D135_PRED),     ENUM(UV_D117_PRED),
  ENUM(UV_D153_PRED),     ENUM(UV_D207_PRED),
  ENUM(UV_D63_PRED),      ENUM(UV_SMOOTH_PRED),
#if CONFIG_SMOOTH_HV
  ENUM(UV_SMOOTH_V_PRED), ENUM(UV_SMOOTH_H_PRED),
#endif  // CONFIG_SMOOTH_HV
  ENUM(UV_TM_PRED),
#if CONFIG_CFL
  ENUM(UV_CFL_PRED),
#endif
  ENUM(UV_MODE_INVALID),  LAST_ENUM
};
#else
#define uv_prediction_mode_map prediction_mode_map
#endif
#define NO_SKIP 0
#define SKIP 1

const map_entry skip_map[] = { ENUM(SKIP), ENUM(NO_SKIP), LAST_ENUM };

const map_entry config_map[] = { ENUM(MI_SIZE), LAST_ENUM };

static const char *exec_name;

insp_frame_data frame_data;
int frame_count = 0;
int decoded_frame_count = 0;
aom_codec_ctx_t codec;
AvxVideoReader *reader = NULL;
const AvxVideoInfo *info = NULL;
aom_image_t *img = NULL;

void on_frame_decoded_dump(char *json) {
#ifdef __EMSCRIPTEN__
  EM_ASM_({ Module.on_frame_decoded_json($0); }, json);
#else
  printf("%s", json);
#endif
}

// Writing out the JSON buffer using snprintf is very slow, especially when
// compiled with emscripten, these functions speed things up quite a bit.
int put_str(char *buffer, const char *str) {
  int i;
  for (i = 0; str[i] != '\0'; i++) {
    buffer[i] = str[i];
  }
  return i;
}

int put_str_with_escape(char *buffer, const char *str) {
  int i;
  int j = 0;
  for (i = 0; str[i] != '\0'; i++) {
    if (str[i] < ' ') {
      continue;
    } else if (str[i] == '"' || str[i] == '\\') {
      buffer[j++] = '\\';
    }
    buffer[j++] = str[i];
  }
  return j;
}

int put_num(char *buffer, char prefix, int num, char suffix) {
  int i = 0;
  char *buf = buffer;
  int is_neg = 0;
  if (prefix) {
    buf[i++] = prefix;
  }
  if (num == 0) {
    buf[i++] = '0';
  } else {
    if (num < 0) {
      num = -num;
      is_neg = 1;
    }
    int s = i;
    while (num != 0) {
      buf[i++] = '0' + (num % 10);
      num = num / 10;
    }
    if (is_neg) {
      buf[i++] = '-';
    }
    int e = i - 1;
    while (s < e) {
      int t = buf[s];
      buf[s] = buf[e];
      buf[e] = t;
      s++;
      e--;
    }
  }
  if (suffix) {
    buf[i++] = suffix;
  }
  return i;
}

int put_map(char *buffer, const map_entry *map) {
  char *buf = buffer;
  const map_entry *entry = map;
  while (entry->name != NULL) {
    *(buf++) = '"';
    buf += put_str(buf, entry->name);
    *(buf++) = '"';
    buf += put_num(buf, ':', entry->value, 0);
    entry++;
    if (entry->name != NULL) {
      *(buf++) = ',';
    }
  }
  return buf - buffer;
}

int put_reference_frame(char *buffer) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t;
  buf += put_str(buf, "  \"referenceFrameMap\": {");
  buf += put_map(buf, refs_map);
  buf += put_str(buf, "},\n");
  buf += put_str(buf, "  \"referenceFrame\": [");
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      buf += put_num(buf, '[', mi->ref_frame[0], 0);
      buf += put_num(buf, ',', mi->ref_frame[1], ']');
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          if (mi->ref_frame[0] != next_mi->ref_frame[0] ||
              mi->ref_frame[1] != next_mi->ref_frame[1]) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return buf - buffer;
}

int put_motion_vectors(char *buffer) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t;
  buf += put_str(buf, "  \"motionVectors\": [");
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *mi = &frame_data.mi_grid[r * mi_cols + c];
      buf += put_num(buf, '[', mi->mv[0].col, 0);
      buf += put_num(buf, ',', mi->mv[0].row, 0);
      buf += put_num(buf, ',', mi->mv[1].col, 0);
      buf += put_num(buf, ',', mi->mv[1].row, ']');
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          if (mi->mv[0].col != next_mi->mv[0].col ||
              mi->mv[0].row != next_mi->mv[0].row ||
              mi->mv[1].col != next_mi->mv[1].col ||
              mi->mv[1].row != next_mi->mv[1].row) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return buf - buffer;
}

int put_block_info(char *buffer, const map_entry *map, const char *name,
                   size_t offset) {
  const int mi_rows = frame_data.mi_rows;
  const int mi_cols = frame_data.mi_cols;
  char *buf = buffer;
  int r, c, t, v;
  if (map) {
    buf += snprintf(buf, MAX_BUFFER, "  \"%sMap\": {", name);
    buf += put_map(buf, map);
    buf += put_str(buf, "},\n");
  }
  buf += snprintf(buf, MAX_BUFFER, "  \"%s\": [", name);
  for (r = 0; r < mi_rows; ++r) {
    *(buf++) = '[';
    for (c = 0; c < mi_cols; ++c) {
      insp_mi_data *curr_mi = &frame_data.mi_grid[r * mi_cols + c];
      v = *(((int8_t *)curr_mi) + offset);
      buf += put_num(buf, 0, v, 0);
      if (compress) {  // RLE
        for (t = c + 1; t < mi_cols; ++t) {
          insp_mi_data *next_mi = &frame_data.mi_grid[r * mi_cols + t];
          if (v != *(((int8_t *)next_mi) + offset)) {
            break;
          }
        }
        if (t - c > 1) {
          *(buf++) = ',';
          buf += put_num(buf, '[', t - c - 1, ']');
          c = t - 1;
        }
      }
      if (c < mi_cols - 1) *(buf++) = ',';
    }
    *(buf++) = ']';
    if (r < mi_rows - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return buf - buffer;
}

#if CONFIG_ACCOUNTING
int put_accounting(char *buffer) {
  char *buf = buffer;
  int i;
  const Accounting *accounting = frame_data.accounting;
  if (accounting == NULL) {
    printf("XXX\n");
    return 0;
  }
  const int num_syms = accounting->syms.num_syms;
  const int num_strs = accounting->syms.dictionary.num_strs;
  buf += put_str(buf, "  \"symbolsMap\": [");
  for (i = 0; i < num_strs; i++) {
    buf += snprintf(buf, MAX_BUFFER, "\"%s\"",
                    accounting->syms.dictionary.strs[i]);
    if (i < num_strs - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  buf += put_str(buf, "  \"symbols\": [\n    ");
  AccountingSymbolContext context;
  context.x = -2;
  context.y = -2;
  AccountingSymbol *sym;
  for (i = 0; i < num_syms; i++) {
    sym = &accounting->syms.syms[i];
    if (memcmp(&context, &sym->context, sizeof(AccountingSymbolContext)) != 0) {
      buf += put_num(buf, '[', sym->context.x, 0);
      buf += put_num(buf, ',', sym->context.y, ']');
    } else {
      buf += put_num(buf, '[', sym->id, 0);
      buf += put_num(buf, ',', sym->bits, 0);
      buf += put_num(buf, ',', sym->samples, ']');
    }
    context = sym->context;
    if (i < num_syms - 1) *(buf++) = ',';
  }
  buf += put_str(buf, "],\n");
  return buf - buffer;
}
#endif

void inspect(void *pbi, void *data) {
  /* Fetch frame data. */
  ifd_inspect(&frame_data, pbi);
  (void)data;
  // We allocate enough space and hope we don't write out of bounds. Totally
  // unsafe but this speeds things up, especially when compiled to Javascript.
  char *buffer = aom_malloc(MAX_BUFFER);
  char *buf = buffer;
  buf += put_str(buf, "{\n");
  if (layers & BLOCK_SIZE_LAYER) {
    buf += put_block_info(buf, block_size_map, "blockSize",
                          offsetof(insp_mi_data, sb_type));
  }
  if (layers & TRANSFORM_SIZE_LAYER) {
    buf += put_block_info(buf, tx_size_map, "transformSize",
                          offsetof(insp_mi_data, tx_size));
  }
  if (layers & TRANSFORM_TYPE_LAYER) {
    buf += put_block_info(buf, tx_type_map, "transformType",
                          offsetof(insp_mi_data, tx_type));
  }
  if (layers & MODE_LAYER) {
    buf += put_block_info(buf, prediction_mode_map, "mode",
                          offsetof(insp_mi_data, mode));
  }
  if (layers & UV_MODE_LAYER) {
    buf += put_block_info(buf, uv_prediction_mode_map, "uv_mode",
                          offsetof(insp_mi_data, uv_mode));
  }
  if (layers & SKIP_LAYER) {
    buf += put_block_info(buf, skip_map, "skip", offsetof(insp_mi_data, skip));
  }
  if (layers & FILTER_LAYER) {
    buf += put_block_info(buf, NULL, "filter", offsetof(insp_mi_data, filter));
  }
#if CONFIG_CDEF
  if (layers & CDEF_LAYER) {
    buf += put_block_info(buf, NULL, "cdef_level",
                          offsetof(insp_mi_data, cdef_level));
    buf += put_block_info(buf, NULL, "cdef_strength",
                          offsetof(insp_mi_data, cdef_strength));
  }
#endif
#if CONFIG_CFL
  if (layers & CFL_LAYER) {
    buf += put_block_info(buf, NULL, "cfl_alpha_idx",
                          offsetof(insp_mi_data, cfl_alpha_idx));
    buf += put_block_info(buf, NULL, "cfl_alpha_sign",
                          offsetof(insp_mi_data, cfl_alpha_sign));
  }
#endif
  if (layers & MOTION_VECTORS_LAYER) {
    buf += put_motion_vectors(buf);
  }
  if (layers & REFERENCE_FRAME_LAYER) {
    buf += put_reference_frame(buf);
  }
#if CONFIG_ACCOUNTING
  if (layers & ACCOUNTING_LAYER) {
    buf += put_accounting(buf);
  }
#endif
  buf += snprintf(buf, MAX_BUFFER, "  \"frame\": %d,\n", decoded_frame_count);
  buf += snprintf(buf, MAX_BUFFER, "  \"showFrame\": %d,\n",
                  frame_data.show_frame);
  buf += snprintf(buf, MAX_BUFFER, "  \"frameType\": %d,\n",
                  frame_data.frame_type);
  buf += snprintf(buf, MAX_BUFFER, "  \"baseQIndex\": %d,\n",
                  frame_data.base_qindex);
  buf += snprintf(buf, MAX_BUFFER, "  \"tileCols\": %d,\n",
                  frame_data.tile_mi_cols);
  buf += snprintf(buf, MAX_BUFFER, "  \"tileRows\": %d,\n",
                  frame_data.tile_mi_rows);
  buf += put_str(buf, "  \"config\": {");
  buf += put_map(buf, config_map);
  buf += put_str(buf, "},\n");
  buf += put_str(buf, "  \"configString\": \"");
  buf += put_str_with_escape(buf, aom_codec_build_config());
  buf += put_str(buf, "\"\n");
  decoded_frame_count++;
  buf += put_str(buf, "},\n");
  *(buf++) = 0;
  on_frame_decoded_dump(buffer);
  aom_free(buffer);
}

void ifd_init_cb() {
  aom_inspect_init ii;
  ii.inspect_cb = inspect;
  ii.inspect_ctx = NULL;
  aom_codec_control(&codec, AV1_SET_INSPECTION_CALLBACK, &ii);
}

EMSCRIPTEN_KEEPALIVE
int open_file(char *file) {
  if (file == NULL) {
    // The JS analyzer puts the .ivf file at this location.
    file = "/tmp/input.ivf";
  }
  reader = aom_video_reader_open(file);
  if (!reader) die("Failed to open %s for reading.", file);
  info = aom_video_reader_get_info(reader);
  const AvxInterface *decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  fprintf(stderr, "Using %s\n",
          aom_codec_iface_name(decoder->codec_interface()));
  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");
  ifd_init(&frame_data, info->frame_width, info->frame_height);
  ifd_init_cb();
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
int read_frame() {
  if (!aom_video_reader_read_frame(reader)) return EXIT_FAILURE;
  img = NULL;
  aom_codec_iter_t iter = NULL;
  size_t frame_size = 0;
  const unsigned char *frame = aom_video_reader_get_frame(reader, &frame_size);
  if (aom_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 0) !=
      AOM_CODEC_OK) {
    die_codec(&codec, "Failed to decode frame.");
  }
  img = aom_codec_get_frame(&codec, &iter);
  if (img == NULL) {
    return EXIT_FAILURE;
  }
  ++frame_count;
  return EXIT_SUCCESS;
}

EMSCRIPTEN_KEEPALIVE
const char *get_aom_codec_build_config() { return aom_codec_build_config(); }

EMSCRIPTEN_KEEPALIVE
int get_bit_depth() { return img->bit_depth; }

EMSCRIPTEN_KEEPALIVE
int get_bits_per_sample() { return img->bps; }

EMSCRIPTEN_KEEPALIVE
int get_image_format() { return img->fmt; }

EMSCRIPTEN_KEEPALIVE
unsigned char *get_plane(int plane) { return img->planes[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_stride(int plane) { return img->stride[plane]; }

EMSCRIPTEN_KEEPALIVE
int get_plane_width(int plane) { return aom_img_plane_width(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_plane_height(int plane) { return aom_img_plane_height(img, plane); }

EMSCRIPTEN_KEEPALIVE
int get_frame_width() { return info->frame_width; }

EMSCRIPTEN_KEEPALIVE
int get_frame_height() { return info->frame_height; }

static void parse_args(char **argv) {
  char **argi, **argj;
  struct arg arg;
  (void)dump_accounting_arg;
  (void)dump_cdef_arg;
  for (argi = argj = argv; (*argj = *argi); argi += arg.argv_step) {
    arg.argv_step = 1;
    if (arg_match(&arg, &dump_block_size_arg, argi)) layers |= BLOCK_SIZE_LAYER;
#if CONFIG_ACCOUNTING
    else if (arg_match(&arg, &dump_accounting_arg, argi))
      layers |= ACCOUNTING_LAYER;
#endif
    else if (arg_match(&arg, &dump_transform_size_arg, argi))
      layers |= TRANSFORM_SIZE_LAYER;
    else if (arg_match(&arg, &dump_transform_type_arg, argi))
      layers |= TRANSFORM_TYPE_LAYER;
    else if (arg_match(&arg, &dump_mode_arg, argi))
      layers |= MODE_LAYER;
    else if (arg_match(&arg, &dump_uv_mode_arg, argi))
      layers |= UV_MODE_LAYER;
    else if (arg_match(&arg, &dump_skip_arg, argi))
      layers |= SKIP_LAYER;
    else if (arg_match(&arg, &dump_filter_arg, argi))
      layers |= FILTER_LAYER;
#if CONFIG_CDEF
    else if (arg_match(&arg, &dump_cdef_arg, argi))
      layers |= CDEF_LAYER;
#endif
#if CONFIG_CFL
    else if (arg_match(&arg, &dump_cfl_arg, argi))
      layers |= CFL_LAYER;
#endif
    else if (arg_match(&arg, &dump_reference_frame_arg, argi))
      layers |= REFERENCE_FRAME_LAYER;
    else if (arg_match(&arg, &dump_motion_vectors_arg, argi))
      layers |= MOTION_VECTORS_LAYER;
    else if (arg_match(&arg, &dump_all_arg, argi))
      layers |= ALL_LAYERS;
    else if (arg_match(&arg, &compress_arg, argi))
      compress = 1;
    else if (arg_match(&arg, &usage_arg, argi))
      usage_exit();
    else if (arg_match(&arg, &limit_arg, argi))
      stop_after = arg_parse_uint(&arg);
    else
      argj++;
  }
}

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s src_filename <options>\n", exec_name);
  fprintf(stderr, "\nOptions:\n");
  arg_show_usage(stderr, main_args);
  exit(EXIT_FAILURE);
}

EMSCRIPTEN_KEEPALIVE
int main(int argc, char **argv) {
  exec_name = argv[0];
  parse_args(argv);
  if (argc >= 2) {
    open_file(argv[1]);
    printf("[\n");
    while (1) {
      if (stop_after && (decoded_frame_count >= stop_after)) break;
      if (read_frame()) break;
    }
    printf("null\n");
    printf("]");
  } else {
    usage_exit();
  }
}

EMSCRIPTEN_KEEPALIVE
void quit() {
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_reader_close(reader);
}

EMSCRIPTEN_KEEPALIVE
void set_layers(LayerType v) { layers = v; }

EMSCRIPTEN_KEEPALIVE
void set_compress(int v) { compress = v; }

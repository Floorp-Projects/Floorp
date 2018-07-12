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
#ifndef TOOLS_COMMON_H_
#define TOOLS_COMMON_H_

#include <stdio.h>

#include "./aom_config.h"
#include "aom/aom_codec.h"
#include "aom/aom_image.h"
#include "aom/aom_integer.h"
#include "aom_ports/msvc.h"

#if CONFIG_AV1_ENCODER
#include "./y4minput.h"
#endif

#if defined(_MSC_VER)
/* MSVS uses _f{seek,tell}i64. */
#define fseeko _fseeki64
#define ftello _ftelli64
typedef int64_t FileOffset;
#elif defined(_WIN32)
#include <sys/types.h> /* NOLINT*/
/* MinGW uses f{seek,tell}o64 for large files. */
#define fseeko fseeko64
#define ftello ftello64
typedef off64_t FileOffset;
#elif CONFIG_OS_SUPPORT
#include <sys/types.h> /* NOLINT*/
typedef off_t FileOffset;
/* Use 32-bit file operations in WebM file format when building ARM
 * executables (.axf) with RVCT. */
#else
#define fseeko fseek
#define ftello ftell
typedef long FileOffset; /* NOLINT */
#endif /* CONFIG_OS_SUPPORT */

#if CONFIG_OS_SUPPORT
#if defined(_MSC_VER)
#include <io.h> /* NOLINT */
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h> /* NOLINT */
#endif              /* _MSC_VER */
#endif              /* CONFIG_OS_SUPPORT */

#define LITERALU64(hi, lo) ((((uint64_t)hi) << 32) | lo)

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#define IVF_FRAME_HDR_SZ (4 + 8) /* 4 byte size + 8 byte timestamp */
#define IVF_FILE_HDR_SZ 32

#define RAW_FRAME_HDR_SZ sizeof(uint32_t)

#define AV1_FOURCC 0x31305641

enum VideoFileType {
  FILE_TYPE_RAW,
  FILE_TYPE_IVF,
  FILE_TYPE_Y4M,
  FILE_TYPE_WEBM
};

struct FileTypeDetectionBuffer {
  char buf[4];
  size_t buf_read;
  size_t position;
};

struct AvxRational {
  int numerator;
  int denominator;
};

struct AvxInputContext {
  const char *filename;
  FILE *file;
  int64_t length;
  struct FileTypeDetectionBuffer detect;
  enum VideoFileType file_type;
  uint32_t width;
  uint32_t height;
  struct AvxRational pixel_aspect_ratio;
  aom_img_fmt_t fmt;
  aom_bit_depth_t bit_depth;
  int only_i420;
  uint32_t fourcc;
  struct AvxRational framerate;
#if CONFIG_AV1_ENCODER
  y4m_input y4m;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
#define AOM_NO_RETURN __attribute__((noreturn))
#else
#define AOM_NO_RETURN
#endif

/* Sets a stdio stream into binary mode */
FILE *set_binary_mode(FILE *stream);

void die(const char *fmt, ...) AOM_NO_RETURN;
void fatal(const char *fmt, ...) AOM_NO_RETURN;
void warn(const char *fmt, ...);

void die_codec(aom_codec_ctx_t *ctx, const char *s) AOM_NO_RETURN;

/* The tool including this file must define usage_exit() */
void usage_exit(void) AOM_NO_RETURN;

#undef AOM_NO_RETURN

int read_yuv_frame(struct AvxInputContext *input_ctx, aom_image_t *yuv_frame);

typedef struct AvxInterface {
  const char *const name;
  const uint32_t fourcc;
  aom_codec_iface_t *(*const codec_interface)();
} AvxInterface;

int get_aom_encoder_count(void);
const AvxInterface *get_aom_encoder_by_index(int i);
const AvxInterface *get_aom_encoder_by_name(const char *name);

int get_aom_decoder_count(void);
const AvxInterface *get_aom_decoder_by_index(int i);
const AvxInterface *get_aom_decoder_by_name(const char *name);
const AvxInterface *get_aom_decoder_by_fourcc(uint32_t fourcc);

void aom_img_write(const aom_image_t *img, FILE *file);
int aom_img_read(aom_image_t *img, FILE *file);

double sse_to_psnr(double samples, double peak, double mse);

#if CONFIG_HIGHBITDEPTH
void aom_img_upshift(aom_image_t *dst, aom_image_t *src, int input_shift);
void aom_img_downshift(aom_image_t *dst, aom_image_t *src, int down_shift);
void aom_img_truncate_16_to_8(aom_image_t *dst, aom_image_t *src);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // TOOLS_COMMON_H_

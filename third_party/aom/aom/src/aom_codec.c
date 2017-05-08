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

/*!\file
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <stdarg.h>
#include <stdlib.h>
#include "aom/aom_integer.h"
#include "aom/internal/aom_codec_internal.h"
#include "aom_version.h"

#define SAVE_STATUS(ctx, var) (ctx ? (ctx->err = var) : var)

int aom_codec_version(void) { return VERSION_PACKED; }

const char *aom_codec_version_str(void) { return VERSION_STRING_NOSP; }

const char *aom_codec_version_extra_str(void) { return VERSION_EXTRA; }

const char *aom_codec_iface_name(aom_codec_iface_t *iface) {
  return iface ? iface->name : "<invalid interface>";
}

const char *aom_codec_err_to_string(aom_codec_err_t err) {
  switch (err) {
    case AOM_CODEC_OK: return "Success";
    case AOM_CODEC_ERROR: return "Unspecified internal error";
    case AOM_CODEC_MEM_ERROR: return "Memory allocation error";
    case AOM_CODEC_ABI_MISMATCH: return "ABI version mismatch";
    case AOM_CODEC_INCAPABLE:
      return "Codec does not implement requested capability";
    case AOM_CODEC_UNSUP_BITSTREAM:
      return "Bitstream not supported by this decoder";
    case AOM_CODEC_UNSUP_FEATURE:
      return "Bitstream required feature not supported by this decoder";
    case AOM_CODEC_CORRUPT_FRAME: return "Corrupt frame detected";
    case AOM_CODEC_INVALID_PARAM: return "Invalid parameter";
    case AOM_CODEC_LIST_END: return "End of iterated list";
  }

  return "Unrecognized error code";
}

const char *aom_codec_error(aom_codec_ctx_t *ctx) {
  return (ctx) ? aom_codec_err_to_string(ctx->err)
               : aom_codec_err_to_string(AOM_CODEC_INVALID_PARAM);
}

const char *aom_codec_error_detail(aom_codec_ctx_t *ctx) {
  if (ctx && ctx->err)
    return ctx->priv ? ctx->priv->err_detail : ctx->err_detail;

  return NULL;
}

aom_codec_err_t aom_codec_destroy(aom_codec_ctx_t *ctx) {
  aom_codec_err_t res;

  if (!ctx)
    res = AOM_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv)
    res = AOM_CODEC_ERROR;
  else {
    ctx->iface->destroy((aom_codec_alg_priv_t *)ctx->priv);

    ctx->iface = NULL;
    ctx->name = NULL;
    ctx->priv = NULL;
    res = AOM_CODEC_OK;
  }

  return SAVE_STATUS(ctx, res);
}

aom_codec_caps_t aom_codec_get_caps(aom_codec_iface_t *iface) {
  return (iface) ? iface->caps : 0;
}

aom_codec_err_t aom_codec_control_(aom_codec_ctx_t *ctx, int ctrl_id, ...) {
  aom_codec_err_t res;

  if (!ctx || !ctrl_id)
    res = AOM_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv || !ctx->iface->ctrl_maps)
    res = AOM_CODEC_ERROR;
  else {
    aom_codec_ctrl_fn_map_t *entry;

    res = AOM_CODEC_ERROR;

    for (entry = ctx->iface->ctrl_maps; entry && entry->fn; entry++) {
      if (!entry->ctrl_id || entry->ctrl_id == ctrl_id) {
        va_list ap;

        va_start(ap, ctrl_id);
        res = entry->fn((aom_codec_alg_priv_t *)ctx->priv, ap);
        va_end(ap);
        break;
      }
    }
  }

  return SAVE_STATUS(ctx, res);
}

void aom_internal_error(struct aom_internal_error_info *info,
                        aom_codec_err_t error, const char *fmt, ...) {
  va_list ap;

  info->error_code = error;
  info->has_detail = 0;

  if (fmt) {
    size_t sz = sizeof(info->detail);

    info->has_detail = 1;
    va_start(ap, fmt);
    vsnprintf(info->detail, sz - 1, fmt, ap);
    va_end(ap);
    info->detail[sz - 1] = '\0';
  }

  if (info->setjmp) longjmp(info->jmp, info->error_code);
}

void aom_merge_corrupted_flag(int *corrupted, int value) {
  *corrupted |= value;
}

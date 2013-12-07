/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*!\file
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <stdarg.h>
#include <stdlib.h>
#include "vpx/vpx_integer.h"
#include "vpx/internal/vpx_codec_internal.h"
#include "vpx_version.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

int vpx_codec_version(void) {
  return VERSION_PACKED;
}


const char *vpx_codec_version_str(void) {
  return VERSION_STRING_NOSP;
}


const char *vpx_codec_version_extra_str(void) {
  return VERSION_EXTRA;
}


const char *vpx_codec_iface_name(vpx_codec_iface_t *iface) {
  return iface ? iface->name : "<invalid interface>";
}

const char *vpx_codec_err_to_string(vpx_codec_err_t  err) {
  switch (err) {
    case VPX_CODEC_OK:
      return "Success";
    case VPX_CODEC_ERROR:
      return "Unspecified internal error";
    case VPX_CODEC_MEM_ERROR:
      return "Memory allocation error";
    case VPX_CODEC_ABI_MISMATCH:
      return "ABI version mismatch";
    case VPX_CODEC_INCAPABLE:
      return "Codec does not implement requested capability";
    case VPX_CODEC_UNSUP_BITSTREAM:
      return "Bitstream not supported by this decoder";
    case VPX_CODEC_UNSUP_FEATURE:
      return "Bitstream required feature not supported by this decoder";
    case VPX_CODEC_CORRUPT_FRAME:
      return "Corrupt frame detected";
    case  VPX_CODEC_INVALID_PARAM:
      return "Invalid parameter";
    case VPX_CODEC_LIST_END:
      return "End of iterated list";
  }

  return "Unrecognized error code";
}

const char *vpx_codec_error(vpx_codec_ctx_t  *ctx) {
  return (ctx) ? vpx_codec_err_to_string(ctx->err)
         : vpx_codec_err_to_string(VPX_CODEC_INVALID_PARAM);
}

const char *vpx_codec_error_detail(vpx_codec_ctx_t  *ctx) {
  if (ctx && ctx->err)
    return ctx->priv ? ctx->priv->err_detail : ctx->err_detail;

  return NULL;
}


vpx_codec_err_t vpx_codec_destroy(vpx_codec_ctx_t *ctx) {
  vpx_codec_err_t res;

  if (!ctx)
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv)
    res = VPX_CODEC_ERROR;
  else {
    if (ctx->priv->alg_priv)
      ctx->iface->destroy(ctx->priv->alg_priv);

    ctx->iface = NULL;
    ctx->name = NULL;
    ctx->priv = NULL;
    res = VPX_CODEC_OK;
  }

  return SAVE_STATUS(ctx, res);
}


vpx_codec_caps_t vpx_codec_get_caps(vpx_codec_iface_t *iface) {
  return (iface) ? iface->caps : 0;
}


vpx_codec_err_t vpx_codec_control_(vpx_codec_ctx_t  *ctx,
                                   int               ctrl_id,
                                   ...) {
  vpx_codec_err_t res;

  if (!ctx || !ctrl_id)
    res = VPX_CODEC_INVALID_PARAM;
  else if (!ctx->iface || !ctx->priv || !ctx->iface->ctrl_maps)
    res = VPX_CODEC_ERROR;
  else {
    vpx_codec_ctrl_fn_map_t *entry;

    res = VPX_CODEC_ERROR;

    for (entry = ctx->iface->ctrl_maps; entry && entry->fn; entry++) {
      if (!entry->ctrl_id || entry->ctrl_id == ctrl_id) {
        va_list  ap;

        va_start(ap, ctrl_id);
        res = entry->fn(ctx->priv->alg_priv, ctrl_id, ap);
        va_end(ap);
        break;
      }
    }
  }

  return SAVE_STATUS(ctx, res);
}

//------------------------------------------------------------------------------
// mmap interface

vpx_codec_err_t vpx_mmap_alloc(vpx_codec_mmap_t *mmap) {
  unsigned int align = mmap->align ? mmap->align - 1 : 0;

  if (mmap->flags & VPX_CODEC_MEM_ZERO)
    mmap->priv = calloc(1, mmap->sz + align);
  else
    mmap->priv = malloc(mmap->sz + align);

  if (mmap->priv == NULL) return VPX_CODEC_MEM_ERROR;
  mmap->base = (void *)((((uintptr_t)mmap->priv) + align) & ~(uintptr_t)align);
  mmap->dtor = vpx_mmap_dtor;
  return VPX_CODEC_OK;
}

void vpx_mmap_dtor(vpx_codec_mmap_t *mmap) {
  free(mmap->priv);
}

vpx_codec_err_t vpx_validate_mmaps(const vpx_codec_stream_info_t *si,
                                   const vpx_codec_mmap_t *mmaps,
                                   const mem_req_t *mem_reqs, int nreqs,
                                   vpx_codec_flags_t init_flags) {
  int i;

  for (i = 0; i < nreqs - 1; ++i) {
    /* Ensure the segment has been allocated */
    if (mmaps[i].base == NULL) {
      return VPX_CODEC_MEM_ERROR;
    }

    /* Verify variable size segment is big enough for the current si. */
    if (mem_reqs[i].calc_sz != NULL) {
      vpx_codec_dec_cfg_t cfg;

      cfg.w = si->w;
      cfg.h = si->h;

      if (mmaps[i].sz < mem_reqs[i].calc_sz(&cfg, init_flags)) {
        return VPX_CODEC_MEM_ERROR;
      }
    }
  }
  return VPX_CODEC_OK;
}

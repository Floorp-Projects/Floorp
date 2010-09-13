/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*!\file vpx_encoder.c
 * \brief Provides the high level interface to wrap encoder algorithms.
 *
 */
#include <limits.h>
#include <string.h>
#include "vpx/internal/vpx_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

vpx_codec_err_t vpx_codec_enc_init_ver(vpx_codec_ctx_t      *ctx,
                                       vpx_codec_iface_t    *iface,
                                       vpx_codec_enc_cfg_t  *cfg,
                                       vpx_codec_flags_t     flags,
                                       int                   ver)
{
    vpx_codec_err_t res;

    if (ver != VPX_ENCODER_ABI_VERSION)
        res = VPX_CODEC_ABI_MISMATCH;
    else if (!ctx || !iface || !cfg)
        res = VPX_CODEC_INVALID_PARAM;
    else if (iface->abi_version != VPX_CODEC_INTERNAL_ABI_VERSION)
        res = VPX_CODEC_ABI_MISMATCH;
    else if (!(iface->caps & VPX_CODEC_CAP_ENCODER))
        res = VPX_CODEC_INCAPABLE;
    else if ((flags & VPX_CODEC_USE_XMA) && !(iface->caps & VPX_CODEC_CAP_XMA))
        res = VPX_CODEC_INCAPABLE;
    else if ((flags & VPX_CODEC_USE_PSNR)
             && !(iface->caps & VPX_CODEC_CAP_PSNR))
        res = VPX_CODEC_INCAPABLE;
    else
    {
        ctx->iface = iface;
        ctx->name = iface->name;
        ctx->priv = NULL;
        ctx->init_flags = flags;
        ctx->config.enc = cfg;
        res = ctx->iface->init(ctx);

        if (res)
        {
            ctx->err_detail = ctx->priv ? ctx->priv->err_detail : NULL;
            vpx_codec_destroy(ctx);
        }

        if (ctx->priv)
            ctx->priv->iface = ctx->iface;
    }

    return SAVE_STATUS(ctx, res);
}



vpx_codec_err_t  vpx_codec_enc_config_default(vpx_codec_iface_t    *iface,
        vpx_codec_enc_cfg_t  *cfg,
        unsigned int          usage)
{
    vpx_codec_err_t res;
    vpx_codec_enc_cfg_map_t *map;

    if (!iface || !cfg || usage > INT_MAX)
        res = VPX_CODEC_INVALID_PARAM;
    else if (!(iface->caps & VPX_CODEC_CAP_ENCODER))
        res = VPX_CODEC_INCAPABLE;
    else
    {
        res = VPX_CODEC_INVALID_PARAM;

        for (map = iface->enc.cfg_maps; map->usage >= 0; map++)
        {
            if (map->usage == (int)usage)
            {
                *cfg = map->cfg;
                cfg->g_usage = usage;
                res = VPX_CODEC_OK;
                break;
            }
        }
    }

    return res;
}


#if ARCH_X86 || ARCH_X86_64
/* On X86, disable the x87 unit's internal 80 bit precision for better
 * consistency with the SSE unit's 64 bit precision.
 */
#include "vpx_ports/x86.h"
#define FLOATING_POINT_INIT() do {\
        unsigned short x87_orig_mode = x87_set_double_precision();
#define FLOATING_POINT_RESTORE() \
    x87_set_control_word(x87_orig_mode); }while(0)


#else
static void FLOATING_POINT_INIT() {}
static void FLOATING_POINT_RESTORE() {}
#endif


vpx_codec_err_t  vpx_codec_encode(vpx_codec_ctx_t            *ctx,
                                  const vpx_image_t          *img,
                                  vpx_codec_pts_t             pts,
                                  unsigned long               duration,
                                  vpx_enc_frame_flags_t       flags,
                                  unsigned long               deadline)
{
    vpx_codec_err_t res;

    if (!ctx || (img && !duration))
        res = VPX_CODEC_INVALID_PARAM;
    else if (!ctx->iface || !ctx->priv)
        res = VPX_CODEC_ERROR;
    else if (!(ctx->iface->caps & VPX_CODEC_CAP_ENCODER))
        res = VPX_CODEC_INCAPABLE;
    else
    {
        /* Execute in a normalized floating point environment, if the platform
         * requires it.
         */
        FLOATING_POINT_INIT();
        res = ctx->iface->enc.encode(ctx->priv->alg_priv, img, pts,
                                     duration, flags, deadline);
        FLOATING_POINT_RESTORE();
    }

    return SAVE_STATUS(ctx, res);
}


const vpx_codec_cx_pkt_t *vpx_codec_get_cx_data(vpx_codec_ctx_t   *ctx,
        vpx_codec_iter_t  *iter)
{
    const vpx_codec_cx_pkt_t *pkt = NULL;

    if (ctx)
    {
        if (!iter)
            ctx->err = VPX_CODEC_INVALID_PARAM;
        else if (!ctx->iface || !ctx->priv)
            ctx->err = VPX_CODEC_ERROR;
        else if (!(ctx->iface->caps & VPX_CODEC_CAP_ENCODER))
            ctx->err = VPX_CODEC_INCAPABLE;
        else
            pkt = ctx->iface->enc.get_cx_data(ctx->priv->alg_priv, iter);
    }

    if (pkt && pkt->kind == VPX_CODEC_CX_FRAME_PKT)
    {
        /* If the application has specified a destination area for the
         * compressed data, and the codec has not placed the data there,
         * and it fits, copy it.
         */
        char *dst_buf = ctx->priv->enc.cx_data_dst_buf.buf;

        if (dst_buf
            && pkt->data.raw.buf != dst_buf
            && pkt->data.raw.sz
            + ctx->priv->enc.cx_data_pad_before
            + ctx->priv->enc.cx_data_pad_after
            <= ctx->priv->enc.cx_data_dst_buf.sz)
        {
            vpx_codec_cx_pkt_t *modified_pkt = &ctx->priv->enc.cx_data_pkt;

            memcpy(dst_buf + ctx->priv->enc.cx_data_pad_before,
                   pkt->data.raw.buf, pkt->data.raw.sz);
            *modified_pkt = *pkt;
            modified_pkt->data.raw.buf = dst_buf;
            modified_pkt->data.raw.sz += ctx->priv->enc.cx_data_pad_before
                                         + ctx->priv->enc.cx_data_pad_after;
            pkt = modified_pkt;
        }

        if (dst_buf == pkt->data.raw.buf)
        {
            ctx->priv->enc.cx_data_dst_buf.buf = dst_buf + pkt->data.raw.sz;
            ctx->priv->enc.cx_data_dst_buf.sz -= pkt->data.raw.sz;
        }
    }

    return pkt;
}


vpx_codec_err_t vpx_codec_set_cx_data_buf(vpx_codec_ctx_t       *ctx,
        const vpx_fixed_buf_t *buf,
        unsigned int           pad_before,
        unsigned int           pad_after)
{
    if (!ctx || !ctx->priv)
        return VPX_CODEC_INVALID_PARAM;

    if (buf)
    {
        ctx->priv->enc.cx_data_dst_buf = *buf;
        ctx->priv->enc.cx_data_pad_before = pad_before;
        ctx->priv->enc.cx_data_pad_after = pad_after;
    }
    else
    {
        ctx->priv->enc.cx_data_dst_buf.buf = NULL;
        ctx->priv->enc.cx_data_dst_buf.sz = 0;
        ctx->priv->enc.cx_data_pad_before = 0;
        ctx->priv->enc.cx_data_pad_after = 0;
    }

    return VPX_CODEC_OK;
}


const vpx_image_t *vpx_codec_get_preview_frame(vpx_codec_ctx_t   *ctx)
{
    vpx_image_t *img = NULL;

    if (ctx)
    {
        if (!ctx->iface || !ctx->priv)
            ctx->err = VPX_CODEC_ERROR;
        else if (!(ctx->iface->caps & VPX_CODEC_CAP_ENCODER))
            ctx->err = VPX_CODEC_INCAPABLE;
        else if (!ctx->iface->enc.get_preview)
            ctx->err = VPX_CODEC_INCAPABLE;
        else
            img = ctx->iface->enc.get_preview(ctx->priv->alg_priv);
    }

    return img;
}


vpx_fixed_buf_t *vpx_codec_get_global_headers(vpx_codec_ctx_t   *ctx)
{
    vpx_fixed_buf_t *buf = NULL;

    if (ctx)
    {
        if (!ctx->iface || !ctx->priv)
            ctx->err = VPX_CODEC_ERROR;
        else if (!(ctx->iface->caps & VPX_CODEC_CAP_ENCODER))
            ctx->err = VPX_CODEC_INCAPABLE;
        else if (!ctx->iface->enc.get_glob_hdrs)
            ctx->err = VPX_CODEC_INCAPABLE;
        else
            buf = ctx->iface->enc.get_glob_hdrs(ctx->priv->alg_priv);
    }

    return buf;
}


vpx_codec_err_t  vpx_codec_enc_config_set(vpx_codec_ctx_t            *ctx,
        const vpx_codec_enc_cfg_t  *cfg)
{
    vpx_codec_err_t res;

    if (!ctx || !ctx->iface || !ctx->priv || !cfg)
        res = VPX_CODEC_INVALID_PARAM;
    else if (!(ctx->iface->caps & VPX_CODEC_CAP_ENCODER))
        res = VPX_CODEC_INCAPABLE;
    else
        res = ctx->iface->enc.cfg_set(ctx->priv->alg_priv, cfg);

    return SAVE_STATUS(ctx, res);
}


int vpx_codec_pkt_list_add(struct vpx_codec_pkt_list *list,
                           const struct vpx_codec_cx_pkt *pkt)
{
    if (list->cnt < list->max)
    {
        list->pkts[list->cnt++] = *pkt;
        return 0;
    }

    return 1;
}


const vpx_codec_cx_pkt_t *vpx_codec_pkt_list_get(struct vpx_codec_pkt_list *list,
        vpx_codec_iter_t           *iter)
{
    const vpx_codec_cx_pkt_t *pkt;

    if (!(*iter))
    {
        *iter = list->pkts;
    }

    pkt = (const void *) * iter;

    if (pkt - list->pkts < list->cnt)
        *iter = pkt + 1;
    else
        pkt = NULL;

    return pkt;
}

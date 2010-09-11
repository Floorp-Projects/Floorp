/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*!\file vpx_decoder.c
 * \brief Provides the high level interface to wrap decoder algorithms.
 *
 */
#include <stdlib.h>
#include <string.h>
#include "vpx/vpx_decoder.h"
#include "vpx/internal/vpx_codec_internal.h"

#define SAVE_STATUS(ctx,var) (ctx?(ctx->err = var):var)

const char *vpx_dec_iface_name(vpx_dec_iface_t *iface)
{
    return vpx_codec_iface_name((vpx_codec_iface_t *)iface);
}

const char *vpx_dec_err_to_string(vpx_dec_err_t  err)
{
    return vpx_codec_err_to_string(err);
}

const char *vpx_dec_error(vpx_dec_ctx_t  *ctx)
{
    return vpx_codec_error((vpx_codec_ctx_t *)ctx);
}

const char *vpx_dec_error_detail(vpx_dec_ctx_t  *ctx)
{
    return vpx_codec_error_detail((vpx_codec_ctx_t *)ctx);
}


vpx_dec_err_t vpx_dec_init_ver(vpx_dec_ctx_t    *ctx,
                               vpx_dec_iface_t  *iface,
                               int               ver)
{
    return vpx_codec_dec_init_ver((vpx_codec_ctx_t *)ctx,
                                  (vpx_codec_iface_t *)iface,
                                  NULL,
                                  0,
                                  ver);
}


vpx_dec_err_t vpx_dec_destroy(vpx_dec_ctx_t *ctx)
{
    return vpx_codec_destroy((vpx_codec_ctx_t *)ctx);
}


vpx_dec_caps_t vpx_dec_get_caps(vpx_dec_iface_t *iface)
{
    return vpx_codec_get_caps((vpx_codec_iface_t *)iface);
}


vpx_dec_err_t vpx_dec_peek_stream_info(vpx_dec_iface_t       *iface,
                                       const uint8_t         *data,
                                       unsigned int           data_sz,
                                       vpx_dec_stream_info_t *si)
{
    return vpx_codec_peek_stream_info((vpx_codec_iface_t *)iface, data, data_sz,
                                      (vpx_codec_stream_info_t *)si);
}


vpx_dec_err_t vpx_dec_get_stream_info(vpx_dec_ctx_t         *ctx,
                                      vpx_dec_stream_info_t *si)
{
    return vpx_codec_get_stream_info((vpx_codec_ctx_t *)ctx,
                                     (vpx_codec_stream_info_t *)si);
}


vpx_dec_err_t vpx_dec_control(vpx_dec_ctx_t  *ctx,
                              int             ctrl_id,
                              void           *data)
{
    return vpx_codec_control_((vpx_codec_ctx_t *)ctx, ctrl_id, data);
}


vpx_dec_err_t vpx_dec_decode(vpx_dec_ctx_t  *ctx,
                             uint8_t        *data,
                             unsigned int    data_sz,
                             void       *user_priv,
                             int         rel_pts)
{
    (void)rel_pts;
    return vpx_codec_decode((vpx_codec_ctx_t *)ctx, data, data_sz, user_priv,
                            0);
}

vpx_image_t *vpx_dec_get_frame(vpx_dec_ctx_t  *ctx,
                               vpx_dec_iter_t *iter)
{
    return vpx_codec_get_frame((vpx_codec_ctx_t *)ctx, iter);
}


vpx_dec_err_t vpx_dec_register_put_frame_cb(vpx_dec_ctx_t             *ctx,
        vpx_dec_put_frame_cb_fn_t  cb,
        void                      *user_priv)
{
    return vpx_codec_register_put_frame_cb((vpx_codec_ctx_t *)ctx, cb,
                                           user_priv);
}


vpx_dec_err_t vpx_dec_register_put_slice_cb(vpx_dec_ctx_t             *ctx,
        vpx_dec_put_slice_cb_fn_t  cb,
        void                      *user_priv)
{
    return vpx_codec_register_put_slice_cb((vpx_codec_ctx_t *)ctx, cb,
                                           user_priv);
}


vpx_dec_err_t vpx_dec_xma_init_ver(vpx_dec_ctx_t    *ctx,
                                   vpx_dec_iface_t  *iface,
                                   int               ver)
{
    return vpx_codec_dec_init_ver((vpx_codec_ctx_t *)ctx,
                                  (vpx_codec_iface_t *)iface,
                                  NULL,
                                  VPX_CODEC_USE_XMA,
                                  ver);
}

vpx_dec_err_t vpx_dec_get_mem_map(vpx_dec_ctx_t                *ctx_,
                                  vpx_dec_mmap_t               *mmap,
                                  const vpx_dec_stream_info_t  *si,
                                  vpx_dec_iter_t               *iter)
{
    vpx_codec_ctx_t   *ctx = (vpx_codec_ctx_t *)ctx_;
    vpx_dec_err_t      res = VPX_DEC_OK;

    if (!ctx || !mmap || !si || !iter || !ctx->iface)
        res = VPX_DEC_INVALID_PARAM;
    else if (!(ctx->iface->caps & VPX_DEC_CAP_XMA))
        res = VPX_DEC_ERROR;
    else
    {
        if (!ctx->config.dec)
        {
            ctx->config.dec = malloc(sizeof(vpx_codec_dec_cfg_t));
            ctx->config.dec->w = si->w;
            ctx->config.dec->h = si->h;
        }

        res = ctx->iface->get_mmap(ctx, mmap, iter);
    }

    return SAVE_STATUS(ctx, res);
}


vpx_dec_err_t vpx_dec_set_mem_map(vpx_dec_ctx_t   *ctx_,
                                  vpx_dec_mmap_t  *mmap,
                                  unsigned int     num_maps)
{
    vpx_codec_ctx_t   *ctx = (vpx_codec_ctx_t *)ctx_;
    vpx_dec_err_t      res = VPX_DEC_MEM_ERROR;

    if (!ctx || !mmap || !ctx->iface)
        res = VPX_DEC_INVALID_PARAM;
    else if (!(ctx->iface->caps & VPX_DEC_CAP_XMA))
        res = VPX_DEC_ERROR;
    else
    {
        void         *save = (ctx->priv) ? NULL : ctx->config.dec;
        unsigned int i;

        for (i = 0; i < num_maps; i++, mmap++)
        {
            if (!mmap->base)
                break;

            /* Everything look ok, set the mmap in the decoder */
            res = ctx->iface->set_mmap(ctx, mmap);

            if (res)
                break;
        }

        if (save) free(save);
    }

    return SAVE_STATUS(ctx, res);
}

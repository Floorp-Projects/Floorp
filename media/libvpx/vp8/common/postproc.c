/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include "vp8_rtcd.h"
#include "vpx_scale_rtcd.h"
#include "vpx_scale/yv12config.h"
#include "postproc.h"
#include "common.h"
#include "vpx_scale/vpx_scale.h"
#include "systemdependent.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define RGB_TO_YUV(t)                                                                       \
    ( (0.257*(float)(t>>16)) + (0.504*(float)(t>>8&0xff)) + (0.098*(float)(t&0xff)) + 16),  \
    (-(0.148*(float)(t>>16)) - (0.291*(float)(t>>8&0xff)) + (0.439*(float)(t&0xff)) + 128), \
    ( (0.439*(float)(t>>16)) - (0.368*(float)(t>>8&0xff)) - (0.071*(float)(t&0xff)) + 128)

/* global constants */
#if CONFIG_POSTPROC_VISUALIZER
static const unsigned char MB_PREDICTION_MODE_colors[MB_MODE_COUNT][3] =
{
    { RGB_TO_YUV(0x98FB98) },   /* PaleGreen */
    { RGB_TO_YUV(0x00FF00) },   /* Green */
    { RGB_TO_YUV(0xADFF2F) },   /* GreenYellow */
    { RGB_TO_YUV(0x228B22) },   /* ForestGreen */
    { RGB_TO_YUV(0x006400) },   /* DarkGreen */
    { RGB_TO_YUV(0x98F5FF) },   /* Cadet Blue */
    { RGB_TO_YUV(0x6CA6CD) },   /* Sky Blue */
    { RGB_TO_YUV(0x00008B) },   /* Dark blue */
    { RGB_TO_YUV(0x551A8B) },   /* Purple */
    { RGB_TO_YUV(0xFF0000) }    /* Red */
};

static const unsigned char B_PREDICTION_MODE_colors[B_MODE_COUNT][3] =
{
    { RGB_TO_YUV(0x6633ff) },   /* Purple */
    { RGB_TO_YUV(0xcc33ff) },   /* Magenta */
    { RGB_TO_YUV(0xff33cc) },   /* Pink */
    { RGB_TO_YUV(0xff3366) },   /* Coral */
    { RGB_TO_YUV(0x3366ff) },   /* Blue */
    { RGB_TO_YUV(0xed00f5) },   /* Dark Blue */
    { RGB_TO_YUV(0x2e00b8) },   /* Dark Purple */
    { RGB_TO_YUV(0xff6633) },   /* Orange */
    { RGB_TO_YUV(0x33ccff) },   /* Light Blue */
    { RGB_TO_YUV(0x8ab800) },   /* Green */
    { RGB_TO_YUV(0xffcc33) },   /* Light Orange */
    { RGB_TO_YUV(0x33ffcc) },   /* Aqua */
    { RGB_TO_YUV(0x66ff33) },   /* Light Green */
    { RGB_TO_YUV(0xccff33) },   /* Yellow */
};

static const unsigned char MV_REFERENCE_FRAME_colors[MAX_REF_FRAMES][3] =
{
    { RGB_TO_YUV(0x00ff00) },   /* Blue */
    { RGB_TO_YUV(0x0000ff) },   /* Green */
    { RGB_TO_YUV(0xffff00) },   /* Yellow */
    { RGB_TO_YUV(0xff0000) },   /* Red */
};
#endif

const short vp8_rv[] =
{
    8, 5, 2, 2, 8, 12, 4, 9, 8, 3,
    0, 3, 9, 0, 0, 0, 8, 3, 14, 4,
    10, 1, 11, 14, 1, 14, 9, 6, 12, 11,
    8, 6, 10, 0, 0, 8, 9, 0, 3, 14,
    8, 11, 13, 4, 2, 9, 0, 3, 9, 6,
    1, 2, 3, 14, 13, 1, 8, 2, 9, 7,
    3, 3, 1, 13, 13, 6, 6, 5, 2, 7,
    11, 9, 11, 8, 7, 3, 2, 0, 13, 13,
    14, 4, 12, 5, 12, 10, 8, 10, 13, 10,
    4, 14, 4, 10, 0, 8, 11, 1, 13, 7,
    7, 14, 6, 14, 13, 2, 13, 5, 4, 4,
    0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
    8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
    3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
    3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
    13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
    5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
    9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
    4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
    3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
    11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
    5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
    0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
    10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
    4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
    0, 10, 0, 5, 13, 2, 12, 7, 11, 13,
    8, 0, 4, 10, 7, 2, 7, 2, 2, 5,
    3, 4, 7, 3, 3, 14, 14, 5, 9, 13,
    3, 14, 3, 6, 3, 0, 11, 8, 13, 1,
    13, 1, 12, 0, 10, 9, 7, 6, 2, 8,
    5, 2, 13, 7, 1, 13, 14, 7, 6, 7,
    9, 6, 10, 11, 7, 8, 7, 5, 14, 8,
    4, 4, 0, 8, 7, 10, 0, 8, 14, 11,
    3, 12, 5, 7, 14, 3, 14, 5, 2, 6,
    11, 12, 12, 8, 0, 11, 13, 1, 2, 0,
    5, 10, 14, 7, 8, 0, 4, 11, 0, 8,
    0, 3, 10, 5, 8, 0, 11, 6, 7, 8,
    10, 7, 13, 9, 2, 5, 1, 5, 10, 2,
    4, 3, 5, 6, 10, 8, 9, 4, 11, 14,
    3, 8, 3, 7, 8, 5, 11, 4, 12, 3,
    11, 9, 14, 8, 14, 13, 4, 3, 1, 2,
    14, 6, 5, 4, 4, 11, 4, 6, 2, 1,
    5, 8, 8, 12, 13, 5, 14, 10, 12, 13,
    0, 9, 5, 5, 11, 10, 13, 9, 10, 13,
};

extern void vp8_blit_text(const char *msg, unsigned char *address, const int pitch);
extern void vp8_blit_line(int x0, int x1, int y0, int y1, unsigned char *image, const int pitch);
/***********************************************************************************************************
 */
void vp8_post_proc_down_and_across_mb_row_c
(
    unsigned char *src_ptr,
    unsigned char *dst_ptr,
    int src_pixels_per_line,
    int dst_pixels_per_line,
    int cols,
    unsigned char *f,
    int size
)
{
    unsigned char *p_src, *p_dst;
    int row;
    int col;
    unsigned char v;
    unsigned char d[4];

    for (row = 0; row < size; row++)
    {
        /* post_proc_down for one row */
        p_src = src_ptr;
        p_dst = dst_ptr;

        for (col = 0; col < cols; col++)
        {
            unsigned char p_above2 = p_src[col - 2 * src_pixels_per_line];
            unsigned char p_above1 = p_src[col - src_pixels_per_line];
            unsigned char p_below1 = p_src[col + src_pixels_per_line];
            unsigned char p_below2 = p_src[col + 2 * src_pixels_per_line];

            v = p_src[col];

            if ((abs(v - p_above2) < f[col]) && (abs(v - p_above1) < f[col])
                && (abs(v - p_below1) < f[col]) && (abs(v - p_below2) < f[col]))
            {
                unsigned char k1, k2, k3;
                k1 = (p_above2 + p_above1 + 1) >> 1;
                k2 = (p_below2 + p_below1 + 1) >> 1;
                k3 = (k1 + k2 + 1) >> 1;
                v = (k3 + v + 1) >> 1;
            }

            p_dst[col] = v;
        }

        /* now post_proc_across */
        p_src = dst_ptr;
        p_dst = dst_ptr;

        p_src[-2] = p_src[-1] = p_src[0];
        p_src[cols] = p_src[cols + 1] = p_src[cols - 1];

        for (col = 0; col < cols; col++)
        {
            v = p_src[col];

            if ((abs(v - p_src[col - 2]) < f[col])
                && (abs(v - p_src[col - 1]) < f[col])
                && (abs(v - p_src[col + 1]) < f[col])
                && (abs(v - p_src[col + 2]) < f[col]))
            {
                unsigned char k1, k2, k3;
                k1 = (p_src[col - 2] + p_src[col - 1] + 1) >> 1;
                k2 = (p_src[col + 2] + p_src[col + 1] + 1) >> 1;
                k3 = (k1 + k2 + 1) >> 1;
                v = (k3 + v + 1) >> 1;
            }

            d[col & 3] = v;

            if (col >= 2)
                p_dst[col - 2] = d[(col - 2) & 3];
        }

        /* handle the last two pixels */
        p_dst[col - 2] = d[(col - 2) & 3];
        p_dst[col - 1] = d[(col - 1) & 3];

        /* next row */
        src_ptr += src_pixels_per_line;
        dst_ptr += dst_pixels_per_line;
    }
}

static int q2mbl(int x)
{
    if (x < 20) x = 20;

    x = 50 + (x - 50) * 10 / 8;
    return x * x / 3;
}

void vp8_mbpost_proc_across_ip_c(unsigned char *src, int pitch, int rows, int cols, int flimit)
{
    int r, c, i;

    unsigned char *s = src;
    unsigned char d[16];

    for (r = 0; r < rows; r++)
    {
        int sumsq = 0;
        int sum   = 0;

        for (i = -8; i < 0; i++)
          s[i]=s[0];

        /* 17 avoids valgrind warning - we buffer values in c in d
         * and only write them when we've read 8 ahead...
         */
        for (i = 0; i < 17; i++)
          s[i+cols]=s[cols-1];

        for (i = -8; i <= 6; i++)
        {
            sumsq += s[i] * s[i];
            sum   += s[i];
            d[i+8] = 0;
        }

        for (c = 0; c < cols + 8; c++)
        {
            int x = s[c+7] - s[c-8];
            int y = s[c+7] + s[c-8];

            sum  += x;
            sumsq += x * y;

            d[c&15] = s[c];

            if (sumsq * 15 - sum * sum < flimit)
            {
                d[c&15] = (8 + sum + s[c]) >> 4;
            }

            s[c-8] = d[(c-8)&15];
        }

        s += pitch;
    }
}

void vp8_mbpost_proc_down_c(unsigned char *dst, int pitch, int rows, int cols, int flimit)
{
    int r, c, i;
    const short *rv3 = &vp8_rv[63&rand()];

    for (c = 0; c < cols; c++ )
    {
        unsigned char *s = &dst[c];
        int sumsq = 0;
        int sum   = 0;
        unsigned char d[16];
        const short *rv2 = rv3 + ((c * 17) & 127);

        for (i = -8; i < 0; i++)
          s[i*pitch]=s[0];

        /* 17 avoids valgrind warning - we buffer values in c in d
         * and only write them when we've read 8 ahead...
         */
        for (i = 0; i < 17; i++)
          s[(i+rows)*pitch]=s[(rows-1)*pitch];

        for (i = -8; i <= 6; i++)
        {
            sumsq += s[i*pitch] * s[i*pitch];
            sum   += s[i*pitch];
        }

        for (r = 0; r < rows + 8; r++)
        {
            sumsq += s[7*pitch] * s[ 7*pitch] - s[-8*pitch] * s[-8*pitch];
            sum  += s[7*pitch] - s[-8*pitch];
            d[r&15] = s[0];

            if (sumsq * 15 - sum * sum < flimit)
            {
                d[r&15] = (rv2[r&127] + sum + s[0]) >> 4;
            }
            if (r >= 8)
              s[-8*pitch] = d[(r-8)&15];
            s += pitch;
        }
    }
}

#if CONFIG_POSTPROC
static void vp8_de_mblock(YV12_BUFFER_CONFIG         *post,
                          int                         q)
{
    vp8_mbpost_proc_across_ip(post->y_buffer, post->y_stride, post->y_height,
                              post->y_width, q2mbl(q));
    vp8_mbpost_proc_down(post->y_buffer, post->y_stride, post->y_height,
                         post->y_width, q2mbl(q));
}

void vp8_deblock(VP8_COMMON                 *cm,
                 YV12_BUFFER_CONFIG         *source,
                 YV12_BUFFER_CONFIG         *post,
                 int                         q,
                 int                         low_var_thresh,
                 int                         flag)
{
    double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
    int ppl = (int)(level + .5);

    const MODE_INFO *mode_info_context = cm->show_frame_mi;
    int mbr, mbc;

    /* The pixel thresholds are adjusted according to if or not the macroblock
     * is a skipped block.  */
    unsigned char *ylimits = cm->pp_limits_buffer;
    unsigned char *uvlimits = cm->pp_limits_buffer + 16 * cm->mb_cols;
    (void) low_var_thresh;
    (void) flag;

    if (ppl > 0)
    {
        for (mbr = 0; mbr < cm->mb_rows; mbr++)
        {
            unsigned char *ylptr = ylimits;
            unsigned char *uvlptr = uvlimits;
            for (mbc = 0; mbc < cm->mb_cols; mbc++)
            {
                unsigned char mb_ppl;

                if (mode_info_context->mbmi.mb_skip_coeff)
                    mb_ppl = (unsigned char)ppl >> 1;
                else
                    mb_ppl = (unsigned char)ppl;

                vpx_memset(ylptr, mb_ppl, 16);
                vpx_memset(uvlptr, mb_ppl, 8);

                ylptr += 16;
                uvlptr += 8;
                mode_info_context++;
            }
            mode_info_context++;

            vp8_post_proc_down_and_across_mb_row(
                source->y_buffer + 16 * mbr * source->y_stride,
                post->y_buffer + 16 * mbr * post->y_stride, source->y_stride,
                post->y_stride, source->y_width, ylimits, 16);

            vp8_post_proc_down_and_across_mb_row(
                source->u_buffer + 8 * mbr * source->uv_stride,
                post->u_buffer + 8 * mbr * post->uv_stride, source->uv_stride,
                post->uv_stride, source->uv_width, uvlimits, 8);
            vp8_post_proc_down_and_across_mb_row(
                source->v_buffer + 8 * mbr * source->uv_stride,
                post->v_buffer + 8 * mbr * post->uv_stride, source->uv_stride,
                post->uv_stride, source->uv_width, uvlimits, 8);
        }
    } else
    {
        vp8_yv12_copy_frame(source, post);
    }
}
#endif

void vp8_de_noise(VP8_COMMON                 *cm,
                  YV12_BUFFER_CONFIG         *source,
                  YV12_BUFFER_CONFIG         *post,
                  int                         q,
                  int                         low_var_thresh,
                  int                         flag,
                  int                         uvfilter)
{
    int mbr;
    double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
    int ppl = (int)(level + .5);
    int mb_rows = cm->mb_rows;
    int mb_cols = cm->mb_cols;
    unsigned char *limits = cm->pp_limits_buffer;;
    (void) post;
    (void) low_var_thresh;
    (void) flag;

    vpx_memset(limits, (unsigned char)ppl, 16 * mb_cols);

    /* TODO: The original code don't filter the 2 outer rows and columns. */
    for (mbr = 0; mbr < mb_rows; mbr++)
    {
        vp8_post_proc_down_and_across_mb_row(
            source->y_buffer + 16 * mbr * source->y_stride,
            source->y_buffer + 16 * mbr * source->y_stride,
            source->y_stride, source->y_stride, source->y_width, limits, 16);
        if (uvfilter == 1) {
          vp8_post_proc_down_and_across_mb_row(
              source->u_buffer + 8 * mbr * source->uv_stride,
              source->u_buffer + 8 * mbr * source->uv_stride,
              source->uv_stride, source->uv_stride, source->uv_width, limits,
              8);
          vp8_post_proc_down_and_across_mb_row(
              source->v_buffer + 8 * mbr * source->uv_stride,
              source->v_buffer + 8 * mbr * source->uv_stride,
              source->uv_stride, source->uv_stride, source->uv_width, limits,
              8);
        }
    }
}

double vp8_gaussian(double sigma, double mu, double x)
{
    return 1 / (sigma * sqrt(2.0 * 3.14159265)) *
           (exp(-(x - mu) * (x - mu) / (2 * sigma * sigma)));
}

static void fillrd(struct postproc_state *state, int q, int a)
{
    char char_dist[300];

    double sigma;
    int i;

    vp8_clear_system_state();


    sigma = a + .5 + .6 * (63 - q) / 63.0;

    /* set up a lookup table of 256 entries that matches
     * a gaussian distribution with sigma determined by q.
     */
    {
        int next, j;

        next = 0;

        for (i = -32; i < 32; i++)
        {
            const int v = (int)(.5 + 256 * vp8_gaussian(sigma, 0, i));

            if (v)
            {
                for (j = 0; j < v; j++)
                {
                    char_dist[next+j] = (char) i;
                }

                next = next + j;
            }

        }

        for (; next < 256; next++)
            char_dist[next] = 0;

    }

    for (i = 0; i < 3072; i++)
    {
        state->noise[i] = char_dist[rand() & 0xff];
    }

    for (i = 0; i < 16; i++)
    {
        state->blackclamp[i] = -char_dist[0];
        state->whiteclamp[i] = -char_dist[0];
        state->bothclamp[i] = -2 * char_dist[0];
    }

    state->last_q = q;
    state->last_noise = a;
}

/****************************************************************************
 *
 *  ROUTINE       : plane_add_noise_c
 *
 *  INPUTS        : unsigned char *Start    starting address of buffer to add gaussian
 *                                  noise to
 *                  unsigned int Width    width of plane
 *                  unsigned int Height   height of plane
 *                  int  Pitch    distance between subsequent lines of frame
 *                  int  q        quantizer used to determine amount of noise
 *                                  to add
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void.
 *
 *  FUNCTION      : adds gaussian noise to a plane of pixels
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp8_plane_add_noise_c(unsigned char *Start, char *noise,
                           char blackclamp[16],
                           char whiteclamp[16],
                           char bothclamp[16],
                           unsigned int Width, unsigned int Height, int Pitch)
{
    unsigned int i, j;

    for (i = 0; i < Height; i++)
    {
        unsigned char *Pos = Start + i * Pitch;
        char  *Ref = (char *)(noise + (rand() & 0xff));

        for (j = 0; j < Width; j++)
        {
            if (Pos[j] < blackclamp[0])
                Pos[j] = blackclamp[0];

            if (Pos[j] > 255 + whiteclamp[0])
                Pos[j] = 255 + whiteclamp[0];

            Pos[j] += Ref[j];
        }
    }
}

/* Blend the macro block with a solid colored square.  Leave the
 * edges unblended to give distinction to macro blocks in areas
 * filled with the same color block.
 */
void vp8_blend_mb_inner_c (unsigned char *y, unsigned char *u, unsigned char *v,
                        int y_1, int u_1, int v_1, int alpha, int stride)
{
    int i, j;
    int y1_const = y_1*((1<<16)-alpha);
    int u1_const = u_1*((1<<16)-alpha);
    int v1_const = v_1*((1<<16)-alpha);

    y += 2*stride + 2;
    for (i = 0; i < 12; i++)
    {
        for (j = 0; j < 12; j++)
        {
            y[j] = (y[j]*alpha + y1_const)>>16;
        }
        y += stride;
    }

    stride >>= 1;

    u += stride + 1;
    v += stride + 1;

    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 6; j++)
        {
            u[j] = (u[j]*alpha + u1_const)>>16;
            v[j] = (v[j]*alpha + v1_const)>>16;
        }
        u += stride;
        v += stride;
    }
}

/* Blend only the edge of the macro block.  Leave center
 * unblended to allow for other visualizations to be layered.
 */
void vp8_blend_mb_outer_c (unsigned char *y, unsigned char *u, unsigned char *v,
                        int y_1, int u_1, int v_1, int alpha, int stride)
{
    int i, j;
    int y1_const = y_1*((1<<16)-alpha);
    int u1_const = u_1*((1<<16)-alpha);
    int v1_const = v_1*((1<<16)-alpha);

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 16; j++)
        {
            y[j] = (y[j]*alpha + y1_const)>>16;
        }
        y += stride;
    }

    for (i = 0; i < 12; i++)
    {
        y[0]  = (y[0]*alpha  + y1_const)>>16;
        y[1]  = (y[1]*alpha  + y1_const)>>16;
        y[14] = (y[14]*alpha + y1_const)>>16;
        y[15] = (y[15]*alpha + y1_const)>>16;
        y += stride;
    }

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 16; j++)
        {
            y[j] = (y[j]*alpha + y1_const)>>16;
        }
        y += stride;
    }

    stride >>= 1;

    for (j = 0; j < 8; j++)
    {
        u[j] = (u[j]*alpha + u1_const)>>16;
        v[j] = (v[j]*alpha + v1_const)>>16;
    }
    u += stride;
    v += stride;

    for (i = 0; i < 6; i++)
    {
        u[0] = (u[0]*alpha + u1_const)>>16;
        v[0] = (v[0]*alpha + v1_const)>>16;

        u[7] = (u[7]*alpha + u1_const)>>16;
        v[7] = (v[7]*alpha + v1_const)>>16;

        u += stride;
        v += stride;
    }

    for (j = 0; j < 8; j++)
    {
        u[j] = (u[j]*alpha + u1_const)>>16;
        v[j] = (v[j]*alpha + v1_const)>>16;
    }
}

void vp8_blend_b_c (unsigned char *y, unsigned char *u, unsigned char *v,
                        int y_1, int u_1, int v_1, int alpha, int stride)
{
    int i, j;
    int y1_const = y_1*((1<<16)-alpha);
    int u1_const = u_1*((1<<16)-alpha);
    int v1_const = v_1*((1<<16)-alpha);

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            y[j] = (y[j]*alpha + y1_const)>>16;
        }
        y += stride;
    }

    stride >>= 1;

    for (i = 0; i < 2; i++)
    {
        for (j = 0; j < 2; j++)
        {
            u[j] = (u[j]*alpha + u1_const)>>16;
            v[j] = (v[j]*alpha + v1_const)>>16;
        }
        u += stride;
        v += stride;
    }
}

static void constrain_line (int x_0, int *x_1, int y_0, int *y_1, int width, int height)
{
    int dx;
    int dy;

    if (*x_1 > width)
    {
        dx = *x_1 - x_0;
        dy = *y_1 - y_0;

        *x_1 = width;
        if (dx)
            *y_1 = ((width-x_0)*dy)/dx + y_0;
    }
    if (*x_1 < 0)
    {
        dx = *x_1 - x_0;
        dy = *y_1 - y_0;

        *x_1 = 0;
        if (dx)
            *y_1 = ((0-x_0)*dy)/dx + y_0;
    }
    if (*y_1 > height)
    {
        dx = *x_1 - x_0;
        dy = *y_1 - y_0;

        *y_1 = height;
        if (dy)
            *x_1 = ((height-y_0)*dx)/dy + x_0;
    }
    if (*y_1 < 0)
    {
        dx = *x_1 - x_0;
        dy = *y_1 - y_0;

        *y_1 = 0;
        if (dy)
            *x_1 = ((0-y_0)*dx)/dy + x_0;
    }
}

#if CONFIG_POSTPROC
int vp8_post_proc_frame(VP8_COMMON *oci, YV12_BUFFER_CONFIG *dest, vp8_ppflags_t *ppflags)
{
    int q = oci->filter_level * 10 / 6;
    int flags = ppflags->post_proc_flag;
    int deblock_level = ppflags->deblocking_level;
    int noise_level = ppflags->noise_level;

    if (!oci->frame_to_show)
        return -1;

    if (q > 63)
        q = 63;

    if (!flags)
    {
        *dest = *oci->frame_to_show;

        /* handle problem with extending borders */
        dest->y_width = oci->Width;
        dest->y_height = oci->Height;
        dest->uv_height = dest->y_height / 2;
        oci->postproc_state.last_base_qindex = oci->base_qindex;
        oci->postproc_state.last_frame_valid = 1;
        return 0;
    }

    /* Allocate post_proc_buffer_int if needed */
    if ((flags & VP8D_MFQE) && !oci->post_proc_buffer_int_used)
    {
        if ((flags & VP8D_DEBLOCK) || (flags & VP8D_DEMACROBLOCK))
        {
            int width = (oci->Width + 15) & ~15;
            int height = (oci->Height + 15) & ~15;

            if (vp8_yv12_alloc_frame_buffer(&oci->post_proc_buffer_int,
                                            width, height, VP8BORDERINPIXELS))
                vpx_internal_error(&oci->error, VPX_CODEC_MEM_ERROR,
                                   "Failed to allocate MFQE framebuffer");

            oci->post_proc_buffer_int_used = 1;

            /* insure that postproc is set to all 0's so that post proc
             * doesn't pull random data in from edge
             */
            vpx_memset((&oci->post_proc_buffer_int)->buffer_alloc,128,(&oci->post_proc_buffer)->frame_size);

        }
    }

    vp8_clear_system_state();

    if ((flags & VP8D_MFQE) &&
         oci->postproc_state.last_frame_valid &&
         oci->current_video_frame >= 2 &&
         oci->postproc_state.last_base_qindex < 60 &&
         oci->base_qindex - oci->postproc_state.last_base_qindex >= 20)
    {
        vp8_multiframe_quality_enhance(oci);
        if (((flags & VP8D_DEBLOCK) || (flags & VP8D_DEMACROBLOCK)) &&
            oci->post_proc_buffer_int_used)
        {
            vp8_yv12_copy_frame(&oci->post_proc_buffer, &oci->post_proc_buffer_int);
            if (flags & VP8D_DEMACROBLOCK)
            {
                vp8_deblock(oci, &oci->post_proc_buffer_int, &oci->post_proc_buffer,
                                               q + (deblock_level - 5) * 10, 1, 0);
                vp8_de_mblock(&oci->post_proc_buffer,
                              q + (deblock_level - 5) * 10);
            }
            else if (flags & VP8D_DEBLOCK)
            {
                vp8_deblock(oci, &oci->post_proc_buffer_int, &oci->post_proc_buffer,
                            q, 1, 0);
            }
        }
        /* Move partially towards the base q of the previous frame */
        oci->postproc_state.last_base_qindex = (3*oci->postproc_state.last_base_qindex + oci->base_qindex)>>2;
    }
    else if (flags & VP8D_DEMACROBLOCK)
    {
        vp8_deblock(oci, oci->frame_to_show, &oci->post_proc_buffer,
                                     q + (deblock_level - 5) * 10, 1, 0);
        vp8_de_mblock(&oci->post_proc_buffer, q + (deblock_level - 5) * 10);

        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }
    else if (flags & VP8D_DEBLOCK)
    {
        vp8_deblock(oci, oci->frame_to_show, &oci->post_proc_buffer,
                    q, 1, 0);
        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }
    else
    {
        vp8_yv12_copy_frame(oci->frame_to_show, &oci->post_proc_buffer);
        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }
    oci->postproc_state.last_frame_valid = 1;

    if (flags & VP8D_ADDNOISE)
    {
        if (oci->postproc_state.last_q != q
            || oci->postproc_state.last_noise != noise_level)
        {
            fillrd(&oci->postproc_state, 63 - q, noise_level);
        }

        vp8_plane_add_noise
        (oci->post_proc_buffer.y_buffer,
         oci->postproc_state.noise,
         oci->postproc_state.blackclamp,
         oci->postproc_state.whiteclamp,
         oci->postproc_state.bothclamp,
         oci->post_proc_buffer.y_width, oci->post_proc_buffer.y_height,
         oci->post_proc_buffer.y_stride);
    }

#if CONFIG_POSTPROC_VISUALIZER
    if (flags & VP8D_DEBUG_TXT_FRAME_INFO)
    {
        char message[512];
        sprintf(message, "F%1dG%1dQ%3dF%3dP%d_s%dx%d",
                (oci->frame_type == KEY_FRAME),
                oci->refresh_golden_frame,
                oci->base_qindex,
                oci->filter_level,
                flags,
                oci->mb_cols, oci->mb_rows);
        vp8_blit_text(message, oci->post_proc_buffer.y_buffer, oci->post_proc_buffer.y_stride);
    }

    if (flags & VP8D_DEBUG_TXT_MBLK_MODES)
    {
        int i, j;
        unsigned char *y_ptr;
        YV12_BUFFER_CONFIG *post = &oci->post_proc_buffer;
        int mb_rows = post->y_height >> 4;
        int mb_cols = post->y_width  >> 4;
        int mb_index = 0;
        MODE_INFO *mi = oci->mi;

        y_ptr = post->y_buffer + 4 * post->y_stride + 4;

        /* vp8_filter each macro block */
        for (i = 0; i < mb_rows; i++)
        {
            for (j = 0; j < mb_cols; j++)
            {
                char zz[4];

                sprintf(zz, "%c", mi[mb_index].mbmi.mode + 'a');

                vp8_blit_text(zz, y_ptr, post->y_stride);
                mb_index ++;
                y_ptr += 16;
            }

            mb_index ++; /* border */
            y_ptr += post->y_stride  * 16 - post->y_width;

        }
    }

    if (flags & VP8D_DEBUG_TXT_DC_DIFF)
    {
        int i, j;
        unsigned char *y_ptr;
        YV12_BUFFER_CONFIG *post = &oci->post_proc_buffer;
        int mb_rows = post->y_height >> 4;
        int mb_cols = post->y_width  >> 4;
        int mb_index = 0;
        MODE_INFO *mi = oci->mi;

        y_ptr = post->y_buffer + 4 * post->y_stride + 4;

        /* vp8_filter each macro block */
        for (i = 0; i < mb_rows; i++)
        {
            for (j = 0; j < mb_cols; j++)
            {
                char zz[4];
                int dc_diff = !(mi[mb_index].mbmi.mode != B_PRED &&
                              mi[mb_index].mbmi.mode != SPLITMV &&
                              mi[mb_index].mbmi.mb_skip_coeff);

                if (oci->frame_type == KEY_FRAME)
                    sprintf(zz, "a");
                else
                    sprintf(zz, "%c", dc_diff + '0');

                vp8_blit_text(zz, y_ptr, post->y_stride);
                mb_index ++;
                y_ptr += 16;
            }

            mb_index ++; /* border */
            y_ptr += post->y_stride  * 16 - post->y_width;

        }
    }

    if (flags & VP8D_DEBUG_TXT_RATE_INFO)
    {
        char message[512];
        sprintf(message, "Bitrate: %10.2f framerate: %10.2f ", oci->bitrate, oci->framerate);
        vp8_blit_text(message, oci->post_proc_buffer.y_buffer, oci->post_proc_buffer.y_stride);
    }

    /* Draw motion vectors */
    if ((flags & VP8D_DEBUG_DRAW_MV) && ppflags->display_mv_flag)
    {
        YV12_BUFFER_CONFIG *post = &oci->post_proc_buffer;
        int width  = post->y_width;
        int height = post->y_height;
        unsigned char *y_buffer = oci->post_proc_buffer.y_buffer;
        int y_stride = oci->post_proc_buffer.y_stride;
        MODE_INFO *mi = oci->mi;
        int x0, y0;

        for (y0 = 0; y0 < height; y0 += 16)
        {
            for (x0 = 0; x0 < width; x0 += 16)
            {
                int x1, y1;

                if (!(ppflags->display_mv_flag & (1<<mi->mbmi.mode)))
                {
                    mi++;
                    continue;
                }

                if (mi->mbmi.mode == SPLITMV)
                {
                    switch (mi->mbmi.partitioning)
                    {
                        case 0 :    /* mv_top_bottom */
                        {
                            union b_mode_info *bmi = &mi->bmi[0];
                            MV *mv = &bmi->mv.as_mv;

                            x1 = x0 + 8 + (mv->col >> 3);
                            y1 = y0 + 4 + (mv->row >> 3);

                            constrain_line (x0+8, &x1, y0+4, &y1, width, height);
                            vp8_blit_line  (x0+8,  x1, y0+4,  y1, y_buffer, y_stride);

                            bmi = &mi->bmi[8];

                            x1 = x0 + 8 + (mv->col >> 3);
                            y1 = y0 +12 + (mv->row >> 3);

                            constrain_line (x0+8, &x1, y0+12, &y1, width, height);
                            vp8_blit_line  (x0+8,  x1, y0+12,  y1, y_buffer, y_stride);

                            break;
                        }
                        case 1 :    /* mv_left_right */
                        {
                            union b_mode_info *bmi = &mi->bmi[0];
                            MV *mv = &bmi->mv.as_mv;

                            x1 = x0 + 4 + (mv->col >> 3);
                            y1 = y0 + 8 + (mv->row >> 3);

                            constrain_line (x0+4, &x1, y0+8, &y1, width, height);
                            vp8_blit_line  (x0+4,  x1, y0+8,  y1, y_buffer, y_stride);

                            bmi = &mi->bmi[2];

                            x1 = x0 +12 + (mv->col >> 3);
                            y1 = y0 + 8 + (mv->row >> 3);

                            constrain_line (x0+12, &x1, y0+8, &y1, width, height);
                            vp8_blit_line  (x0+12,  x1, y0+8,  y1, y_buffer, y_stride);

                            break;
                        }
                        case 2 :    /* mv_quarters   */
                        {
                            union b_mode_info *bmi = &mi->bmi[0];
                            MV *mv = &bmi->mv.as_mv;

                            x1 = x0 + 4 + (mv->col >> 3);
                            y1 = y0 + 4 + (mv->row >> 3);

                            constrain_line (x0+4, &x1, y0+4, &y1, width, height);
                            vp8_blit_line  (x0+4,  x1, y0+4,  y1, y_buffer, y_stride);

                            bmi = &mi->bmi[2];

                            x1 = x0 +12 + (mv->col >> 3);
                            y1 = y0 + 4 + (mv->row >> 3);

                            constrain_line (x0+12, &x1, y0+4, &y1, width, height);
                            vp8_blit_line  (x0+12,  x1, y0+4,  y1, y_buffer, y_stride);

                            bmi = &mi->bmi[8];

                            x1 = x0 + 4 + (mv->col >> 3);
                            y1 = y0 +12 + (mv->row >> 3);

                            constrain_line (x0+4, &x1, y0+12, &y1, width, height);
                            vp8_blit_line  (x0+4,  x1, y0+12,  y1, y_buffer, y_stride);

                            bmi = &mi->bmi[10];

                            x1 = x0 +12 + (mv->col >> 3);
                            y1 = y0 +12 + (mv->row >> 3);

                            constrain_line (x0+12, &x1, y0+12, &y1, width, height);
                            vp8_blit_line  (x0+12,  x1, y0+12,  y1, y_buffer, y_stride);
                            break;
                        }
                        default :
                        {
                            union b_mode_info *bmi = mi->bmi;
                            int bx0, by0;

                            for (by0 = y0; by0 < (y0+16); by0 += 4)
                            {
                                for (bx0 = x0; bx0 < (x0+16); bx0 += 4)
                                {
                                    MV *mv = &bmi->mv.as_mv;

                                    x1 = bx0 + 2 + (mv->col >> 3);
                                    y1 = by0 + 2 + (mv->row >> 3);

                                    constrain_line (bx0+2, &x1, by0+2, &y1, width, height);
                                    vp8_blit_line  (bx0+2,  x1, by0+2,  y1, y_buffer, y_stride);

                                    bmi++;
                                }
                            }
                        }
                    }
                }
                else if (mi->mbmi.mode >= NEARESTMV)
                {
                    MV *mv = &mi->mbmi.mv.as_mv;
                    const int lx0 = x0 + 8;
                    const int ly0 = y0 + 8;

                    x1 = lx0 + (mv->col >> 3);
                    y1 = ly0 + (mv->row >> 3);

                    if (x1 != lx0 && y1 != ly0)
                    {
                        constrain_line (lx0, &x1, ly0-1, &y1, width, height);
                        vp8_blit_line  (lx0,  x1, ly0-1,  y1, y_buffer, y_stride);

                        constrain_line (lx0, &x1, ly0+1, &y1, width, height);
                        vp8_blit_line  (lx0,  x1, ly0+1,  y1, y_buffer, y_stride);
                    }
                    else
                        vp8_blit_line  (lx0,  x1, ly0,  y1, y_buffer, y_stride);
                }

                mi++;
            }
            mi++;
        }
    }

    /* Color in block modes */
    if ((flags & VP8D_DEBUG_CLR_BLK_MODES)
        && (ppflags->display_mb_modes_flag || ppflags->display_b_modes_flag))
    {
        int y, x;
        YV12_BUFFER_CONFIG *post = &oci->post_proc_buffer;
        int width  = post->y_width;
        int height = post->y_height;
        unsigned char *y_ptr = oci->post_proc_buffer.y_buffer;
        unsigned char *u_ptr = oci->post_proc_buffer.u_buffer;
        unsigned char *v_ptr = oci->post_proc_buffer.v_buffer;
        int y_stride = oci->post_proc_buffer.y_stride;
        MODE_INFO *mi = oci->mi;

        for (y = 0; y < height; y += 16)
        {
            for (x = 0; x < width; x += 16)
            {
                int Y = 0, U = 0, V = 0;

                if (mi->mbmi.mode == B_PRED &&
                    ((ppflags->display_mb_modes_flag & B_PRED) || ppflags->display_b_modes_flag))
                {
                    int by, bx;
                    unsigned char *yl, *ul, *vl;
                    union b_mode_info *bmi = mi->bmi;

                    yl = y_ptr + x;
                    ul = u_ptr + (x>>1);
                    vl = v_ptr + (x>>1);

                    for (by = 0; by < 16; by += 4)
                    {
                        for (bx = 0; bx < 16; bx += 4)
                        {
                            if ((ppflags->display_b_modes_flag & (1<<mi->mbmi.mode))
                                || (ppflags->display_mb_modes_flag & B_PRED))
                            {
                                Y = B_PREDICTION_MODE_colors[bmi->as_mode][0];
                                U = B_PREDICTION_MODE_colors[bmi->as_mode][1];
                                V = B_PREDICTION_MODE_colors[bmi->as_mode][2];

                                vp8_blend_b
                                    (yl+bx, ul+(bx>>1), vl+(bx>>1), Y, U, V, 0xc000, y_stride);
                            }
                            bmi++;
                        }

                        yl += y_stride*4;
                        ul += y_stride*1;
                        vl += y_stride*1;
                    }
                }
                else if (ppflags->display_mb_modes_flag & (1<<mi->mbmi.mode))
                {
                    Y = MB_PREDICTION_MODE_colors[mi->mbmi.mode][0];
                    U = MB_PREDICTION_MODE_colors[mi->mbmi.mode][1];
                    V = MB_PREDICTION_MODE_colors[mi->mbmi.mode][2];

                    vp8_blend_mb_inner
                        (y_ptr+x, u_ptr+(x>>1), v_ptr+(x>>1), Y, U, V, 0xc000, y_stride);
                }

                mi++;
            }
            y_ptr += y_stride*16;
            u_ptr += y_stride*4;
            v_ptr += y_stride*4;

            mi++;
        }
    }

    /* Color in frame reference blocks */
    if ((flags & VP8D_DEBUG_CLR_FRM_REF_BLKS) && ppflags->display_ref_frame_flag)
    {
        int y, x;
        YV12_BUFFER_CONFIG *post = &oci->post_proc_buffer;
        int width  = post->y_width;
        int height = post->y_height;
        unsigned char *y_ptr = oci->post_proc_buffer.y_buffer;
        unsigned char *u_ptr = oci->post_proc_buffer.u_buffer;
        unsigned char *v_ptr = oci->post_proc_buffer.v_buffer;
        int y_stride = oci->post_proc_buffer.y_stride;
        MODE_INFO *mi = oci->mi;

        for (y = 0; y < height; y += 16)
        {
            for (x = 0; x < width; x +=16)
            {
                int Y = 0, U = 0, V = 0;

                if (ppflags->display_ref_frame_flag & (1<<mi->mbmi.ref_frame))
                {
                    Y = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][0];
                    U = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][1];
                    V = MV_REFERENCE_FRAME_colors[mi->mbmi.ref_frame][2];

                    vp8_blend_mb_outer
                        (y_ptr+x, u_ptr+(x>>1), v_ptr+(x>>1), Y, U, V, 0xc000, y_stride);
                }

                mi++;
            }
            y_ptr += y_stride*16;
            u_ptr += y_stride*4;
            v_ptr += y_stride*4;

            mi++;
        }
    }
#endif

    *dest = oci->post_proc_buffer;

    /* handle problem with extending borders */
    dest->y_width = oci->Width;
    dest->y_height = oci->Height;
    dest->uv_height = dest->y_height / 2;
    return 0;
}
#endif

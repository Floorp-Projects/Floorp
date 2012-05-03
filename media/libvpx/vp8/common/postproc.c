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
#include "vpx_scale/yv12config.h"
#include "postproc.h"
#include "common.h"
#include "recon.h"
#include "vpx_scale/yv12extend.h"
#include "vpx_scale/vpxscale.h"
#include "systemdependent.h"
#include "variance.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define RGB_TO_YUV(t)                                                                       \
    ( (0.257*(float)(t>>16)) + (0.504*(float)(t>>8&0xff)) + (0.098*(float)(t&0xff)) + 16),  \
    (-(0.148*(float)(t>>16)) - (0.291*(float)(t>>8&0xff)) + (0.439*(float)(t&0xff)) + 128), \
    ( (0.439*(float)(t>>16)) - (0.368*(float)(t>>8&0xff)) - (0.071*(float)(t&0xff)) + 128)

/* global constants */
#define MFQE_PRECISION 4
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

static const short kernel5[] =
{
    1, 1, 4, 1, 1
};

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
void vp8_post_proc_down_and_across_c
(
    unsigned char *src_ptr,
    unsigned char *dst_ptr,
    int src_pixels_per_line,
    int dst_pixels_per_line,
    int rows,
    int cols,
    int flimit
)
{
    unsigned char *p_src, *p_dst;
    int row;
    int col;
    int i;
    int v;
    int pitch = src_pixels_per_line;
    unsigned char d[8];
    (void)dst_pixels_per_line;

    for (row = 0; row < rows; row++)
    {
        /* post_proc_down for one row */
        p_src = src_ptr;
        p_dst = dst_ptr;

        for (col = 0; col < cols; col++)
        {

            int kernel = 4;
            int v = p_src[col];

            for (i = -2; i <= 2; i++)
            {
                if (abs(v - p_src[col+i*pitch]) > flimit)
                    goto down_skip_convolve;

                kernel += kernel5[2+i] * p_src[col+i*pitch];
            }

            v = (kernel >> 3);
        down_skip_convolve:
            p_dst[col] = v;
        }

        /* now post_proc_across */
        p_src = dst_ptr;
        p_dst = dst_ptr;

        for (i = -8; i<0; i++)
          p_src[i]=p_src[0];

        for (i = cols; i<cols+8; i++)
          p_src[i]=p_src[cols-1];

        for (i = 0; i < 8; i++)
            d[i] = p_src[i];

        for (col = 0; col < cols; col++)
        {
            int kernel = 4;
            v = p_src[col];

            d[col&7] = v;

            for (i = -2; i <= 2; i++)
            {
                if (abs(v - p_src[col+i]) > flimit)
                    goto across_skip_convolve;

                kernel += kernel5[2+i] * p_src[col+i];
            }

            d[col&7] = (kernel >> 3);
        across_skip_convolve:

            if (col >= 2)
                p_dst[col-2] = d[(col-2)&7];
        }

        /* handle the last two pixels */
        p_dst[col-2] = d[(col-2)&7];
        p_dst[col-1] = d[(col-1)&7];


        /* next row */
        src_ptr += pitch;
        dst_ptr += pitch;
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

        for (i = -8; i<0; i++)
          s[i]=s[0];

        // 17 avoids valgrind warning - we buffer values in c in d
        // and only write them when we've read 8 ahead...
        for (i = cols; i<cols+17; i++)
          s[i]=s[cols-1];

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

        // 17 avoids valgrind warning - we buffer values in c in d
        // and only write them when we've read 8 ahead...
        for (i = rows; i < rows+17; i++)
          s[i*pitch]=s[(rows-1)*pitch];

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

            s[-8*pitch] = d[(r-8)&15];
            s += pitch;
        }
    }
}


static void vp8_deblock_and_de_macro_block(YV12_BUFFER_CONFIG         *source,
        YV12_BUFFER_CONFIG         *post,
        int                         q,
        int                         low_var_thresh,
        int                         flag,
        vp8_postproc_rtcd_vtable_t *rtcd)
{
    double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
    int ppl = (int)(level + .5);
    (void) low_var_thresh;
    (void) flag;

    POSTPROC_INVOKE(rtcd, downacross)(source->y_buffer, post->y_buffer, source->y_stride,  post->y_stride, source->y_height, source->y_width,  ppl);
    POSTPROC_INVOKE(rtcd, across)(post->y_buffer, post->y_stride, post->y_height, post->y_width, q2mbl(q));
    POSTPROC_INVOKE(rtcd, down)(post->y_buffer, post->y_stride, post->y_height, post->y_width, q2mbl(q));


    POSTPROC_INVOKE(rtcd, downacross)(source->u_buffer, post->u_buffer, source->uv_stride, post->uv_stride, source->uv_height, source->uv_width, ppl);
    POSTPROC_INVOKE(rtcd, downacross)(source->v_buffer, post->v_buffer, source->uv_stride, post->uv_stride, source->uv_height, source->uv_width, ppl);

}

void vp8_deblock(YV12_BUFFER_CONFIG         *source,
                 YV12_BUFFER_CONFIG         *post,
                 int                         q,
                 int                         low_var_thresh,
                 int                         flag,
                 vp8_postproc_rtcd_vtable_t *rtcd)
{
    double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
    int ppl = (int)(level + .5);
    (void) low_var_thresh;
    (void) flag;

    POSTPROC_INVOKE(rtcd, downacross)(source->y_buffer, post->y_buffer, source->y_stride,  post->y_stride, source->y_height, source->y_width,   ppl);
    POSTPROC_INVOKE(rtcd, downacross)(source->u_buffer, post->u_buffer, source->uv_stride, post->uv_stride,  source->uv_height, source->uv_width, ppl);
    POSTPROC_INVOKE(rtcd, downacross)(source->v_buffer, post->v_buffer, source->uv_stride, post->uv_stride, source->uv_height, source->uv_width, ppl);
}

void vp8_de_noise(YV12_BUFFER_CONFIG         *source,
                  YV12_BUFFER_CONFIG         *post,
                  int                         q,
                  int                         low_var_thresh,
                  int                         flag,
                  vp8_postproc_rtcd_vtable_t *rtcd)
{
    double level = 6.0e-05 * q * q * q - .0067 * q * q + .306 * q + .0065;
    int ppl = (int)(level + .5);
    (void) post;
    (void) low_var_thresh;
    (void) flag;

    POSTPROC_INVOKE(rtcd, downacross)(
        source->y_buffer + 2 * source->y_stride + 2,
        source->y_buffer + 2 * source->y_stride + 2,
        source->y_stride,
        source->y_stride,
        source->y_height - 4,
        source->y_width - 4,
        ppl);
    POSTPROC_INVOKE(rtcd, downacross)(
        source->u_buffer + 2 * source->uv_stride + 2,
        source->u_buffer + 2 * source->uv_stride + 2,
        source->uv_stride,
        source->uv_stride,
        source->uv_height - 4,
        source->uv_width - 4, ppl);
    POSTPROC_INVOKE(rtcd, downacross)(
        source->v_buffer + 2 * source->uv_stride + 2,
        source->v_buffer + 2 * source->uv_stride + 2,
        source->uv_stride,
        source->uv_stride,
        source->uv_height - 4,
        source->uv_width - 4, ppl);

}

double vp8_gaussian(double sigma, double mu, double x)
{
    return 1 / (sigma * sqrt(2.0 * 3.14159265)) *
           (exp(-(x - mu) * (x - mu) / (2 * sigma * sigma)));
}

extern void (*vp8_clear_system_state)(void);


static void fillrd(struct postproc_state *state, int q, int a)
{
    char char_dist[300];

    double sigma;
    int ai = a, qi = q, i;

    vp8_clear_system_state();


    sigma = ai + .5 + .6 * (63 - qi) / 63.0;

    /* set up a lookup table of 256 entries that matches
     * a gaussian distribution with sigma determined by q.
     */
    {
        double i;
        int next, j;

        next = 0;

        for (i = -32; i < 32; i++)
        {
            int a = (int)(.5 + 256 * vp8_gaussian(sigma, 0, i));

            if (a)
            {
                for (j = 0; j < a; j++)
                {
                    char_dist[next+j] = (char) i;
                }

                next = next + j;
            }

        }

        for (next = next; next < 256; next++)
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
                        int y1, int u1, int v1, int alpha, int stride)
{
    int i, j;
    int y1_const = y1*((1<<16)-alpha);
    int u1_const = u1*((1<<16)-alpha);
    int v1_const = v1*((1<<16)-alpha);

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
                        int y1, int u1, int v1, int alpha, int stride)
{
    int i, j;
    int y1_const = y1*((1<<16)-alpha);
    int u1_const = u1*((1<<16)-alpha);
    int v1_const = v1*((1<<16)-alpha);

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
                        int y1, int u1, int v1, int alpha, int stride)
{
    int i, j;
    int y1_const = y1*((1<<16)-alpha);
    int u1_const = u1*((1<<16)-alpha);
    int v1_const = v1*((1<<16)-alpha);

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

static void constrain_line (int x0, int *x1, int y0, int *y1, int width, int height)
{
    int dx;
    int dy;

    if (*x1 > width)
    {
        dx = *x1 - x0;
        dy = *y1 - y0;

        *x1 = width;
        if (dx)
            *y1 = ((width-x0)*dy)/dx + y0;
    }
    if (*x1 < 0)
    {
        dx = *x1 - x0;
        dy = *y1 - y0;

        *x1 = 0;
        if (dx)
            *y1 = ((0-x0)*dy)/dx + y0;
    }
    if (*y1 > height)
    {
        dx = *x1 - x0;
        dy = *y1 - y0;

        *y1 = height;
        if (dy)
            *x1 = ((height-y0)*dx)/dy + x0;
    }
    if (*y1 < 0)
    {
        dx = *x1 - x0;
        dy = *y1 - y0;

        *y1 = 0;
        if (dy)
            *x1 = ((0-y0)*dx)/dy + x0;
    }
}


static void multiframe_quality_enhance_block
(
    int blksize, /* Currently only values supported are 16, 8, 4 */
    int qcurr,
    int qprev,
    unsigned char *y,
    unsigned char *u,
    unsigned char *v,
    int y_stride,
    int uv_stride,
    unsigned char *yd,
    unsigned char *ud,
    unsigned char *vd,
    int yd_stride,
    int uvd_stride,
    vp8_variance_rtcd_vtable_t *rtcd
)
{
    static const unsigned char VP8_ZEROS[16]=
    {
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };
    int blksizeby2 = blksize >> 1;
    int qdiff = qcurr - qprev;

    int i, j;
    unsigned char *yp;
    unsigned char *ydp;
    unsigned char *up;
    unsigned char *udp;
    unsigned char *vp;
    unsigned char *vdp;

    unsigned int act, sse, sad, thr;
    if (blksize == 16)
    {
        act = (VARIANCE_INVOKE(rtcd, var16x16)(yd, yd_stride, VP8_ZEROS, 0, &sse)+128)>>8;
        sad = (VARIANCE_INVOKE(rtcd, sad16x16)(y, y_stride, yd, yd_stride, 0)+128)>>8;
    }
    else if (blksize == 8)
    {
        act = (VARIANCE_INVOKE(rtcd, var8x8)(yd, yd_stride, VP8_ZEROS, 0, &sse)+32)>>6;
        sad = (VARIANCE_INVOKE(rtcd, sad8x8)(y, y_stride, yd, yd_stride, 0)+32)>>6;
    }
    else
    {
        act = (VARIANCE_INVOKE(rtcd, var4x4)(yd, yd_stride, VP8_ZEROS, 0, &sse)+8)>>4;
        sad = (VARIANCE_INVOKE(rtcd, sad4x4)(y, y_stride, yd, yd_stride, 0)+8)>>4;
    }
    /* thr = qdiff/8 + log2(act) + log4(qprev) */
    thr = (qdiff>>3);
    while (act>>=1) thr++;
    while (qprev>>=2) thr++;
    if (sad < thr)
    {
        static const int roundoff = (1 << (MFQE_PRECISION - 1));
        int ifactor = (sad << MFQE_PRECISION) / thr;
        ifactor >>= (qdiff >> 5);
        // TODO: SIMD optimize this section
        if (ifactor)
        {
            int icfactor = (1 << MFQE_PRECISION) - ifactor;
            for (yp = y, ydp = yd, i = 0; i < blksize; ++i, yp += y_stride, ydp += yd_stride)
            {
                for (j = 0; j < blksize; ++j)
                    ydp[j] = (int)((yp[j] * ifactor + ydp[j] * icfactor + roundoff) >> MFQE_PRECISION);
            }
            for (up = u, udp = ud, i = 0; i < blksizeby2; ++i, up += uv_stride, udp += uvd_stride)
            {
                for (j = 0; j < blksizeby2; ++j)
                    udp[j] = (int)((up[j] * ifactor + udp[j] * icfactor + roundoff) >> MFQE_PRECISION);
            }
            for (vp = v, vdp = vd, i = 0; i < blksizeby2; ++i, vp += uv_stride, vdp += uvd_stride)
            {
                for (j = 0; j < blksizeby2; ++j)
                    vdp[j] = (int)((vp[j] * ifactor + vdp[j] * icfactor + roundoff) >> MFQE_PRECISION);
            }
        }
    }
    else
    {
        if (blksize == 16)
        {
            vp8_recon_copy16x16(y, y_stride, yd, yd_stride);
            vp8_recon_copy8x8(u, uv_stride, ud, uvd_stride);
            vp8_recon_copy8x8(v, uv_stride, vd, uvd_stride);
        }
        else if (blksize == 8)
        {
            vp8_recon_copy8x8(y, y_stride, yd, yd_stride);
            for (up = u, udp = ud, i = 0; i < blksizeby2; ++i, up += uv_stride, udp += uvd_stride)
                vpx_memcpy(udp, up, blksizeby2);
            for (vp = v, vdp = vd, i = 0; i < blksizeby2; ++i, vp += uv_stride, vdp += uvd_stride)
                vpx_memcpy(vdp, vp, blksizeby2);
        }
        else
        {
            for (yp = y, ydp = yd, i = 0; i < blksize; ++i, yp += y_stride, ydp += yd_stride)
                vpx_memcpy(ydp, yp, blksize);
            for (up = u, udp = ud, i = 0; i < blksizeby2; ++i, up += uv_stride, udp += uvd_stride)
                vpx_memcpy(udp, up, blksizeby2);
            for (vp = v, vdp = vd, i = 0; i < blksizeby2; ++i, vp += uv_stride, vdp += uvd_stride)
                vpx_memcpy(vdp, vp, blksizeby2);
        }
    }
}

#if CONFIG_RUNTIME_CPU_DETECT
#define RTCD_VTABLE(oci) (&(oci)->rtcd.postproc)
#define RTCD_VARIANCE(oci) (&(oci)->rtcd.variance)
#else
#define RTCD_VTABLE(oci) NULL
#define RTCD_VARIANCE(oci) NULL
#endif

void vp8_multiframe_quality_enhance
(
    VP8_COMMON *cm
)
{
    YV12_BUFFER_CONFIG *show = cm->frame_to_show;
    YV12_BUFFER_CONFIG *dest = &cm->post_proc_buffer;

    FRAME_TYPE frame_type = cm->frame_type;
    /* Point at base of Mb MODE_INFO list has motion vectors etc */
    const MODE_INFO *mode_info_context = cm->mi;
    int mb_row;
    int mb_col;
    int qcurr = cm->base_qindex;
    int qprev = cm->postproc_state.last_base_qindex;

    unsigned char *y_ptr, *u_ptr, *v_ptr;
    unsigned char *yd_ptr, *ud_ptr, *vd_ptr;

    /* Set up the buffer pointers */
    y_ptr = show->y_buffer;
    u_ptr = show->u_buffer;
    v_ptr = show->v_buffer;
    yd_ptr = dest->y_buffer;
    ud_ptr = dest->u_buffer;
    vd_ptr = dest->v_buffer;

    /* postprocess each macro block */
    for (mb_row = 0; mb_row < cm->mb_rows; mb_row++)
    {
        for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
        {
            /* if motion is high there will likely be no benefit */
            if (((frame_type == INTER_FRAME &&
                  abs(mode_info_context->mbmi.mv.as_mv.row) <= 10 &&
                  abs(mode_info_context->mbmi.mv.as_mv.col) <= 10) ||
                 (frame_type == KEY_FRAME)))
            {
                if (mode_info_context->mbmi.mode == B_PRED || mode_info_context->mbmi.mode == SPLITMV)
                {
                    int i, j;
                    for (i=0; i<2; ++i)
                        for (j=0; j<2; ++j)
                            multiframe_quality_enhance_block(8,
                                                             qcurr,
                                                             qprev,
                                                             y_ptr + 8*(i*show->y_stride+j),
                                                             u_ptr + 4*(i*show->uv_stride+j),
                                                             v_ptr + 4*(i*show->uv_stride+j),
                                                             show->y_stride,
                                                             show->uv_stride,
                                                             yd_ptr + 8*(i*dest->y_stride+j),
                                                             ud_ptr + 4*(i*dest->uv_stride+j),
                                                             vd_ptr + 4*(i*dest->uv_stride+j),
                                                             dest->y_stride,
                                                             dest->uv_stride,
                                                             RTCD_VARIANCE(cm));
                }
                else
                {
                    multiframe_quality_enhance_block(16,
                                                     qcurr,
                                                     qprev,
                                                     y_ptr,
                                                     u_ptr,
                                                     v_ptr,
                                                     show->y_stride,
                                                     show->uv_stride,
                                                     yd_ptr,
                                                     ud_ptr,
                                                     vd_ptr,
                                                     dest->y_stride,
                                                     dest->uv_stride,
                                                     RTCD_VARIANCE(cm));

                }
            }
            else
            {
                vp8_recon_copy16x16(y_ptr, show->y_stride, yd_ptr, dest->y_stride);
                vp8_recon_copy8x8(u_ptr, show->uv_stride, ud_ptr, dest->uv_stride);
                vp8_recon_copy8x8(v_ptr, show->uv_stride, vd_ptr, dest->uv_stride);
            }
            y_ptr += 16;
            u_ptr += 8;
            v_ptr += 8;
            yd_ptr += 16;
            ud_ptr += 8;
            vd_ptr += 8;
            mode_info_context++;     /* step to next MB */
        }

        y_ptr += show->y_stride  * 16 - 16 * cm->mb_cols;
        u_ptr += show->uv_stride *  8 - 8 * cm->mb_cols;
        v_ptr += show->uv_stride *  8 - 8 * cm->mb_cols;
        yd_ptr += dest->y_stride  * 16 - 16 * cm->mb_cols;
        ud_ptr += dest->uv_stride *  8 - 8 * cm->mb_cols;
        vd_ptr += dest->uv_stride *  8 - 8 * cm->mb_cols;

        mode_info_context++;         /* Skip border mb */
    }
}

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

            // insure that postproc is set to all 0's so that post proc
            // doesn't pull random data in from edge
            vpx_memset((&oci->post_proc_buffer_int)->buffer_alloc,126,(&oci->post_proc_buffer)->frame_size);

        }
    }

#if ARCH_X86||ARCH_X86_64
    vpx_reset_mmx_state();
#endif

    if ((flags & VP8D_MFQE) &&
         oci->current_video_frame >= 2 &&
         oci->base_qindex - oci->postproc_state.last_base_qindex >= 10)
    {
        vp8_multiframe_quality_enhance(oci);
        if (((flags & VP8D_DEBLOCK) || (flags & VP8D_DEMACROBLOCK)) &&
            oci->post_proc_buffer_int_used)
        {
            vp8_yv12_copy_frame_ptr(&oci->post_proc_buffer, &oci->post_proc_buffer_int);
            if (flags & VP8D_DEMACROBLOCK)
            {
                vp8_deblock_and_de_macro_block(&oci->post_proc_buffer_int, &oci->post_proc_buffer,
                                               q + (deblock_level - 5) * 10, 1, 0, RTCD_VTABLE(oci));
            }
            else if (flags & VP8D_DEBLOCK)
            {
                vp8_deblock(&oci->post_proc_buffer_int, &oci->post_proc_buffer,
                            q, 1, 0, RTCD_VTABLE(oci));
            }
        }
        /* Move partially towards the base q of the previous frame */
        oci->postproc_state.last_base_qindex = (3*oci->postproc_state.last_base_qindex + oci->base_qindex)>>2;
    }
    else if (flags & VP8D_DEMACROBLOCK)
    {
        vp8_deblock_and_de_macro_block(oci->frame_to_show, &oci->post_proc_buffer,
                                       q + (deblock_level - 5) * 10, 1, 0, RTCD_VTABLE(oci));
        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }
    else if (flags & VP8D_DEBLOCK)
    {
        vp8_deblock(oci->frame_to_show, &oci->post_proc_buffer,
                    q, 1, 0, RTCD_VTABLE(oci));
        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }
    else
    {
        vp8_yv12_copy_frame_ptr(oci->frame_to_show, &oci->post_proc_buffer);
        oci->postproc_state.last_base_qindex = oci->base_qindex;
    }

    if (flags & VP8D_ADDNOISE)
    {
        if (oci->postproc_state.last_q != q
            || oci->postproc_state.last_noise != noise_level)
        {
            fillrd(&oci->postproc_state, 63 - q, noise_level);
        }

        POSTPROC_INVOKE(RTCD_VTABLE(oci), addnoise)
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
        sprintf(message, "Bitrate: %10.2f frame_rate: %10.2f ", oci->bitrate, oci->framerate);
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

                                POSTPROC_INVOKE(RTCD_VTABLE(oci), blend_b)
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

                    POSTPROC_INVOKE(RTCD_VTABLE(oci), blend_mb_inner)
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

                    POSTPROC_INVOKE(RTCD_VTABLE(oci), blend_mb_outer)
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

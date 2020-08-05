/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dp_renderer.h"

#ifdef HAVE_RENDERER_PLACEBO
#include <assert.h>

#include <libplacebo/renderer.h>
#include <libplacebo/utils/upload.h>

#ifdef HAVE_PLACEBO_VULKAN
# include <libplacebo/vulkan.h>
# include <SDL_vulkan.h>
#endif
#ifdef HAVE_PLACEBO_OPENGL
# include <libplacebo/opengl.h>
# include <SDL_opengl.h>
#endif


/**
 * Renderer context for libplacebo
 */
typedef struct renderer_priv_ctx
{
    // SDL window
    SDL_Window *win;
    // Placebo context
    struct pl_context *ctx;
    // Placebo renderer
    struct pl_renderer *renderer;
#ifdef HAVE_PLACEBO_VULKAN
    // Placebo Vulkan handle
    const struct pl_vulkan *vk;
    // Placebo Vulkan instance
    const struct pl_vk_inst *vk_inst;
    // Vulkan surface
    VkSurfaceKHR surf;
#endif
#ifdef HAVE_PLACEBO_OPENGL
    // Placebo OpenGL handle
    const struct pl_opengl *gl;
#endif
    // Placebo GPU
    const struct pl_gpu *gpu;
    // Placebo swapchain
    const struct pl_swapchain *swapchain;
    // Lock protecting access to the texture
    SDL_mutex *lock;
    // Image to render, and planes backing them
    struct pl_image image;
    const struct pl_tex *plane_tex[3];
} Dav1dPlayRendererPrivateContext;

static Dav1dPlayRendererPrivateContext*
    placebo_renderer_create_common(int window_flags)
{
    // Create Window
    SDL_Window *sdlwin = dp_create_sdl_window(window_flags | SDL_WINDOW_RESIZABLE);
    if (sdlwin == NULL)
        return NULL;

    // Alloc
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = malloc(sizeof(Dav1dPlayRendererPrivateContext));
    if (rd_priv_ctx == NULL) {
        return NULL;
    }

    *rd_priv_ctx = (Dav1dPlayRendererPrivateContext) {0};
    rd_priv_ctx->win = sdlwin;

    // Init libplacebo
    rd_priv_ctx->ctx = pl_context_create(PL_API_VER, &(struct pl_context_params) {
        .log_cb     = pl_log_color,
#ifndef NDEBUG
        .log_level  = PL_LOG_DEBUG,
#else
        .log_level  = PL_LOG_WARN,
#endif
    });
    if (rd_priv_ctx->ctx == NULL) {
        free(rd_priv_ctx);
        return NULL;
    }

    // Create Mutex
    rd_priv_ctx->lock = SDL_CreateMutex();
    if (rd_priv_ctx->lock == NULL) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        pl_context_destroy(&(rd_priv_ctx->ctx));
        free(rd_priv_ctx);
        return NULL;
    }

    return rd_priv_ctx;
}

#ifdef HAVE_PLACEBO_OPENGL
static void *placebo_renderer_create_gl()
{
    SDL_Window *sdlwin = NULL;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    // Common init
    Dav1dPlayRendererPrivateContext *rd_priv_ctx =
        placebo_renderer_create_common(SDL_WINDOW_OPENGL);

    if (rd_priv_ctx == NULL)
        return NULL;
    sdlwin = rd_priv_ctx->win;

    // Init OpenGL
    struct pl_opengl_params params = pl_opengl_default_params;
# ifndef NDEBUG
    params.debug = true;
# endif

    SDL_GLContext glcontext = SDL_GL_CreateContext(sdlwin);
    SDL_GL_MakeCurrent(sdlwin, glcontext);

    rd_priv_ctx->gl = pl_opengl_create(rd_priv_ctx->ctx, &params);
    if (!rd_priv_ctx->gl) {
        fprintf(stderr, "Failed creating opengl device!\n");
        exit(2);
    }

    rd_priv_ctx->swapchain = pl_opengl_create_swapchain(rd_priv_ctx->gl,
        &(struct pl_opengl_swapchain_params) {
            .swap_buffers = (void (*)(void *)) SDL_GL_SwapWindow,
            .priv = sdlwin,
        });

    if (!rd_priv_ctx->swapchain) {
        fprintf(stderr, "Failed creating opengl swapchain!\n");
        exit(2);
    }

    int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
    SDL_GL_GetDrawableSize(sdlwin, &w, &h);

    if (!pl_swapchain_resize(rd_priv_ctx->swapchain, &w, &h)) {
        fprintf(stderr, "Failed resizing vulkan swapchain!\n");
        exit(2);
    }

    rd_priv_ctx->gpu = rd_priv_ctx->gl->gpu;

    if (w != WINDOW_WIDTH || h != WINDOW_HEIGHT)
        printf("Note: window dimensions differ (got %dx%d)\n", w, h);

    return rd_priv_ctx;
}
#endif

#ifdef HAVE_PLACEBO_VULKAN
static void *placebo_renderer_create_vk()
{
    SDL_Window *sdlwin = NULL;

    // Common init
    Dav1dPlayRendererPrivateContext *rd_priv_ctx =
        placebo_renderer_create_common(SDL_WINDOW_VULKAN);

    if (rd_priv_ctx == NULL)
        return NULL;
    sdlwin = rd_priv_ctx->win;

    // Init Vulkan
    unsigned num = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, NULL)) {
        fprintf(stderr, "Failed enumerating Vulkan extensions: %s\n", SDL_GetError());
        exit(1);
    }

    const char **extensions = malloc(num * sizeof(const char *));
    assert(extensions);

    SDL_bool ok = SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, extensions);
    if (!ok) {
        fprintf(stderr, "Failed getting Vk instance extensions\n");
        exit(1);
    }

    if (num > 0) {
        printf("Requesting %d additional Vulkan extensions:\n", num);
        for (unsigned i = 0; i < num; i++)
            printf("    %s\n", extensions[i]);
    }

    struct pl_vk_inst_params iparams = pl_vk_inst_default_params;
    iparams.extensions = extensions;
    iparams.num_extensions = num;

    rd_priv_ctx->vk_inst = pl_vk_inst_create(rd_priv_ctx->ctx, &iparams);
    if (!rd_priv_ctx->vk_inst) {
        fprintf(stderr, "Failed creating Vulkan instance!\n");
        exit(1);
    }
    free(extensions);

    if (!SDL_Vulkan_CreateSurface(sdlwin, rd_priv_ctx->vk_inst->instance, &rd_priv_ctx->surf)) {
        fprintf(stderr, "Failed creating vulkan surface: %s\n", SDL_GetError());
        exit(1);
    }

    struct pl_vulkan_params params = pl_vulkan_default_params;
    params.instance = rd_priv_ctx->vk_inst->instance;
    params.surface = rd_priv_ctx->surf;
    params.allow_software = true;

    rd_priv_ctx->vk = pl_vulkan_create(rd_priv_ctx->ctx, &params);
    if (!rd_priv_ctx->vk) {
        fprintf(stderr, "Failed creating vulkan device!\n");
        exit(2);
    }

    // Create swapchain
    rd_priv_ctx->swapchain = pl_vulkan_create_swapchain(rd_priv_ctx->vk,
        &(struct pl_vulkan_swapchain_params) {
            .surface = rd_priv_ctx->surf,
            .present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        });

    if (!rd_priv_ctx->swapchain) {
        fprintf(stderr, "Failed creating vulkan swapchain!\n");
        exit(2);
    }

    int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
    if (!pl_swapchain_resize(rd_priv_ctx->swapchain, &w, &h)) {
        fprintf(stderr, "Failed resizing vulkan swapchain!\n");
        exit(2);
    }

    rd_priv_ctx->gpu = rd_priv_ctx->vk->gpu;

    if (w != WINDOW_WIDTH || h != WINDOW_HEIGHT)
        printf("Note: window dimensions differ (got %dx%d)\n", w, h);

    return rd_priv_ctx;
}
#endif

static void placebo_renderer_destroy(void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    pl_renderer_destroy(&(rd_priv_ctx->renderer));
    pl_swapchain_destroy(&(rd_priv_ctx->swapchain));
    for (int i = 0; i < 3; i++)
        pl_tex_destroy(rd_priv_ctx->gpu, &(rd_priv_ctx->plane_tex[i]));

#ifdef HAVE_PLACEBO_VULKAN
    if (rd_priv_ctx->vk) {
        pl_vulkan_destroy(&(rd_priv_ctx->vk));
        vkDestroySurfaceKHR(rd_priv_ctx->vk_inst->instance, rd_priv_ctx->surf, NULL);
        pl_vk_inst_destroy(&(rd_priv_ctx->vk_inst));
    }
#endif
#ifdef HAVE_PLACEBO_OPENGL
    if (rd_priv_ctx->gl)
        pl_opengl_destroy(&(rd_priv_ctx->gl));
#endif

    SDL_DestroyWindow(rd_priv_ctx->win);

    pl_context_destroy(&(rd_priv_ctx->ctx));
}

static void placebo_render(void *cookie, const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);
    if (!rd_priv_ctx->image.num_planes) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    // Prepare rendering
    if (rd_priv_ctx->renderer == NULL) {
        rd_priv_ctx->renderer = pl_renderer_create(rd_priv_ctx->ctx, rd_priv_ctx->gpu);
    }

    struct pl_swapchain_frame frame;
    bool ok = pl_swapchain_start_frame(rd_priv_ctx->swapchain, &frame);
    if (!ok) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    struct pl_render_params render_params = {0};
    if (settings->highquality)
        render_params = pl_render_default_params;

    struct pl_render_target target;
    pl_render_target_from_swapchain(&target, &frame);
    target.profile = (struct pl_icc_profile) {
        .data = NULL,
        .len = 0,
    };

#if PL_API_VER >= 66
    pl_rect2df_aspect_copy(&target.dst_rect, &rd_priv_ctx->image.src_rect, 0.0);
    if (pl_render_target_partial(&target))
        pl_tex_clear(rd_priv_ctx->gpu, target.fbo, (float[4]){ 0.0 });
#endif

    if (!pl_render_image(rd_priv_ctx->renderer, &rd_priv_ctx->image, &target, &render_params)) {
        fprintf(stderr, "Failed rendering frame!\n");
        pl_tex_clear(rd_priv_ctx->gpu, target.fbo, (float[4]){ 1.0 });
    }

    ok = pl_swapchain_submit_frame(rd_priv_ctx->swapchain);
    if (!ok) {
        fprintf(stderr, "Failed submitting frame!\n");
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    pl_swapchain_swap_buffers(rd_priv_ctx->swapchain);
    SDL_UnlockMutex(rd_priv_ctx->lock);
}

static int placebo_upload_image(void *cookie, Dav1dPicture *dav1d_pic,
                                const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);

    if (dav1d_pic == NULL) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return 0;
    }

    int width = dav1d_pic->p.w;
    int height = dav1d_pic->p.h;
    int sub_x = 0, sub_y = 0;
    int bytes = (dav1d_pic->p.bpc + 7) / 8; // rounded up
    enum pl_chroma_location chroma_loc = PL_CHROMA_UNKNOWN;

    struct pl_image *image = &rd_priv_ctx->image;
    *image = (struct pl_image) {
        .num_planes = 3,
        .width      = width,
        .height     = height,
        .src_rect   = {0, 0, width, height},

        .repr = {
            .bits = {
                .sample_depth = bytes * 8,
                .color_depth = dav1d_pic->p.bpc,
            },
        },
    };

    // Figure out the correct plane dimensions/count
    switch (dav1d_pic->p.layout) {
    case DAV1D_PIXEL_LAYOUT_I400:
        image->num_planes = 1;
        break;
    case DAV1D_PIXEL_LAYOUT_I420:
        sub_x = sub_y = 1;
        break;
    case DAV1D_PIXEL_LAYOUT_I422:
        sub_x = 1;
        break;
    case DAV1D_PIXEL_LAYOUT_I444:
        break;
    }

    // Set the right colorspace metadata etc.
    switch (dav1d_pic->seq_hdr->pri) {
    case DAV1D_COLOR_PRI_UNKNOWN:   image->color.primaries = PL_COLOR_PRIM_UNKNOWN; break;
    case DAV1D_COLOR_PRI_BT709:     image->color.primaries = PL_COLOR_PRIM_BT_709; break;
    case DAV1D_COLOR_PRI_BT470M:    image->color.primaries = PL_COLOR_PRIM_BT_470M; break;
    case DAV1D_COLOR_PRI_BT470BG:   image->color.primaries = PL_COLOR_PRIM_BT_601_625; break;
    case DAV1D_COLOR_PRI_BT601:     image->color.primaries = PL_COLOR_PRIM_BT_601_625; break;
    case DAV1D_COLOR_PRI_BT2020:    image->color.primaries = PL_COLOR_PRIM_BT_2020; break;

    case DAV1D_COLOR_PRI_XYZ:
        // Handled below
        assert(dav1d_pic->seq_hdr->mtrx == DAV1D_MC_IDENTITY);
        break;

    default:
        printf("warning: unknown dav1d color primaries %d.. ignoring, picture "
               "may be very incorrect\n", dav1d_pic->seq_hdr->pri);
        break;
    }

    switch (dav1d_pic->seq_hdr->trc) {
    case DAV1D_TRC_BT709:
    case DAV1D_TRC_BT470M:
    case DAV1D_TRC_BT470BG:
    case DAV1D_TRC_BT601:
    case DAV1D_TRC_SMPTE240:
    case DAV1D_TRC_BT2020_10BIT:
    case DAV1D_TRC_BT2020_12BIT:
        // These all map to the effective "SDR" CRT-based EOTF, BT.1886
        image->color.transfer = PL_COLOR_TRC_BT_1886;
        break;

    case DAV1D_TRC_UNKNOWN:     image->color.transfer = PL_COLOR_TRC_UNKNOWN; break;
    case DAV1D_TRC_LINEAR:      image->color.transfer = PL_COLOR_TRC_LINEAR; break;
    case DAV1D_TRC_SRGB:        image->color.transfer = PL_COLOR_TRC_SRGB; break;
    case DAV1D_TRC_SMPTE2084:   image->color.transfer = PL_COLOR_TRC_PQ; break;
    case DAV1D_TRC_HLG:         image->color.transfer = PL_COLOR_TRC_HLG; break;

    default:
        printf("warning: unknown dav1d color transfer %d.. ignoring, picture "
               "may be very incorrect\n", dav1d_pic->seq_hdr->trc);
        break;
    }

    switch (dav1d_pic->seq_hdr->mtrx) {
    case DAV1D_MC_IDENTITY:
        // This is going to be either RGB or XYZ
        if (dav1d_pic->seq_hdr->pri == DAV1D_COLOR_PRI_XYZ) {
            image->repr.sys = PL_COLOR_SYSTEM_XYZ;
        } else {
            image->repr.sys = PL_COLOR_SYSTEM_RGB;
        }
        break;

    case DAV1D_MC_UNKNOWN:
        // PL_COLOR_SYSTEM_UNKNOWN maps to RGB, so hard-code this one
        image->repr.sys = pl_color_system_guess_ycbcr(width, height);
        break;

    case DAV1D_MC_BT709:        image->repr.sys = PL_COLOR_SYSTEM_BT_709; break;
    case DAV1D_MC_BT601:        image->repr.sys = PL_COLOR_SYSTEM_BT_601; break;
    case DAV1D_MC_SMPTE240:     image->repr.sys = PL_COLOR_SYSTEM_SMPTE_240M; break;
    case DAV1D_MC_SMPTE_YCGCO:  image->repr.sys = PL_COLOR_SYSTEM_YCGCO; break;
    case DAV1D_MC_BT2020_NCL:   image->repr.sys = PL_COLOR_SYSTEM_BT_2020_NC; break;
    case DAV1D_MC_BT2020_CL:    image->repr.sys = PL_COLOR_SYSTEM_BT_2020_C; break;

    case DAV1D_MC_ICTCP:
        // This one is split up based on the actual HDR curve in use
        if (dav1d_pic->seq_hdr->trc == DAV1D_TRC_HLG) {
            image->repr.sys = PL_COLOR_SYSTEM_BT_2100_HLG;
        } else {
            image->repr.sys = PL_COLOR_SYSTEM_BT_2100_PQ;
        }
        break;

    default:
        printf("warning: unknown dav1d color matrix %d.. ignoring, picture "
               "may be very incorrect\n", dav1d_pic->seq_hdr->mtrx);
        break;
    }

    if (dav1d_pic->seq_hdr->color_range) {
        image->repr.levels = PL_COLOR_LEVELS_PC;
    } else {
        image->repr.levels = PL_COLOR_LEVELS_TV;
    }

    switch (dav1d_pic->seq_hdr->chr) {
    case DAV1D_CHR_UNKNOWN:     chroma_loc = PL_CHROMA_UNKNOWN; break;
    case DAV1D_CHR_VERTICAL:    chroma_loc = PL_CHROMA_LEFT; break;
    case DAV1D_CHR_COLOCATED:   chroma_loc = PL_CHROMA_TOP_LEFT; break;
    }

#if PL_API_VER >= 63
    if (settings->gpugrain && dav1d_pic->frame_hdr->film_grain.present) {
        Dav1dFilmGrainData *src = &dav1d_pic->frame_hdr->film_grain.data;
        struct pl_av1_grain_data *dst = &image->av1_grain;
        *dst = (struct pl_av1_grain_data) {
            .grain_seed     = src->seed,
            .num_points_y   = src->num_y_points,
            .chroma_scaling_from_luma = src->chroma_scaling_from_luma,
            .num_points_uv  = { src->num_uv_points[0], src->num_uv_points[1] },
            .scaling_shift  = src->scaling_shift,
            .ar_coeff_lag   = src->ar_coeff_lag,
            .ar_coeff_shift = (int)src->ar_coeff_shift,
            .grain_scale_shift = src->grain_scale_shift,
            .uv_mult        = { src->uv_mult[0], src->uv_mult[1] },
            .uv_mult_luma   = { src->uv_luma_mult[0], src->uv_luma_mult[1] },
            .uv_offset      = { src->uv_offset[0], src->uv_offset[1] },
            .overlap        = src->overlap_flag,
        };

        assert(sizeof(dst->points_y) == sizeof(src->y_points));
        assert(sizeof(dst->points_uv) == sizeof(src->uv_points));
        assert(sizeof(dst->ar_coeffs_y) == sizeof(src->ar_coeffs_y));
        memcpy(dst->points_y, src->y_points, sizeof(src->y_points));
        memcpy(dst->points_uv, src->uv_points, sizeof(src->uv_points));
        memcpy(dst->ar_coeffs_y, src->ar_coeffs_y, sizeof(src->ar_coeffs_y));

        // this one has different row sizes for alignment
        for (int c = 0; c < 2; c++) {
            for (int i = 0; i < 25; i++)
                dst->ar_coeffs_uv[c][i] = src->ar_coeffs_uv[c][i];
        }
    }
#endif

    // Upload the actual planes
    struct pl_plane_data data[3] = {
        {
            // Y plane
            .type           = PL_FMT_UNORM,
            .width          = width,
            .height         = height,
            .pixel_stride   = bytes,
            .row_stride     = dav1d_pic->stride[0],
            .component_size = {bytes * 8},
            .component_map  = {0},
        }, {
            // U plane
            .type           = PL_FMT_UNORM,
            .width          = width >> sub_x,
            .height         = height >> sub_y,
            .pixel_stride   = bytes,
            .row_stride     = dav1d_pic->stride[1],
            .component_size = {bytes * 8},
            .component_map  = {1},
        }, {
            // V plane
            .type           = PL_FMT_UNORM,
            .width          = width >> sub_x,
            .height         = height >> sub_y,
            .pixel_stride   = bytes,
            .row_stride     = dav1d_pic->stride[1],
            .component_size = {bytes * 8},
            .component_map  = {2},
        },
    };

    bool ok = true;

    for (int i = 0; i < image->num_planes; i++) {
        if (settings->zerocopy) {
            const struct pl_buf *buf = dav1d_pic->allocator_data;
            assert(buf);
            data[i].buf = buf;
            data[i].buf_offset = (uintptr_t) dav1d_pic->data[i] - (uintptr_t) buf->data;
        } else {
            data[i].pixels = dav1d_pic->data[i];
        }

        ok &= pl_upload_plane(rd_priv_ctx->gpu, &image->planes[i], &rd_priv_ctx->plane_tex[i], &data[i]);
    }

    // Apply the correct chroma plane shift. This has to be done after pl_upload_plane
#if PL_API_VER >= 67
    pl_image_set_chroma_location(image, chroma_loc);
#else
    pl_chroma_location_offset(chroma_loc, &image->planes[1].shift_x, &image->planes[1].shift_y);
    pl_chroma_location_offset(chroma_loc, &image->planes[2].shift_x, &image->planes[2].shift_y);
#endif

    if (!ok) {
        fprintf(stderr, "Failed uploading planes!\n");
        *image = (struct pl_image) {0};
    }

    SDL_UnlockMutex(rd_priv_ctx->lock);
    return !ok;
}

// Align to power of 2
#define ALIGN2(x, align) (((x) + (align) - 1) & ~((align) - 1))

static int placebo_alloc_pic(Dav1dPicture *const p, void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);
    SDL_LockMutex(rd_priv_ctx->lock);

    const struct pl_gpu *gpu = rd_priv_ctx->gpu;
    int ret = DAV1D_ERR(ENOMEM);

    // Copied from dav1d_default_picture_alloc
    const int hbd = p->p.bpc > 8;
    const int aligned_w = ALIGN2(p->p.w, 128);
    const int aligned_h = ALIGN2(p->p.h, 128);
    const int has_chroma = p->p.layout != DAV1D_PIXEL_LAYOUT_I400;
    const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    p->stride[0] = aligned_w << hbd;
    p->stride[1] = has_chroma ? (aligned_w >> ss_hor) << hbd : 0;

    // Align strides up to multiples of the GPU performance hints
    p->stride[0] = ALIGN2(p->stride[0], gpu->limits.align_tex_xfer_stride);
    p->stride[1] = ALIGN2(p->stride[1], gpu->limits.align_tex_xfer_stride);

    // Aligning offsets to 4 also implicity aligns to the texel size (1 or 2)
    size_t off_align = ALIGN2(gpu->limits.align_tex_xfer_offset, 4);
    const size_t y_sz = ALIGN2(p->stride[0] * aligned_h, off_align);
    const size_t uv_sz = ALIGN2(p->stride[1] * (aligned_h >> ss_ver), off_align);

    // The extra DAV1D_PICTURE_ALIGNMENTs are to brute force plane alignment,
    // even in the case that the driver gives us insane alignments
    const size_t pic_size = y_sz + 2 * uv_sz;
    const size_t total_size = pic_size + DAV1D_PICTURE_ALIGNMENT * 4;

    // Validate size limitations
    if (total_size > gpu->limits.max_xfer_size) {
        printf("alloc of %zu bytes exceeds limits\n", total_size);
        goto err;
    }

    const struct pl_buf *buf = pl_buf_create(gpu, &(struct pl_buf_params) {
        .type = PL_BUF_TEX_TRANSFER,
        .host_mapped = true,
        .size = total_size,
        .memory_type = PL_BUF_MEM_HOST,
        .user_data = p,
    });

    if (!buf) {
        printf("alloc of GPU mapped buffer failed\n");
        goto err;
    }

    assert(buf->data);
    uintptr_t base = (uintptr_t) buf->data, data[3];
    data[0] = ALIGN2(base, DAV1D_PICTURE_ALIGNMENT);
    data[1] = ALIGN2(data[0] + y_sz, DAV1D_PICTURE_ALIGNMENT);
    data[2] = ALIGN2(data[1] + uv_sz, DAV1D_PICTURE_ALIGNMENT);

    // Sanity check offset alignment for the sake of debugging
    if (data[0] - base != ALIGN2(data[0] - base, off_align) ||
        data[1] - base != ALIGN2(data[1] - base, off_align) ||
        data[2] - base != ALIGN2(data[2] - base, off_align))
    {
        printf("GPU buffer horribly misaligned, expect slowdown!\n");
    }

    p->allocator_data = (void *) buf;
    p->data[0] = (void *) data[0];
    p->data[1] = (void *) data[1];
    p->data[2] = (void *) data[2];
    ret = 0;

    // fall through
err:
    SDL_UnlockMutex(rd_priv_ctx->lock);
    return ret;
}

static void placebo_release_pic(Dav1dPicture *pic, void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);
    assert(pic->allocator_data);

    SDL_LockMutex(rd_priv_ctx->lock);
    const struct pl_gpu *gpu = rd_priv_ctx->gpu;
    pl_buf_destroy(gpu, (const struct pl_buf **) &pic->allocator_data);
    SDL_UnlockMutex(rd_priv_ctx->lock);
}

#ifdef HAVE_PLACEBO_VULKAN
const Dav1dPlayRenderInfo rdr_placebo_vk = {
    .name = "placebo-vk",
    .create_renderer = placebo_renderer_create_vk,
    .destroy_renderer = placebo_renderer_destroy,
    .render = placebo_render,
    .update_frame = placebo_upload_image,
    .alloc_pic = placebo_alloc_pic,
    .release_pic = placebo_release_pic,

# if PL_API_VER >= 63
    .supports_gpu_grain = 1,
# endif
};
#else
const Dav1dPlayRenderInfo rdr_placebo_vk = { NULL };
#endif

#ifdef HAVE_PLACEBO_OPENGL
const Dav1dPlayRenderInfo rdr_placebo_gl = {
    .name = "placebo-gl",
    .create_renderer = placebo_renderer_create_gl,
    .destroy_renderer = placebo_renderer_destroy,
    .render = placebo_render,
    .update_frame = placebo_upload_image,
    .alloc_pic = placebo_alloc_pic,
    .release_pic = placebo_release_pic,

# if PL_API_VER >= 63
    .supports_gpu_grain = 1,
# endif
};
#else
const Dav1dPlayRenderInfo rdr_placebo_gl = { NULL };
#endif

#else
const Dav1dPlayRenderInfo rdr_placebo_vk = { NULL };
const Dav1dPlayRenderInfo rdr_placebo_gl = { NULL };
#endif

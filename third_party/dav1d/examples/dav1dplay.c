/*
 * Copyright © 2019, VideoLAN and dav1d authors
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

#include "config.h"
#include "vcs_version.h"

#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "common/attributes.h"

#include "dav1d/dav1d.h"

#include "tools/input/input.h"

/**
 * Settings structure
 * Hold all settings available for the player,
 * this is usually filled by parsing arguments
 * from the console.
 */
typedef struct {
    const char *inputfile;
    int highquality;
    int untimed;
    int zerocopy;
} Dav1dPlaySettings;

#define WINDOW_WIDTH  910
#define WINDOW_HEIGHT 512

#define DAV1D_EVENT_NEW_FRAME 1
#define DAV1D_EVENT_DEC_QUIT  2

/*
 * Fifo helper functions
 */
typedef struct dp_fifo
{
    SDL_mutex *lock;
    SDL_cond *cond_change;
    size_t capacity;
    size_t count;
    void **entries;
} Dav1dPlayPtrFifo;

static void dp_fifo_destroy(Dav1dPlayPtrFifo *fifo)
{
    assert(fifo->count == 0);
    SDL_DestroyMutex(fifo->lock);
    SDL_DestroyCond(fifo->cond_change);
    free(fifo->entries);
    free(fifo);
}

static Dav1dPlayPtrFifo *dp_fifo_create(size_t capacity)
{
    Dav1dPlayPtrFifo *fifo;

    assert(capacity > 0);
    if (capacity <= 0)
        return NULL;

    fifo = malloc(sizeof(*fifo));
    if (fifo == NULL)
        return NULL;

    fifo->capacity = capacity;
    fifo->count = 0;

    fifo->lock = SDL_CreateMutex();
    if (fifo->lock == NULL) {
        free(fifo);
        return NULL;
    }
    fifo->cond_change = SDL_CreateCond();
    if (fifo->cond_change == NULL) {
        SDL_DestroyMutex(fifo->lock);
        free(fifo);
        return NULL;
    }

    fifo->entries = calloc(capacity, sizeof(void*));
    if (fifo->entries == NULL) {
        dp_fifo_destroy(fifo);
        return NULL;
    }

    return fifo;
}

static void dp_fifo_push(Dav1dPlayPtrFifo *fifo, void *element)
{
    SDL_LockMutex(fifo->lock);
    while (fifo->count == fifo->capacity)
        SDL_CondWait(fifo->cond_change, fifo->lock);
    fifo->entries[fifo->count++] = element;
    if (fifo->count == 1)
        SDL_CondSignal(fifo->cond_change);
    SDL_UnlockMutex(fifo->lock);
}

static void *dp_fifo_array_shift(void **arr, size_t len)
{
    void *shifted_element = arr[0];
    for (size_t i = 1; i < len; ++i)
        arr[i-1] = arr[i];
    return shifted_element;
}

static void *dp_fifo_shift(Dav1dPlayPtrFifo *fifo)
{
    SDL_LockMutex(fifo->lock);
    while (fifo->count == 0)
        SDL_CondWait(fifo->cond_change, fifo->lock);
    void *res = dp_fifo_array_shift(fifo->entries, fifo->count--);
    if (fifo->count == fifo->capacity - 1)
        SDL_CondSignal(fifo->cond_change);
    SDL_UnlockMutex(fifo->lock);
    return res;
}

/**
 * Renderer info
 */
typedef struct rdr_info
{
    // Cookie passed to the renderer implementation callbacks
    void *cookie;
    // Callback to create the renderer
    void* (*create_renderer)(void *data);
    // Callback to destroy the renderer
    void (*destroy_renderer)(void *cookie);
    // Callback to the render function that renders a prevously sent frame
    void (*render)(void *cookie, const Dav1dPlaySettings *settings);
    // Callback to the send frame function
    int (*update_frame)(void *cookie, Dav1dPicture *dav1d_pic,
                        const Dav1dPlaySettings *settings);
    // Callback for alloc/release pictures (optional)
    int (*alloc_pic)(Dav1dPicture *pic, void *cookie);
    void (*release_pic)(Dav1dPicture *pic, void *cookie);
} Dav1dPlayRenderInfo;

#ifdef HAVE_PLACEBO_VULKAN

#include <libplacebo/renderer.h>
#include <libplacebo/utils/upload.h>
#include <libplacebo/vulkan.h>
#include <SDL_vulkan.h>


/**
 * Renderer context for libplacebo
 */
typedef struct renderer_priv_ctx
{
    // Placebo context
    struct pl_context *ctx;
    // Placebo renderer
    struct pl_renderer *renderer;
    // Placebo Vulkan handle
    const struct pl_vulkan *vk;
    // Placebo Vulkan instance
    const struct pl_vk_inst *vk_inst;
    // Vulkan surface
    VkSurfaceKHR surf;
    // Placebo swapchain
    const struct pl_swapchain *swapchain;
    // Lock protecting access to the texture
    SDL_mutex *lock;
    // Planes to render
    struct pl_plane y_plane;
    struct pl_plane u_plane;
    struct pl_plane v_plane;
    // Textures to render
    const struct pl_tex *y_tex;
    const struct pl_tex *u_tex;
    const struct pl_tex *v_tex;
} Dav1dPlayRendererPrivateContext;

static void *placebo_renderer_create(void *data)
{
    // Alloc
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = malloc(sizeof(Dav1dPlayRendererPrivateContext));
    if (rd_priv_ctx == NULL) {
        return NULL;
    }

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

    // Init Vulkan
    struct pl_vk_inst_params iparams = pl_vk_inst_default_params;

    SDL_Window *sdlwin = data;

    unsigned num = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, NULL)) {
        fprintf(stderr, "Failed enumerating Vulkan extensions: %s\n", SDL_GetError());
        exit(1);
    }

    iparams.extensions = malloc(num * sizeof(const char *));
    iparams.num_extensions = num;
    assert(iparams.extensions);

    SDL_bool ok = SDL_Vulkan_GetInstanceExtensions(sdlwin, &num, iparams.extensions);
    if (!ok) {
        fprintf(stderr, "Failed getting Vk instance extensions\n");
        exit(1);
    }

    if (num > 0) {
        printf("Requesting %d additional Vulkan extensions:\n", num);
        for (unsigned i = 0; i < num; i++)
            printf("    %s\n", iparams.extensions[i]);
    }

    rd_priv_ctx->vk_inst = pl_vk_inst_create(rd_priv_ctx->ctx, &iparams);
    if (!rd_priv_ctx->vk_inst) {
        fprintf(stderr, "Failed creating Vulkan instance!\n");
        exit(1);
    }
    free(iparams.extensions);

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

    if (w != WINDOW_WIDTH || h != WINDOW_HEIGHT)
        printf("Note: window dimensions differ (got %dx%d)\n", w, h);

    rd_priv_ctx->y_tex = NULL;
    rd_priv_ctx->u_tex = NULL;
    rd_priv_ctx->v_tex = NULL;

    rd_priv_ctx->renderer = NULL;

    return rd_priv_ctx;
}

static void placebo_renderer_destroy(void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    pl_renderer_destroy(&(rd_priv_ctx->renderer));
    pl_tex_destroy(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->y_tex));
    pl_tex_destroy(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->u_tex));
    pl_tex_destroy(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->v_tex));
    pl_swapchain_destroy(&(rd_priv_ctx->swapchain));
    pl_vulkan_destroy(&(rd_priv_ctx->vk));
    vkDestroySurfaceKHR(rd_priv_ctx->vk_inst->instance, rd_priv_ctx->surf, NULL);
    pl_vk_inst_destroy(&(rd_priv_ctx->vk_inst));
    pl_context_destroy(&(rd_priv_ctx->ctx));
}

static void placebo_render(void *cookie, const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);
    if (rd_priv_ctx->y_tex == NULL) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    // Prepare rendering
    if (rd_priv_ctx->renderer == NULL) {
        rd_priv_ctx->renderer = pl_renderer_create(rd_priv_ctx->ctx, rd_priv_ctx->vk->gpu);
    }

    struct pl_swapchain_frame frame;
    bool ok = pl_swapchain_start_frame(rd_priv_ctx->swapchain, &frame);
    if (!ok) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    const struct pl_tex *img = rd_priv_ctx->y_plane.texture;
    struct pl_image image = {
        .num_planes = 3,
        .planes     = { rd_priv_ctx->y_plane, rd_priv_ctx->u_plane, rd_priv_ctx->v_plane },
        .repr       = pl_color_repr_hdtv,
        .color      = pl_color_space_unknown,
        .width      = img->params.w,
        .height     = img->params.h,
    };

    struct pl_render_params render_params = {0};
    if (settings->highquality)
        render_params = pl_render_default_params;

    struct pl_render_target target;
    pl_render_target_from_swapchain(&target, &frame);
    target.profile = (struct pl_icc_profile) {
        .data = NULL,
        .len = 0,
    };

    if (!pl_render_image(rd_priv_ctx->renderer, &image, &target, &render_params)) {
        fprintf(stderr, "Failed rendering frame!\n");
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
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

static int placebo_upload_planes(void *cookie, Dav1dPicture *dav1d_pic,
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

    enum Dav1dPixelLayout dav1d_layout = dav1d_pic->p.layout;

    if (DAV1D_PIXEL_LAYOUT_I420 != dav1d_layout || dav1d_pic->p.bpc != 8) {
        fprintf(stderr, "Unsupported pixel format, only 8bit 420 supported so far.\n");
        exit(50);
    }

    struct pl_plane_data data_y = {
        .type           = PL_FMT_UNORM,
        .width          = width,
        .height         = height,
        .pixel_stride   = 1,
        .row_stride     = dav1d_pic->stride[0],
        .component_size = {8},
        .component_map  = {0},
    };

    struct pl_plane_data data_u = {
        .type           = PL_FMT_UNORM,
        .width          = width/2,
        .height         = height/2,
        .pixel_stride   = 1,
        .row_stride     = dav1d_pic->stride[1],
        .component_size = {8},
        .component_map  = {1},
    };

    struct pl_plane_data data_v = {
        .type           = PL_FMT_UNORM,
        .width          = width/2,
        .height         = height/2,
        .pixel_stride   = 1,
        .row_stride     = dav1d_pic->stride[1],
        .component_size = {8},
        .component_map  = {2},
    };

    if (settings->zerocopy) {
        const struct pl_buf *buf = dav1d_pic->allocator_data;
        assert(buf);
        data_y.buf = data_u.buf = data_v.buf = buf;
        data_y.buf_offset = (uintptr_t) dav1d_pic->data[0] - (uintptr_t) buf->data;
        data_u.buf_offset = (uintptr_t) dav1d_pic->data[1] - (uintptr_t) buf->data;
        data_v.buf_offset = (uintptr_t) dav1d_pic->data[2] - (uintptr_t) buf->data;
    } else {
        data_y.pixels = dav1d_pic->data[0];
        data_u.pixels = dav1d_pic->data[1];
        data_v.pixels = dav1d_pic->data[2];
    }

    bool ok = true;
    ok &= pl_upload_plane(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->y_plane), &(rd_priv_ctx->y_tex), &data_y);
    ok &= pl_upload_plane(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->u_plane), &(rd_priv_ctx->u_tex), &data_u);
    ok &= pl_upload_plane(rd_priv_ctx->vk->gpu, &(rd_priv_ctx->v_plane), &(rd_priv_ctx->v_tex), &data_v);

    pl_chroma_location_offset(PL_CHROMA_LEFT, &rd_priv_ctx->u_plane.shift_x, &rd_priv_ctx->u_plane.shift_y);
    pl_chroma_location_offset(PL_CHROMA_LEFT, &rd_priv_ctx->v_plane.shift_x, &rd_priv_ctx->v_plane.shift_y);

    if (!ok) {
        fprintf(stderr, "Failed uploading planes!\n");
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

    const struct pl_gpu *gpu = rd_priv_ctx->vk->gpu;
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
    const struct pl_gpu *gpu = rd_priv_ctx->vk->gpu;
    pl_buf_destroy(gpu, (const struct pl_buf **) &pic->allocator_data);
    SDL_UnlockMutex(rd_priv_ctx->lock);
}

static const Dav1dPlayRenderInfo renderer_info = {
    .create_renderer = placebo_renderer_create,
    .destroy_renderer = placebo_renderer_destroy,
    .render = placebo_render,
    .update_frame = placebo_upload_planes,
    .alloc_pic = placebo_alloc_pic,
    .release_pic = placebo_release_pic,
};

#else

/**
 * Renderer context for SDL
 */
typedef struct renderer_priv_ctx
{
    // SDL renderer
    SDL_Renderer *renderer;
    // Lock protecting access to the texture
    SDL_mutex *lock;
    // Texture to render
    SDL_Texture *tex;
} Dav1dPlayRendererPrivateContext;

static void *sdl_renderer_create(void *data)
{
    SDL_Window *win = data;

    // Alloc
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = malloc(sizeof(Dav1dPlayRendererPrivateContext));
    if (rd_priv_ctx == NULL) {
        return NULL;
    }

    // Create renderer
    rd_priv_ctx->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    // Set scale quality
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    // Create Mutex
    rd_priv_ctx->lock = SDL_CreateMutex();
    if (rd_priv_ctx->lock == NULL) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        free(rd_priv_ctx);
        return NULL;
    }

    rd_priv_ctx->tex = NULL;

    return rd_priv_ctx;
}

static void sdl_renderer_destroy(void *cookie)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_DestroyRenderer(rd_priv_ctx->renderer);
    SDL_DestroyMutex(rd_priv_ctx->lock);
    free(rd_priv_ctx);
}

static void sdl_render(void *cookie, const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);

    if (rd_priv_ctx->tex == NULL) {
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return;
    }

    // Display the frame
    SDL_RenderClear(rd_priv_ctx->renderer);
    SDL_RenderCopy(rd_priv_ctx->renderer, rd_priv_ctx->tex, NULL, NULL);
    SDL_RenderPresent(rd_priv_ctx->renderer);

    SDL_UnlockMutex(rd_priv_ctx->lock);
}

static int sdl_update_texture(void *cookie, Dav1dPicture *dav1d_pic,
                              const Dav1dPlaySettings *settings)
{
    Dav1dPlayRendererPrivateContext *rd_priv_ctx = cookie;
    assert(rd_priv_ctx != NULL);

    SDL_LockMutex(rd_priv_ctx->lock);

    if (dav1d_pic == NULL) {
        rd_priv_ctx->tex = NULL;
        SDL_UnlockMutex(rd_priv_ctx->lock);
        return 0;
    }

    int width = dav1d_pic->p.w;
    int height = dav1d_pic->p.h;
    int tex_w = width;
    int tex_h = height;

    enum Dav1dPixelLayout dav1d_layout = dav1d_pic->p.layout;

    if (DAV1D_PIXEL_LAYOUT_I420 != dav1d_layout || dav1d_pic->p.bpc != 8) {
        fprintf(stderr, "Unsupported pixel format, only 8bit 420 supported so far.\n");
        exit(50);
    }

    SDL_Texture *texture = rd_priv_ctx->tex;
    if (texture != NULL) {
        SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h);
        if (tex_w != width || tex_h != height) {
            SDL_DestroyTexture(texture);
            texture = NULL;
        }
    }

    if (texture == NULL) {
        texture = SDL_CreateTexture(rd_priv_ctx->renderer, SDL_PIXELFORMAT_IYUV,
            SDL_TEXTUREACCESS_STREAMING, width, height);
    }

    SDL_UpdateYUVTexture(texture, NULL,
        dav1d_pic->data[0], (int)dav1d_pic->stride[0], // Y
        dav1d_pic->data[1], (int)dav1d_pic->stride[1], // U
        dav1d_pic->data[2], (int)dav1d_pic->stride[1]  // V
        );

    rd_priv_ctx->tex = texture;
    SDL_UnlockMutex(rd_priv_ctx->lock);
    return 0;
}

static const Dav1dPlayRenderInfo renderer_info = {
    .create_renderer = sdl_renderer_create,
    .destroy_renderer = sdl_renderer_destroy,
    .render = sdl_render,
    .update_frame = sdl_update_texture
};

#endif

/**
 * Render context structure
 * This structure contains informations necessary
 * to be shared between the decoder and the renderer
 * threads.
 */
typedef struct render_context
{
    Dav1dPlaySettings settings;
    Dav1dSettings lib_settings;

    // Renderer callbacks
    Dav1dPlayRenderInfo *renderer_info;
    // Renderer private data (passed to callbacks)
    void *rd_priv;

    // Lock to protect access to the context structure
    SDL_mutex *lock;

    // Timestamp of previous decoded frame
    int64_t last_pts;
    // Timestamp of current decoded frame
    int64_t current_pts;
    // Ticks when last frame was received
    uint32_t last_ticks;
    // PTS time base
    double timebase;

    // Fifo
    Dav1dPlayPtrFifo *fifo;

    // Custom SDL2 event type
    uint32_t renderer_event_type;

    // Indicates if termination of the decoder thread was requested
    uint8_t dec_should_terminate;
} Dav1dPlayRenderContext;

static void dp_settings_print_usage(const char *const app,
    const char *const reason, ...)
{
    if (reason) {
        va_list args;

        va_start(args, reason);
        vfprintf(stderr, reason, args);
        va_end(args);
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "Usage: %s [options]\n\n", app);
    fprintf(stderr, "Supported options:\n"
            " --input/-i  $file:    input file\n"
            " --untimed/-u:         ignore PTS, render as fast as possible\n"
            " --framethreads $num:  number of frame threads (default: 1)\n"
            " --tilethreads $num:   number of tile threads (default: 1)\n"
            " --highquality:        enable high quality rendering\n"
            " --zerocopy/-z:        enable zero copy upload path\n"
            " --version/-v:         print version and exit\n");
    exit(1);
}

static unsigned parse_unsigned(const char *const optarg, const int option,
                               const char *const app)
{
    char *end;
    const unsigned res = (unsigned) strtoul(optarg, &end, 0);
    if (*end || end == optarg)
        dp_settings_print_usage(app, "Invalid argument \"%s\" for option %s; should be an integer",
          optarg, option);
    return res;
}

static void dp_rd_ctx_parse_args(Dav1dPlayRenderContext *rd_ctx,
    const int argc, char *const *const argv)
{
    int o;
    Dav1dPlaySettings *settings = &rd_ctx->settings;
    Dav1dSettings *lib_settings = &rd_ctx->lib_settings;

    // Short options
    static const char short_opts[] = "i:vuz";

    enum {
        ARG_FRAME_THREADS = 256,
        ARG_TILE_THREADS,
        ARG_HIGH_QUALITY,
    };

    // Long options
    static const struct option long_opts[] = {
        { "input",          1, NULL, 'i' },
        { "version",        0, NULL, 'v' },
        { "untimed",        0, NULL, 'u' },
        { "framethreads",   1, NULL, ARG_FRAME_THREADS },
        { "tilethreads",    1, NULL, ARG_TILE_THREADS },
        { "highquality",    0, NULL, ARG_HIGH_QUALITY },
        { "zerocopy",       0, NULL, 'z' },
        { NULL,             0, NULL, 0 },
    };

    while ((o = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (o) {
            case 'i':
                settings->inputfile = optarg;
                break;
            case 'v':
                fprintf(stderr, "%s\n", dav1d_version());
                exit(0);
            case 'u':
                settings->untimed = true;
                break;
            case ARG_HIGH_QUALITY:
                settings->highquality = true;
#ifndef HAVE_PLACEBO_VULKAN
                fprintf(stderr, "warning: --highquality requires libplacebo\n");
#endif
                break;
            case 'z':
                settings->zerocopy = true;
#ifndef HAVE_PLACEBO_VULKAN
                fprintf(stderr, "warning: --zerocopy requires libplacebo\n");
#endif
                break;
            case ARG_FRAME_THREADS:
                lib_settings->n_frame_threads =
                    parse_unsigned(optarg, ARG_FRAME_THREADS, argv[0]);
                break;
            case ARG_TILE_THREADS:
                lib_settings->n_tile_threads =
                    parse_unsigned(optarg, ARG_TILE_THREADS, argv[0]);
                break;
            default:
                dp_settings_print_usage(argv[0], NULL);
        }
    }

    if (optind < argc)
        dp_settings_print_usage(argv[0],
            "Extra/unused arguments found, e.g. '%s'\n", argv[optind]);
    if (!settings->inputfile)
        dp_settings_print_usage(argv[0], "Input file (-i/--input) is required");
}

/**
 * Destroy a Dav1dPlayRenderContext
 */
static void dp_rd_ctx_destroy(Dav1dPlayRenderContext *rd_ctx)
{
    assert(rd_ctx != NULL);

    renderer_info.destroy_renderer(rd_ctx->rd_priv);
    dp_fifo_destroy(rd_ctx->fifo);
    SDL_DestroyMutex(rd_ctx->lock);
    free(rd_ctx);
}

/**
 * Create a Dav1dPlayRenderContext
 *
 * \note  The Dav1dPlayRenderContext must be destroyed
 *        again by using dp_rd_ctx_destroy.
 */
static Dav1dPlayRenderContext *dp_rd_ctx_create(void *rd_data)
{
    Dav1dPlayRenderContext *rd_ctx;

    // Alloc
    rd_ctx = malloc(sizeof(Dav1dPlayRenderContext));
    if (rd_ctx == NULL) {
        return NULL;
    }

    // Register a custom event to notify our SDL main thread
    // about new frames
    rd_ctx->renderer_event_type = SDL_RegisterEvents(1);
    if (rd_ctx->renderer_event_type == UINT32_MAX) {
        fprintf(stderr, "Failure to create custom SDL event type!\n");
        free(rd_ctx);
        return NULL;
    }

    rd_ctx->fifo = dp_fifo_create(5);
    if (rd_ctx->fifo == NULL) {
        fprintf(stderr, "Failed to create FIFO for output pictures!\n");
        free(rd_ctx);
        return NULL;
    }

    rd_ctx->lock = SDL_CreateMutex();
    if (rd_ctx->lock == NULL) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        dp_fifo_destroy(rd_ctx->fifo);
        free(rd_ctx);
        return NULL;
    }

    rd_ctx->rd_priv = renderer_info.create_renderer(rd_data);
    if (rd_ctx->rd_priv == NULL) {
        SDL_DestroyMutex(rd_ctx->lock);
        dp_fifo_destroy(rd_ctx->fifo);
        free(rd_ctx);
        return NULL;
    }

    dav1d_default_settings(&rd_ctx->lib_settings);
    memset(&rd_ctx->settings, 0, sizeof(rd_ctx->settings));

    rd_ctx->last_pts = 0;
    rd_ctx->last_ticks = 0;
    rd_ctx->current_pts = 0;
    rd_ctx->timebase = 0;
    rd_ctx->dec_should_terminate = 0;

    return rd_ctx;
}

/**
 * Notify about new available frame
 */
static void dp_rd_ctx_post_event(Dav1dPlayRenderContext *rd_ctx, uint32_t code)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = rd_ctx->renderer_event_type;
    event.user.code = code;
    SDL_PushEvent(&event);
}

/**
 * Update the decoder context with a new dav1d picture
 *
 * Once the decoder decoded a new picture, this call can be used
 * to update the internal texture of the render context with the
 * new picture.
 */
static void dp_rd_ctx_update_with_dav1d_picture(Dav1dPlayRenderContext *rd_ctx,
    Dav1dPicture *dav1d_pic)
{
    renderer_info.update_frame(rd_ctx->rd_priv, dav1d_pic, &rd_ctx->settings);
    rd_ctx->current_pts = dav1d_pic->m.timestamp;
}

/**
 * Terminate decoder thread (async)
 */
static void dp_rd_ctx_request_shutdown(Dav1dPlayRenderContext *rd_ctx)
{
    SDL_LockMutex(rd_ctx->lock);
    rd_ctx->dec_should_terminate = 1;
    SDL_UnlockMutex(rd_ctx->lock);
}

/**
 * Query state of decoder shutdown request
 */
static int dp_rd_ctx_should_terminate(Dav1dPlayRenderContext *rd_ctx)
{
    int ret = 0;
    SDL_LockMutex(rd_ctx->lock);
    ret = rd_ctx->dec_should_terminate;
    SDL_UnlockMutex(rd_ctx->lock);
    return ret;
}

/**
 * Render the currently available texture
 *
 * Renders the currently available texture, if any.
 */
static void dp_rd_ctx_render(Dav1dPlayRenderContext *rd_ctx)
{
    // Calculate time since last frame was received
    uint32_t ticks_now = SDL_GetTicks();
    uint32_t ticks_diff = (rd_ctx->last_ticks != 0) ? ticks_now - rd_ctx->last_ticks : 0;

    // Calculate when to display the frame
    int64_t pts_diff = rd_ctx->current_pts - rd_ctx->last_pts;
    int32_t wait_time = (pts_diff * rd_ctx->timebase) * 1000 - ticks_diff;
    rd_ctx->last_pts = rd_ctx->current_pts;

    // In untimed mode, simply don't wait
    if (rd_ctx->settings.untimed)
        wait_time = 0;

    // This way of timing the playback is not accurate, as there is no guarantee
    // that SDL_Delay will wait for exactly the requested amount of time so in a
    // accurate player this would need to be done in a better way.
    if (wait_time > 0) {
        SDL_Delay(wait_time);
    } else if (wait_time < -10) { // Do not warn for minor time drifts
        fprintf(stderr, "Frame displayed %f seconds too late\n", wait_time/(float)1000);
    }

    renderer_info.render(rd_ctx->rd_priv, &rd_ctx->settings);

    rd_ctx->last_ticks = SDL_GetTicks();
}

/* Decoder thread "main" function */
static int decoder_thread_main(void *cookie)
{
    Dav1dPlayRenderContext *rd_ctx = cookie;

    Dav1dPicture *p;
    Dav1dContext *c = NULL;
    Dav1dData data;
    DemuxerContext *in_ctx = NULL;
    int res = 0;
    unsigned n_out = 0, total, timebase[2], fps[2];

    // Store current ticks for stats calculation
    uint32_t decoder_start = SDL_GetTicks();

    Dav1dPlaySettings settings = rd_ctx->settings;

    // Init dav1d tools demuxers
    init_demuxers();

    if ((res = input_open(&in_ctx, "ivf",
                          settings.inputfile,
                          fps, &total, timebase)) < 0)
    {
        fprintf(stderr, "Failed to open demuxer\n");
        res = 1;
        goto cleanup;
    }

    double timebase_d = timebase[1]/(double)timebase[0];
    rd_ctx->timebase = timebase_d;

    if ((res = dav1d_open(&c, &rd_ctx->lib_settings))) {
        fprintf(stderr, "Failed opening dav1d decoder\n");
        res = 1;
        goto cleanup;
    }

    if ((res = input_read(in_ctx, &data)) < 0) {
        fprintf(stderr, "Failed demuxing input\n");
        res = 1;
        goto cleanup;
    }

    // Decoder loop
    do {
        if (dp_rd_ctx_should_terminate(rd_ctx))
            break;

        // Send data packets we got from the demuxer to dav1d
        if ((res = dav1d_send_data(c, &data)) < 0) {
            // On EAGAIN, dav1d can not consume more data and
            // dav1d_get_picture needs to be called first, which
            // will happen below, so just keep going in that case
            // and do not error out.
            if (res != DAV1D_ERR(EAGAIN)) {
                dav1d_data_unref(&data);
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
        }

        p = calloc(1, sizeof(*p));

        // Try to get a decoded frame
        if ((res = dav1d_get_picture(c, p)) < 0) {
            // In all error cases, even EAGAIN, p needs to be freed as
            // it is never added to the queue and would leak.
            free(p);

            // On EAGAIN, it means dav1d has not enough data to decode
            // therefore this is not a decoding error but just means
            // we need to feed it more data, which happens in the next
            // run of this decoder loop.
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
            res = 0;
        } else {

            // Queue frame
            dp_fifo_push(rd_ctx->fifo, p);
            dp_rd_ctx_post_event(rd_ctx, DAV1D_EVENT_NEW_FRAME);

            n_out++;
        }
    } while ((data.sz > 0 || !input_read(in_ctx, &data)));

    // Release remaining data
    if (data.sz > 0) dav1d_data_unref(&data);

    // Do not drain in case an error occured and caused us to leave the
    // decoding loop early.
    if (res < 0)
        goto cleanup;

    // Drain decoder
    // When there is no more data to feed to the decoder, for example
    // because the file ended, we still need to request pictures, as
    // even though we do not have more data, there can be frames decoded
    // from data we sent before. So we need to call dav1d_get_picture until
    // we get an EAGAIN error.
    do {
        if (dp_rd_ctx_should_terminate(rd_ctx))
            break;

        p = calloc(1, sizeof(*p));
        res = dav1d_get_picture(c, p);
        if (res < 0) {
            free(p);
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
        } else {
            // Queue frame
            dp_fifo_push(rd_ctx->fifo, p);
            dp_rd_ctx_post_event(rd_ctx, DAV1D_EVENT_NEW_FRAME);

            n_out++;
        }
    } while (res != DAV1D_ERR(EAGAIN));

    // Print stats
    uint32_t decoding_time_ms = SDL_GetTicks() - decoder_start;
    printf("Decoded %u frames in %d seconds, avg %.02f fps\n",
        n_out, decoding_time_ms/1000, n_out / (decoding_time_ms / 1000.0));

cleanup:
    dp_rd_ctx_post_event(rd_ctx, DAV1D_EVENT_DEC_QUIT);

    if (in_ctx)
        input_close(in_ctx);
    if (c)
        dav1d_close(&c);

    return (res != DAV1D_ERR(EAGAIN) && res < 0);
}

int main(int argc, char **argv)
{
    SDL_Thread *decoder_thread;
    SDL_Window *win = NULL;

    // Check for version mismatch between library and tool
    const char *version = dav1d_version();
    if (strcmp(version, DAV1D_VERSION)) {
        fprintf(stderr, "Version mismatch (library: %s, executable: %s)\n",
                version, DAV1D_VERSION);
        return 1;
    }

    // Init SDL2 library
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        return 10;

    // Create Window and Renderer
    int window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef HAVE_PLACEBO_VULKAN
    window_flags |= SDL_WINDOW_VULKAN;
#endif
    win = SDL_CreateWindow("Dav1dPlay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, window_flags);
    SDL_SetWindowResizable(win, SDL_TRUE);

    // Create render context
    Dav1dPlayRenderContext *rd_ctx = dp_rd_ctx_create(win);
    if (rd_ctx == NULL) {
        fprintf(stderr, "Failed creating render context\n");
        return 5;
    }

    // Parse and validate arguments
    dp_rd_ctx_parse_args(rd_ctx, argc, argv);

    if (rd_ctx->settings.zerocopy) {
        if (renderer_info.alloc_pic) {
            rd_ctx->lib_settings.allocator = (Dav1dPicAllocator) {
                .cookie = rd_ctx->rd_priv,
                .alloc_picture_callback = renderer_info.alloc_pic,
                .release_picture_callback = renderer_info.release_pic,
            };
        } else {
            fprintf(stderr, "--zerocopy unsupported by compiled renderer\n");
        }
    }

    // Start decoder thread
    decoder_thread = SDL_CreateThread(decoder_thread_main, "Decoder thread", rd_ctx);

    // Main loop
    while (1) {

        SDL_Event e;
        if (SDL_WaitEvent(&e)) {
            if (e.type == SDL_QUIT) {
                dp_rd_ctx_request_shutdown(rd_ctx);
            } else if (e.type == rd_ctx->renderer_event_type) {
                if (e.user.code == DAV1D_EVENT_NEW_FRAME) {
                    // Dequeue frame and update the render context with it
                    Dav1dPicture *p = dp_fifo_shift(rd_ctx->fifo);

                    // Do not update textures during termination
                    if (!dp_rd_ctx_should_terminate(rd_ctx))
                        dp_rd_ctx_update_with_dav1d_picture(rd_ctx, p);
                    dav1d_picture_unref(p);
                    free(p);
                } else if (e.user.code == DAV1D_EVENT_DEC_QUIT) {
                    break;
                }
            }
        }

        // Do not render during termination
        if (!dp_rd_ctx_should_terminate(rd_ctx))
            dp_rd_ctx_render(rd_ctx);
    }

    int decoder_ret = 0;
    SDL_WaitThread(decoder_thread, &decoder_ret);

    dp_rd_ctx_destroy(rd_ctx);
    SDL_DestroyWindow(win);

    return decoder_ret;
}

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

#include <SDL.h>

#include "dav1d/dav1d.h"

#include "common/attributes.h"
#include "tools/input/input.h"
#include "dp_fifo.h"
#include "dp_renderer.h"

// Selected renderer callbacks and cookie
static const Dav1dPlayRenderInfo *renderer_info = { NULL };

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
            " --gpugrain/-g:        enable GPU grain synthesis\n"
            " --version/-v:         print version and exit\n"
            " --renderer/-r:        select renderer backend (default: auto)\n");
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
    static const char short_opts[] = "i:vuzgr:";

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
        { "gpugrain",       0, NULL, 'g' },
        { "renderer",       0, NULL, 'r'},
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
                break;
            case 'z':
                settings->zerocopy = true;
                break;
            case 'g':
                settings->gpugrain = true;
                break;
            case 'r':
                settings->renderer_name = optarg;
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
    if (settings->renderer_name && strcmp(settings->renderer_name, "auto") == 0)
        settings->renderer_name = NULL;
}

/**
 * Destroy a Dav1dPlayRenderContext
 */
static void dp_rd_ctx_destroy(Dav1dPlayRenderContext *rd_ctx)
{
    assert(rd_ctx != NULL);

    renderer_info->destroy_renderer(rd_ctx->rd_priv);
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
static Dav1dPlayRenderContext *dp_rd_ctx_create(int argc, char **argv)
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

    // Parse and validate arguments
    dav1d_default_settings(&rd_ctx->lib_settings);
    memset(&rd_ctx->settings, 0, sizeof(rd_ctx->settings));
    dp_rd_ctx_parse_args(rd_ctx, argc, argv);

    // Select renderer
    renderer_info = dp_get_renderer(rd_ctx->settings.renderer_name);

    if (renderer_info == NULL) {
        printf("No suitable rendered matching %s found.\n",
            (rd_ctx->settings.renderer_name) ? rd_ctx->settings.renderer_name : "auto");
    } else {
        printf("Using %s renderer\n", renderer_info->name);
    }

    rd_ctx->rd_priv = (renderer_info) ? renderer_info->create_renderer() : NULL;
    if (rd_ctx->rd_priv == NULL) {
        SDL_DestroyMutex(rd_ctx->lock);
        dp_fifo_destroy(rd_ctx->fifo);
        free(rd_ctx);
        return NULL;
    }

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
    renderer_info->update_frame(rd_ctx->rd_priv, dav1d_pic, &rd_ctx->settings);
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

    renderer_info->render(rd_ctx->rd_priv, &rd_ctx->settings);

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

    // Create render context
    Dav1dPlayRenderContext *rd_ctx = dp_rd_ctx_create(argc, argv);
    if (rd_ctx == NULL) {
        fprintf(stderr, "Failed creating render context\n");
        return 5;
    }

    if (rd_ctx->settings.zerocopy) {
        if (renderer_info->alloc_pic) {
            rd_ctx->lib_settings.allocator = (Dav1dPicAllocator) {
                .cookie = rd_ctx->rd_priv,
                .alloc_picture_callback = renderer_info->alloc_pic,
                .release_picture_callback = renderer_info->release_pic,
            };
        } else {
            fprintf(stderr, "--zerocopy unsupported by selected renderer\n");
        }
    }

    if (rd_ctx->settings.gpugrain) {
        if (renderer_info->supports_gpu_grain) {
            rd_ctx->lib_settings.apply_grain = 0;
        } else {
            fprintf(stderr, "--gpugrain unsupported by selected renderer\n");
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
            } else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    // TODO: Handle window resizes
                }
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

    return decoder_ret;
}

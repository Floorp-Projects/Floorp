// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "jxl/codestream_header.h"
#include "jxl/decode.h"
#include "jxl/resizable_parallel_runner.h"
#include "jxl/types.h"
#include "skcms.h"

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>
#undef GDK_PIXBUF_ENABLE_BACKEND

G_BEGIN_DECLS

// Information about a single frame.
typedef struct {
  uint64_t duration_ms;
  GdkPixbuf *data;
  gboolean decoded;
} GdkPixbufJxlAnimationFrame;

// Represent a whole JPEG XL animation; all its fields are owned; as a GObject,
// the Animation struct itself is reference counted (as are the GdkPixbufs for
// individual frames).
struct _GdkPixbufJxlAnimation {
  GdkPixbufAnimation parent_instance;

  // GDK interface implementation callbacks.
  GdkPixbufModuleSizeFunc image_size_callback;
  GdkPixbufModulePreparedFunc pixbuf_prepared_callback;
  GdkPixbufModuleUpdatedFunc area_updated_callback;
  gpointer user_data;

  // All frames known so far; a frame is added when the JXL_DEC_FRAME event is
  // received from the decoder; initially frame.decoded is FALSE, until
  // the JXL_DEC_IMAGE event is received.
  GArray *frames;

  // JPEG XL decoder and related structures.
  JxlParallelRunner *parallel_runner;
  JxlDecoder *decoder;
  JxlPixelFormat pixel_format;

  // Decoding is `done` when JXL_DEC_SUCCESS is received; calling
  // load_increment afterwards gives an error.
  gboolean done;

  // Image information.
  size_t xsize;
  size_t ysize;
  gboolean alpha_premultiplied;
  gboolean has_animation;
  gboolean has_alpha;
  uint64_t total_duration_ms;
  uint64_t tick_duration_us;
  uint64_t repetition_count;  // 0 = loop forever

  // ICC profile, to which `icc` might refer to.
  gpointer icc_buff;
  skcms_ICCProfile icc;
};

#define GDK_TYPE_PIXBUF_JXL_ANIMATION (gdk_pixbuf_jxl_animation_get_type())
G_DECLARE_FINAL_TYPE(GdkPixbufJxlAnimation, gdk_pixbuf_jxl_animation, GDK,
                     JXL_ANIMATION, GdkPixbufAnimation);

G_DEFINE_TYPE(GdkPixbufJxlAnimation, gdk_pixbuf_jxl_animation,
              GDK_TYPE_PIXBUF_ANIMATION);

// Iterator to a given point in time in the animation; contains a pointer to the
// full animation.
struct _GdkPixbufJxlAnimationIter {
  GdkPixbufAnimationIter parent_instance;
  GdkPixbufJxlAnimation *animation;
  size_t current_frame;
  uint64_t time_offset;
};

#define GDK_TYPE_PIXBUF_JXL_ANIMATION_ITER \
  (gdk_pixbuf_jxl_animation_iter_get_type())
G_DECLARE_FINAL_TYPE(GdkPixbufJxlAnimationIter, gdk_pixbuf_jxl_animation_iter,
                     GDK, JXL_ANIMATION_ITER, GdkPixbufAnimationIter);
G_DEFINE_TYPE(GdkPixbufJxlAnimationIter, gdk_pixbuf_jxl_animation_iter,
              GDK_TYPE_PIXBUF_ANIMATION_ITER);

static void gdk_pixbuf_jxl_animation_init(GdkPixbufJxlAnimation *obj) {
  // Suppress "unused function" warnings.
  (void)glib_autoptr_cleanup_GdkPixbufJxlAnimation;
  (void)GDK_JXL_ANIMATION;
  (void)GDK_IS_JXL_ANIMATION;
}

static gboolean gdk_pixbuf_jxl_animation_is_static_image(
    GdkPixbufAnimation *anim) {
  GdkPixbufJxlAnimation *jxl_anim = (GdkPixbufJxlAnimation *)anim;
  return !jxl_anim->has_animation;
}

static GdkPixbuf *gdk_pixbuf_jxl_animation_get_static_image(
    GdkPixbufAnimation *anim) {
  GdkPixbufJxlAnimation *jxl_anim = (GdkPixbufJxlAnimation *)anim;
  if (jxl_anim->frames == NULL || jxl_anim->frames->len == 0) return NULL;
  GdkPixbufJxlAnimationFrame *frame =
      &g_array_index(jxl_anim->frames, GdkPixbufJxlAnimationFrame, 0);
  return frame->decoded ? frame->data : NULL;
}

static void gdk_pixbuf_jxl_animation_get_size(GdkPixbufAnimation *anim,
                                              int *width, int *height) {
  GdkPixbufJxlAnimation *jxl_anim = (GdkPixbufJxlAnimation *)anim;
  if (width) *width = jxl_anim->xsize;
  if (height) *height = jxl_anim->ysize;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static gboolean gdk_pixbuf_jxl_animation_iter_advance(
    GdkPixbufAnimationIter *iter, const GTimeVal *current_time);

static GdkPixbufAnimationIter *gdk_pixbuf_jxl_animation_get_iter(
    GdkPixbufAnimation *anim, const GTimeVal *start_time) {
  GdkPixbufJxlAnimationIter *iter =
      g_object_new(GDK_TYPE_PIXBUF_JXL_ANIMATION_ITER, NULL);
  iter->animation = (GdkPixbufJxlAnimation *)anim;
  iter->time_offset = start_time->tv_sec * 1000ULL + start_time->tv_usec / 1000;
  g_object_ref(iter->animation);
  gdk_pixbuf_jxl_animation_iter_advance((GdkPixbufAnimationIter *)iter,
                                        start_time);
  return (GdkPixbufAnimationIter *)iter;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void gdk_pixbuf_jxl_animation_finalize(GObject *obj) {
  GdkPixbufJxlAnimation *decoder_state = (GdkPixbufJxlAnimation *)obj;
  if (decoder_state->frames != NULL) {
    for (size_t i = 0; i < decoder_state->frames->len; i++) {
      g_object_unref(
          g_array_index(decoder_state->frames, GdkPixbufJxlAnimationFrame, i)
              .data);
    }
    g_array_free(decoder_state->frames, /*free_segment=*/TRUE);
  }
  JxlResizableParallelRunnerDestroy(decoder_state->parallel_runner);
  JxlDecoderDestroy(decoder_state->decoder);
  g_free(decoder_state->icc_buff);
}

static void gdk_pixbuf_jxl_animation_class_init(
    GdkPixbufJxlAnimationClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = gdk_pixbuf_jxl_animation_finalize;
  klass->parent_class.is_static_image =
      gdk_pixbuf_jxl_animation_is_static_image;
  klass->parent_class.get_static_image =
      gdk_pixbuf_jxl_animation_get_static_image;
  klass->parent_class.get_size = gdk_pixbuf_jxl_animation_get_size;
  klass->parent_class.get_iter = gdk_pixbuf_jxl_animation_get_iter;
}

static void gdk_pixbuf_jxl_animation_iter_init(GdkPixbufJxlAnimationIter *obj) {
  (void)glib_autoptr_cleanup_GdkPixbufJxlAnimationIter;
  (void)GDK_JXL_ANIMATION_ITER;
  (void)GDK_IS_JXL_ANIMATION_ITER;
}

static int gdk_pixbuf_jxl_animation_iter_get_delay_time(
    GdkPixbufAnimationIter *iter) {
  GdkPixbufJxlAnimationIter *jxl_iter = (GdkPixbufJxlAnimationIter *)iter;
  if (jxl_iter->animation->frames->len <= jxl_iter->current_frame) {
    return 0;
  }
  return g_array_index(jxl_iter->animation->frames, GdkPixbufJxlAnimationFrame,
                       jxl_iter->current_frame)
      .duration_ms;
}

static GdkPixbuf *gdk_pixbuf_jxl_animation_iter_get_pixbuf(
    GdkPixbufAnimationIter *iter) {
  GdkPixbufJxlAnimationIter *jxl_iter = (GdkPixbufJxlAnimationIter *)iter;
  if (jxl_iter->animation->frames->len <= jxl_iter->current_frame) {
    return NULL;
  }
  return g_array_index(jxl_iter->animation->frames, GdkPixbufJxlAnimationFrame,
                       jxl_iter->current_frame)
      .data;
}

static gboolean gdk_pixbuf_jxl_animation_iter_on_currently_loading_frame(
    GdkPixbufAnimationIter *iter) {
  GdkPixbufJxlAnimationIter *jxl_iter = (GdkPixbufJxlAnimationIter *)iter;
  if (jxl_iter->animation->frames->len <= jxl_iter->current_frame) {
    return TRUE;
  }
  return !g_array_index(jxl_iter->animation->frames, GdkPixbufJxlAnimationFrame,
                        jxl_iter->current_frame)
              .decoded;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static gboolean gdk_pixbuf_jxl_animation_iter_advance(
    GdkPixbufAnimationIter *iter, const GTimeVal *current_time) {
  GdkPixbufJxlAnimationIter *jxl_iter = (GdkPixbufJxlAnimationIter *)iter;
  size_t old_frame = jxl_iter->current_frame;

  uint64_t current_time_ms = current_time->tv_sec * 1000ULL +
                             current_time->tv_usec / 1000 -
                             jxl_iter->time_offset;

  if (jxl_iter->animation->frames->len == 0) {
    jxl_iter->current_frame = 0;
  } else if (!jxl_iter->animation->done &&
             current_time_ms >= jxl_iter->animation->total_duration_ms) {
    jxl_iter->current_frame = jxl_iter->animation->frames->len - 1;
  } else if (jxl_iter->animation->repetition_count != 0 &&
             current_time_ms > jxl_iter->animation->repetition_count *
                                   jxl_iter->animation->total_duration_ms) {
    jxl_iter->current_frame = jxl_iter->animation->frames->len - 1;
  } else {
    uint64_t total_duration_ms = jxl_iter->animation->total_duration_ms;
    // Guard against divide-by-0 in malicious files.
    if (total_duration_ms == 0) total_duration_ms = 1;
    uint64_t loop_offset = current_time_ms % total_duration_ms;
    jxl_iter->current_frame = 0;
    while (true) {
      uint64_t duration =
          g_array_index(jxl_iter->animation->frames, GdkPixbufJxlAnimationFrame,
                        jxl_iter->current_frame)
              .duration_ms;
      if (duration >= loop_offset) {
        break;
      }
      loop_offset -= duration;
      jxl_iter->current_frame++;
    }
  }

  return old_frame != jxl_iter->current_frame;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void gdk_pixbuf_jxl_animation_iter_finalize(GObject *obj) {
  GdkPixbufJxlAnimationIter *iter = (GdkPixbufJxlAnimationIter *)obj;
  g_object_unref(iter->animation);
}

static void gdk_pixbuf_jxl_animation_iter_class_init(
    GdkPixbufJxlAnimationIterClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = gdk_pixbuf_jxl_animation_iter_finalize;
  klass->parent_class.get_delay_time =
      gdk_pixbuf_jxl_animation_iter_get_delay_time;
  klass->parent_class.get_pixbuf = gdk_pixbuf_jxl_animation_iter_get_pixbuf;
  klass->parent_class.on_currently_loading_frame =
      gdk_pixbuf_jxl_animation_iter_on_currently_loading_frame;
  klass->parent_class.advance = gdk_pixbuf_jxl_animation_iter_advance;
}

G_END_DECLS

static gpointer begin_load(GdkPixbufModuleSizeFunc size_func,
                           GdkPixbufModulePreparedFunc prepare_func,
                           GdkPixbufModuleUpdatedFunc update_func,
                           gpointer user_data, GError **error) {
  GdkPixbufJxlAnimation *decoder_state =
      g_object_new(GDK_TYPE_PIXBUF_JXL_ANIMATION, NULL);
  if (decoder_state == NULL) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Creation of the animation state failed");
    return NULL;
  }
  decoder_state->image_size_callback = size_func;
  decoder_state->pixbuf_prepared_callback = prepare_func;
  decoder_state->area_updated_callback = update_func;
  decoder_state->user_data = user_data;
  decoder_state->frames =
      g_array_new(/*zero_terminated=*/FALSE, /*clear_=*/TRUE,
                  sizeof(GdkPixbufJxlAnimationFrame));

  if (decoder_state->frames == NULL) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Creation of the frame array failed");
    goto cleanup;
  }

  if (!(decoder_state->parallel_runner =
            JxlResizableParallelRunnerCreate(NULL))) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Creation of the JXL parallel runner failed");
    goto cleanup;
  }

  if (!(decoder_state->decoder = JxlDecoderCreate(NULL))) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Creation of the JXL decoder failed");
    goto cleanup;
  }

  JxlDecoderStatus status;

  if ((status = JxlDecoderSetParallelRunner(
           decoder_state->decoder, JxlResizableParallelRunner,
           decoder_state->parallel_runner)) != JXL_DEC_SUCCESS) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "JxlDecoderSetParallelRunner failed: %x", status);
    goto cleanup;
  }
  if ((status = JxlDecoderSubscribeEvents(
           decoder_state->decoder, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                                       JXL_DEC_FULL_IMAGE | JXL_DEC_FRAME)) !=
      JXL_DEC_SUCCESS) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "JxlDecoderSubscribeEvents failed: %x", status);
    goto cleanup;
  }

  decoder_state->pixel_format.data_type = JXL_TYPE_FLOAT;
  decoder_state->pixel_format.endianness = JXL_NATIVE_ENDIAN;

  return decoder_state;
cleanup:
  JxlResizableParallelRunnerDestroy(decoder_state->parallel_runner);
  JxlDecoderDestroy(decoder_state->decoder);
  g_object_unref(decoder_state);
  return NULL;
}

static gboolean stop_load(gpointer context, GError **error) {
  g_object_unref(context);
  return TRUE;
}

static void draw_pixels(void *context, size_t x, size_t y, size_t num_pixels,
                        const void *pixels) {
  GdkPixbufJxlAnimation *decoder_state = context;
  gboolean has_alpha = decoder_state->pixel_format.num_channels == 4;

  GdkPixbuf *output =
      g_array_index(decoder_state->frames, GdkPixbufJxlAnimationFrame,
                    decoder_state->frames->len - 1)
          .data;

  guchar *dst = gdk_pixbuf_get_pixels(output) +
                decoder_state->pixel_format.num_channels * x +
                gdk_pixbuf_get_rowstride(output) * y;

  skcms_Transform(
      pixels,
      has_alpha ? skcms_PixelFormat_RGBA_ffff : skcms_PixelFormat_RGB_fff,
      decoder_state->alpha_premultiplied ? skcms_AlphaFormat_PremulAsEncoded
                                         : skcms_AlphaFormat_Unpremul,
      &decoder_state->icc, dst,
      has_alpha ? skcms_PixelFormat_RGBA_8888 : skcms_PixelFormat_RGB_888,
      skcms_AlphaFormat_Unpremul, skcms_sRGB_profile(), num_pixels);
}

static gboolean load_increment(gpointer context, const guchar *buf, guint size,
                               GError **error) {
  GdkPixbufJxlAnimation *decoder_state = context;
  if (decoder_state->done == TRUE) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "JXL decoder load_increment called after end of file");
    return FALSE;
  }

  JxlDecoderStatus status;

  if ((status = JxlDecoderSetInput(decoder_state->decoder, buf, size)) !=
      JXL_DEC_SUCCESS) {
    // Should never happen if things are done properly.
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "JXL decoder logic error: %x", status);
    return FALSE;
  }

  for (;;) {
    status = JxlDecoderProcessInput(decoder_state->decoder);
    switch (status) {
      case JXL_DEC_NEED_MORE_INPUT: {
        JxlDecoderReleaseInput(decoder_state->decoder);
        return TRUE;
      }

      case JXL_DEC_BASIC_INFO: {
        JxlBasicInfo info;
        if (JxlDecoderGetBasicInfo(decoder_state->decoder, &info) !=
            JXL_DEC_SUCCESS) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "JXLDecoderGetBasicInfo failed");
          return FALSE;
        }
        decoder_state->pixel_format.num_channels = info.alpha_bits > 0 ? 4 : 3;
        decoder_state->alpha_premultiplied = info.alpha_premultiplied;
        decoder_state->xsize = info.xsize;
        decoder_state->ysize = info.ysize;
        decoder_state->has_animation = info.have_animation;
        decoder_state->has_alpha = info.alpha_bits > 0;
        if (info.have_animation) {
          decoder_state->repetition_count = info.animation.num_loops;
          decoder_state->tick_duration_us = 1000000ULL *
                                            info.animation.tps_denominator /
                                            info.animation.tps_numerator;
        }
        gint width = info.xsize;
        gint height = info.ysize;
        if (decoder_state->image_size_callback) {
          decoder_state->image_size_callback(&width, &height,
                                             decoder_state->user_data);
        }

        // GDK convention for signaling being interested only in the basic info.
        if (width == 0 || height == 0) {
          decoder_state->done = TRUE;
          return TRUE;
        }

        // Set an appropriate number of threads for the image size.
        JxlResizableParallelRunnerSetThreads(
            decoder_state->parallel_runner,
            JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        break;
      }

      case JXL_DEC_COLOR_ENCODING: {
        // Get the ICC color profile of the pixel data
        size_t icc_size;
        if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(
                                   decoder_state->decoder,
                                   &decoder_state->pixel_format,
                                   JXL_COLOR_PROFILE_TARGET_DATA, &icc_size)) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "JxlDecoderGetICCProfileSize failed");
          return FALSE;
        }
        if (!(decoder_state->icc_buff = g_malloc(icc_size))) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "Allocating ICC profile failed");
          return FALSE;
        }
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetColorAsICCProfile(decoder_state->decoder,
                                           &decoder_state->pixel_format,
                                           JXL_COLOR_PROFILE_TARGET_DATA,
                                           decoder_state->icc_buff, icc_size)) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "JxlDecoderGetColorAsICCProfile failed");
          return FALSE;
        }
        if (!skcms_Parse(decoder_state->icc_buff, icc_size,
                         &decoder_state->icc)) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "Invalid ICC profile from JXL image decoder");
          return FALSE;
        }
        break;
      }

      case JXL_DEC_FRAME: {
        // TODO(veluca): support rescaling.
        JxlFrameHeader frame_header;
        if (JxlDecoderGetFrameHeader(decoder_state->decoder, &frame_header) !=
            JXL_DEC_SUCCESS) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "Failed to retrieve frame info");
          return FALSE;
        }

        {
          GdkPixbufJxlAnimationFrame frame;
          frame.decoded = FALSE;
          frame.duration_ms =
              frame_header.duration * decoder_state->tick_duration_us / 1000;
          decoder_state->total_duration_ms += frame.duration_ms;
          frame.data =
              gdk_pixbuf_new(GDK_COLORSPACE_RGB, decoder_state->has_alpha,
                             /*bits_per_sample=*/8, decoder_state->xsize,
                             decoder_state->ysize);
          if (frame.data == NULL) {
            g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                        "Failed to allocate output pixel buffer");
            return FALSE;
          }
          decoder_state->pixel_format.align =
              gdk_pixbuf_get_rowstride(frame.data);
          g_array_append_val(decoder_state->frames, frame);
        }
        if (decoder_state->pixbuf_prepared_callback &&
            decoder_state->frames->len == 1) {
          decoder_state->pixbuf_prepared_callback(
              g_array_index(decoder_state->frames, GdkPixbufJxlAnimationFrame,
                            0)
                  .data,
              decoder_state->has_animation ? (GdkPixbufAnimation *)decoder_state
                                           : NULL,
              decoder_state->user_data);
        }
        break;
      }

      case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
        if (JXL_DEC_SUCCESS !=
            JxlDecoderSetImageOutCallback(decoder_state->decoder,
                                          &decoder_state->pixel_format,
                                          draw_pixels, decoder_state)) {
          g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                      "JxlDecoderSetImageOutCallback failed");
          return FALSE;
        }
        break;
      }

      case JXL_DEC_FULL_IMAGE: {
        // TODO(veluca): consider doing partial updates.
        if (decoder_state->area_updated_callback) {
          GdkPixbuf *output = g_array_index(decoder_state->frames,
                                            GdkPixbufJxlAnimationFrame, 0)
                                  .data;
          decoder_state->area_updated_callback(
              output, 0, 0, gdk_pixbuf_get_width(output),
              gdk_pixbuf_get_height(output), decoder_state->user_data);
        }
        g_array_index(decoder_state->frames, GdkPixbufJxlAnimationFrame,
                      decoder_state->frames->len - 1)
            .decoded = TRUE;
        break;
      }

      case JXL_DEC_SUCCESS: {
        decoder_state->done = TRUE;
        return TRUE;
      }

      default: {
        g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                    "Unexpected JxlDecoderProcessInput return code: %x",
                    status);
        return FALSE;
      }
    }
  }
  return TRUE;
}

void fill_vtable(GdkPixbufModule *module) {
  module->begin_load = begin_load;
  module->stop_load = stop_load;
  module->load_increment = load_increment;
  // TODO(veluca): implement saving.
}

void fill_info(GdkPixbufFormat *info) {
  static GdkPixbufModulePattern signature[] = {
      {"\xFF\x0A", "  ", 100},
      {"...\x0CJXL \x0D\x0A\x87\x0A", "zzz         ", 100},
      {NULL, NULL, 0},
  };

  static gchar *mime_types[] = {"image/jxl", NULL};

  static gchar *extensions[] = {"jxl", NULL};

  info->name = "jxl";
  info->signature = signature;
  info->description = "JPEG XL image";
  info->mime_types = mime_types;
  info->extensions = extensions;
  // TODO(veluca): add writing support.
  info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
  info->license = "BSD-3";
}

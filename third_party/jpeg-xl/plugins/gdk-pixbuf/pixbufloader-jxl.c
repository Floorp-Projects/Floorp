// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>
#undef GDK_PIXBUF_ENABLE_BACKEND

#include "jxl/decode.h"

static void DestroyPixels(guchar *pixels, gpointer data) { free(pixels); }

typedef struct {
  GdkPixbufModuleSizeFunc size_func;
  GdkPixbufModuleUpdatedFunc update_func;
  GdkPixbufModulePreparedFunc prepare_func;
  gpointer user_data;
  GdkPixbuf *pixbuf;
  GError **error;

  FILE *increment_buffer;
  char *increment_buffer_ptr;
  size_t increment_buffer_size;

} JxlContext;

uint8_t *JxlMemoryToPixels(const uint8_t *next_in, size_t size, size_t *stride,
                           size_t *xsize, size_t *ysize, int *has_alpha) {
  JxlDecoder *dec = JxlDecoderCreate(NULL);
  *has_alpha = 1;
  uint8_t *pixels = NULL;
  if (!dec) {
    fprintf(stderr, "JxlDecoderCreate failed\n");
    return 0;
  }
  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE)) {
    fprintf(stderr, "JxlDecoderSubscribeEvents failed\n");
    JxlDecoderDestroy(dec);
    return 0;
  }

  JxlBasicInfo info;
  int success = 0;
  JxlPixelFormat format = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
  JxlDecoderSetInput(dec, next_in, size);

  for (;;) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec);

    if (status == JXL_DEC_ERROR) {
      fprintf(stderr, "Decoder error\n");
      break;
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      fprintf(stderr, "Error, already provided all input\n");
      break;
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info)) {
        fprintf(stderr, "JxlDecoderGetBasicInfo failed\n");
        break;
      }
      *xsize = info.xsize;
      *ysize = info.ysize;
      *stride = info.xsize * 4;
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t buffer_size;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(dec, &format, &buffer_size)) {
        fprintf(stderr, "JxlDecoderImageOutBufferSize failed\n");
        break;
      }
      if (buffer_size != *stride * *ysize) {
        fprintf(stderr, "Invalid out buffer size %zu %zu\n", buffer_size,
                *stride * *ysize);
        break;
      }
      size_t pixels_buffer_size = buffer_size * sizeof(uint8_t);
      pixels = malloc(pixels_buffer_size);
      void *pixels_buffer = (void *)pixels;
      if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec, &format,
                                                         pixels_buffer,
                                                         pixels_buffer_size)) {
        fprintf(stderr, "JxlDecoderSetImageOutBuffer failed\n");
        break;
      }
    } else if (status == JXL_DEC_FULL_IMAGE) {
      // This means the decoder has decoded all pixels into the buffer.
      success = 1;
      break;
    } else if (status == JXL_DEC_SUCCESS) {
      fprintf(stderr, "Decoding finished before receiving pixel data\n");
      break;
    } else {
      fprintf(stderr, "Unexpected decoder status: %d\n", status);
      break;
    }
  }
  if (success){
    return pixels;
  } else {
    free(pixels);
    return NULL;
  }
}

static GdkPixbuf *gdk_pixbuf__jxl_image_load(FILE *f, GError **error) {
  size_t data_size;
  int status;
  gpointer data;

  // Get data size
  status = fseek(f, 0, SEEK_END);
  if (status) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Failed to find end of file");
  }
  data_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  status = fseek(f, 0, SEEK_SET);
  if (status) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Failed to set pointer to beginning of file");
  }

  // Get data
  data = g_malloc(data_size);
  status = (fread(data, data_size, 1, f) == 1);
  if (!status) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Failed to read file");
    g_free(data);
    return NULL;
  }
  size_t xsize, ysize, stride;
  int has_alpha;
  uint8_t *decoded =
      JxlMemoryToPixels(data, data_size, &stride, &xsize, &ysize, &has_alpha);
  g_free(data);
  if (!decoded) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Failed to decode data");
    return NULL;
  }

  GdkPixbuf *pixbuf =
      gdk_pixbuf_new_from_data(decoded, GDK_COLORSPACE_RGB, has_alpha, 8, xsize,
                               ysize, stride, &DestroyPixels, NULL);

  if (!pixbuf) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Failed to create output pixbuf");
    free(decoded);
    return NULL;
  }

  return pixbuf;
}

static gpointer gdk_pixbuf__jxl_image_begin_load(
    GdkPixbufModuleSizeFunc size_func, GdkPixbufModulePreparedFunc prepare_func,
    GdkPixbufModuleUpdatedFunc update_func, gpointer user_data,
    GError **error) {
  JxlContext *context = g_new(JxlContext, 1);
  context->size_func = size_func;
  context->prepare_func = prepare_func;
  context->update_func = update_func;
  context->user_data = user_data;
  context->error = error;

  context->increment_buffer = open_memstream(&context->increment_buffer_ptr,
                                             &context->increment_buffer_size);

  if (!context->increment_buffer) {
    perror("Cannot create increment buffer.");
    g_free(context);
    return NULL;
  }

  return context;
}

static gboolean gdk_pixbuf__jxl_image_stop_load(gpointer user_context,
                                                GError **error) {
  JxlContext *context = (JxlContext *)user_context;

  int status = fflush(context->increment_buffer);
  status |= fseek(context->increment_buffer, 0L, SEEK_SET);

  if (status != 0) {
    perror("Cannot flush and rewind increment buffer.");
    fclose(context->increment_buffer);
    free(context->increment_buffer_ptr);
    g_free(context);
    return FALSE;
  }

  context->pixbuf =
      gdk_pixbuf__jxl_image_load(context->increment_buffer, error);

  gint width = gdk_pixbuf_get_width(context->pixbuf);
  gint height = gdk_pixbuf_get_height(context->pixbuf);
  if (context->size_func) {
    context->size_func(&width, &height, context->user_data);
  }

  if (context->prepare_func) {
    (*context->prepare_func)(context->pixbuf, NULL, context->user_data);
  }

  if (context->update_func) {
    (*context->update_func)(
        context->pixbuf, 0, 0, gdk_pixbuf_get_width(context->pixbuf),
        gdk_pixbuf_get_height(context->pixbuf), context->user_data);
  }

  fclose(context->increment_buffer);

  free(context->increment_buffer_ptr);

  g_object_unref(context->pixbuf);
  g_free(context);

  return TRUE;
}

static gboolean gdk_pixbuf__jxl_image_load_increment(gpointer user_context,
                                                     const guchar *buf,
                                                     guint size,
                                                     GError **error) {
  JxlContext *context = (JxlContext *)user_context;

  int status = fwrite(buf, size, sizeof(guchar), context->increment_buffer);

  if (status != sizeof(guchar)) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Can't write to increment buffer.");
    return FALSE;
  }

  status = fflush(context->increment_buffer);

  if (status != 0) {
    g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                "Can't flush the increment buffer.");
    return FALSE;
  }

  return TRUE;
}

void fill_vtable(GdkPixbufModule *module) {
  module->load = gdk_pixbuf__jxl_image_load;
  module->begin_load = gdk_pixbuf__jxl_image_begin_load;
  module->stop_load = gdk_pixbuf__jxl_image_stop_load;
  module->load_increment = gdk_pixbuf__jxl_image_load_increment;
}

void fill_info(GdkPixbufFormat *info) {
  static GdkPixbufModulePattern signature[] = {
      {"\xd7\x4c\x4d\x0a", "    ", 100}, {NULL, NULL, 0}};

  static gchar *mime_types[] = {"image/jxl", NULL};

  static gchar *extensions[] = {"jxl", NULL};

  info->name = "JPEG XL";
  info->signature = signature;
  info->description = "JPEG XL image";
  info->mime_types = mime_types;
  info->extensions = extensions;
  info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
  info->license = "Apache 2";
}

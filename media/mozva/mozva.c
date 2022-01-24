/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma GCC visibility push(default)
#include <va/va.h>
#pragma GCC visibility pop

#include "mozilla/Types.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

#define GET_FUNC(func, lib) (func##Fn = dlsym(lib, #func))

#define IS_FUNC_LOADED(func) (func##Fn != NULL)

static VAStatus (*vaDestroyBufferFn)(VADisplay dpy, VABufferID buffer_id);
static VAStatus (*vaBeginPictureFn)(VADisplay dpy, VAContextID context,
                                    VASurfaceID render_target);
static VAStatus (*vaEndPictureFn)(VADisplay dpy, VAContextID context);
static VAStatus (*vaRenderPictureFn)(VADisplay dpy, VAContextID context,
                                     VABufferID* buffers, int num_buffers);
static int (*vaMaxNumProfilesFn)(VADisplay dpy);
static VAStatus (*vaCreateContextFn)(VADisplay dpy, VAConfigID config_id,
                                     int picture_width, int picture_height,
                                     int flag, VASurfaceID* render_targets,
                                     int num_render_targets,
                                     VAContextID* context /* out */);
static VAStatus (*vaDestroyContextFn)(VADisplay dpy, VAContextID context);
static VAStatus (*vaCreateBufferFn)(VADisplay dpy, VAContextID context,
                                    VABufferType type,         /* in */
                                    unsigned int size,         /* in */
                                    unsigned int num_elements, /* in */
                                    void* data,                /* in */
                                    VABufferID* buf_id /* out */);
static VAStatus (*vaQuerySurfaceAttributesFn)(VADisplay dpy, VAConfigID config,
                                              VASurfaceAttrib* attrib_list,
                                              unsigned int* num_attribs);
static VAStatus (*vaQueryConfigProfilesFn)(VADisplay dpy,
                                           VAProfile* profile_list, /* out */
                                           int* num_profiles /* out */);
static const char* (*vaErrorStrFn)(VAStatus error_status);
static VAStatus (*vaCreateConfigFn)(VADisplay dpy, VAProfile profile,
                                    VAEntrypoint entrypoint,
                                    VAConfigAttrib* attrib_list,
                                    int num_attribs,
                                    VAConfigID* config_id /* out */);
static VAStatus (*vaDestroyConfigFn)(VADisplay dpy, VAConfigID config_id);
static int (*vaMaxNumImageFormatsFn)(VADisplay dpy);
static VAStatus (*vaQueryImageFormatsFn)(VADisplay dpy,
                                         VAImageFormat* format_list, /* out */
                                         int* num_formats /* out */);
static const char* (*vaQueryVendorStringFn)(VADisplay dpy);
static VAStatus (*vaDestroySurfacesFn)(VADisplay dpy, VASurfaceID* surfaces,
                                       int num_surfaces);
static VAStatus (*vaCreateSurfacesFn)(VADisplay dpy, unsigned int format,
                                      unsigned int width, unsigned int height,
                                      VASurfaceID* surfaces,
                                      unsigned int num_surfaces,
                                      VASurfaceAttrib* attrib_list,
                                      unsigned int num_attribs);
static VAStatus (*vaDeriveImageFn)(VADisplay dpy, VASurfaceID surface,
                                   VAImage* image /* out */);
static VAStatus (*vaDestroyImageFn)(VADisplay dpy, VAImageID image);
static VAStatus (*vaPutImageFn)(VADisplay dpy, VASurfaceID surface,
                                VAImageID image, int src_x, int src_y,
                                unsigned int src_width, unsigned int src_height,
                                int dest_x, int dest_y, unsigned int dest_width,
                                unsigned int dest_height);
static VAStatus (*vaSyncSurfaceFn)(VADisplay dpy, VASurfaceID render_target);
static VAStatus (*vaCreateImageFn)(VADisplay dpy, VAImageFormat* format,
                                   int width, int height,
                                   VAImage* image /* out */);
static VAStatus (*vaGetImageFn)(
    VADisplay dpy, VASurfaceID surface,
    int x,                     /* coordinates of the upper left source pixel */
    int y, unsigned int width, /* width and height of the region */
    unsigned int height, VAImageID image);
static VAStatus (*vaMapBufferFn)(VADisplay dpy, VABufferID buf_id, /* in */
                                 void** pbuf /* out */);
static VAStatus (*vaUnmapBufferFn)(VADisplay dpy, VABufferID buf_id /* in */);
static VAStatus (*vaTerminateFn)(VADisplay dpy);
static VAStatus (*vaInitializeFn)(VADisplay dpy, int* major_version, /* out */
                                  int* minor_version /* out */);
static VAStatus (*vaSetDriverNameFn)(VADisplay dpy, char* driver_name);
static int (*vaMaxNumEntrypointsFn)(VADisplay dpy);
static VAStatus (*vaQueryConfigEntrypointsFn)(VADisplay dpy, VAProfile profile,
                                              VAEntrypoint* entrypoint_list,
                                              int* num_entrypoints);
static VAMessageCallback (*vaSetErrorCallbackFn)(VADisplay dpy,
                                                 VAMessageCallback callback,
                                                 void* user_context);
static VAMessageCallback (*vaSetInfoCallbackFn)(VADisplay dpy,
                                                VAMessageCallback callback,
                                                void* user_context);
int LoadVALibrary() {
  static pthread_mutex_t sVALock = PTHREAD_MUTEX_INITIALIZER;
  static void* sVALib = NULL;
  static int sVAInitialized = 0;
  static int sVALoaded = 0;

  pthread_mutex_lock(&sVALock);

  if (!sVAInitialized) {
    sVAInitialized = 1;
    sVALib = dlopen("libva.so.2", RTLD_LAZY);
    if (!sVALib) {
      pthread_mutex_unlock(&sVALock);
      return 0;
    }
    GET_FUNC(vaDestroyBuffer, sVALib);
    GET_FUNC(vaBeginPicture, sVALib);
    GET_FUNC(vaEndPicture, sVALib);
    GET_FUNC(vaRenderPicture, sVALib);
    GET_FUNC(vaMaxNumProfiles, sVALib);
    GET_FUNC(vaCreateContext, sVALib);
    GET_FUNC(vaDestroyContext, sVALib);
    GET_FUNC(vaCreateBuffer, sVALib);
    GET_FUNC(vaQuerySurfaceAttributes, sVALib);
    GET_FUNC(vaQueryConfigProfiles, sVALib);
    GET_FUNC(vaErrorStr, sVALib);
    GET_FUNC(vaCreateConfig, sVALib);
    GET_FUNC(vaDestroyConfig, sVALib);
    GET_FUNC(vaMaxNumImageFormats, sVALib);
    GET_FUNC(vaQueryImageFormats, sVALib);
    GET_FUNC(vaQueryVendorString, sVALib);
    GET_FUNC(vaDestroySurfaces, sVALib);
    GET_FUNC(vaCreateSurfaces, sVALib);
    GET_FUNC(vaDeriveImage, sVALib);
    GET_FUNC(vaDestroyImage, sVALib);
    GET_FUNC(vaPutImage, sVALib);
    GET_FUNC(vaSyncSurface, sVALib);
    GET_FUNC(vaCreateImage, sVALib);
    GET_FUNC(vaGetImage, sVALib);
    GET_FUNC(vaMapBuffer, sVALib);
    GET_FUNC(vaUnmapBuffer, sVALib);
    GET_FUNC(vaTerminate, sVALib);
    GET_FUNC(vaInitialize, sVALib);
    GET_FUNC(vaSetDriverName, sVALib);
    GET_FUNC(vaMaxNumEntrypoints, sVALib);
    GET_FUNC(vaQueryConfigEntrypoints, sVALib);
    GET_FUNC(vaSetErrorCallback, sVALib);
    GET_FUNC(vaSetInfoCallback, sVALib);

    sVALoaded =
        (IS_FUNC_LOADED(vaDestroyBuffer) && IS_FUNC_LOADED(vaBeginPicture) &&
         IS_FUNC_LOADED(vaEndPicture) && IS_FUNC_LOADED(vaRenderPicture) &&
         IS_FUNC_LOADED(vaMaxNumProfiles) && IS_FUNC_LOADED(vaCreateContext) &&
         IS_FUNC_LOADED(vaDestroyContext) && IS_FUNC_LOADED(vaCreateBuffer) &&
         IS_FUNC_LOADED(vaQuerySurfaceAttributes) &&
         IS_FUNC_LOADED(vaQueryConfigProfiles) && IS_FUNC_LOADED(vaErrorStr) &&
         IS_FUNC_LOADED(vaCreateConfig) && IS_FUNC_LOADED(vaDestroyConfig) &&
         IS_FUNC_LOADED(vaMaxNumImageFormats) &&
         IS_FUNC_LOADED(vaQueryImageFormats) &&
         IS_FUNC_LOADED(vaQueryVendorString) &&
         IS_FUNC_LOADED(vaDestroySurfaces) &&
         IS_FUNC_LOADED(vaCreateSurfaces) && IS_FUNC_LOADED(vaDeriveImage) &&
         IS_FUNC_LOADED(vaDestroyImage) && IS_FUNC_LOADED(vaPutImage) &&
         IS_FUNC_LOADED(vaSyncSurface) && IS_FUNC_LOADED(vaCreateImage) &&
         IS_FUNC_LOADED(vaGetImage) && IS_FUNC_LOADED(vaMapBuffer) &&
         IS_FUNC_LOADED(vaUnmapBuffer) && IS_FUNC_LOADED(vaTerminate) &&
         IS_FUNC_LOADED(vaInitialize) && IS_FUNC_LOADED(vaSetDriverName) &&
         IS_FUNC_LOADED(vaMaxNumEntrypoints) &&
         IS_FUNC_LOADED(vaQueryConfigEntrypoints) &&
         IS_FUNC_LOADED(vaSetErrorCallback) &&
         IS_FUNC_LOADED(vaSetInfoCallback));
  }
  pthread_mutex_unlock(&sVALock);
  return sVALoaded;
}

#pragma GCC visibility push(default)

VAStatus vaDestroyBuffer(VADisplay dpy, VABufferID buffer_id) {
  if (LoadVALibrary()) {
    return vaDestroyBufferFn(dpy, buffer_id);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaBeginPicture(VADisplay dpy, VAContextID context,
                        VASurfaceID render_target) {
  if (LoadVALibrary()) {
    return vaBeginPictureFn(dpy, context, render_target);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaEndPicture(VADisplay dpy, VAContextID context) {
  if (LoadVALibrary()) {
    return vaEndPictureFn(dpy, context);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaRenderPicture(VADisplay dpy, VAContextID context,
                         VABufferID* buffers, int num_buffers) {
  if (LoadVALibrary()) {
    return vaRenderPictureFn(dpy, context, buffers, num_buffers);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

int vaMaxNumProfiles(VADisplay dpy) {
  if (LoadVALibrary()) {
    return vaMaxNumProfilesFn(dpy);
  }
  return 0;
}

VAStatus vaCreateContext(VADisplay dpy, VAConfigID config_id, int picture_width,
                         int picture_height, int flag,
                         VASurfaceID* render_targets, int num_render_targets,
                         VAContextID* context /* out */) {
  if (LoadVALibrary()) {
    return vaCreateContextFn(dpy, config_id, picture_width, picture_height,
                             flag, render_targets, num_render_targets, context);
  }
  *context = 0;
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaDestroyContext(VADisplay dpy, VAContextID context) {
  if (LoadVALibrary()) {
    return vaDestroyContextFn(dpy, context);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaCreateBuffer(VADisplay dpy, VAContextID context,
                        VABufferType type,         /* in */
                        unsigned int size,         /* in */
                        unsigned int num_elements, /* in */
                        void* data,                /* in */
                        VABufferID* buf_id /* out */) {
  if (LoadVALibrary()) {
    return vaCreateBufferFn(dpy, context, type, size, num_elements, data,
                            buf_id);
  }
  *buf_id = 0;
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaQuerySurfaceAttributes(VADisplay dpy, VAConfigID config,
                                  VASurfaceAttrib* attrib_list,
                                  unsigned int* num_attribs) {
  if (LoadVALibrary()) {
    return vaQuerySurfaceAttributesFn(dpy, config, attrib_list, num_attribs);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaQueryConfigProfiles(VADisplay dpy, VAProfile* profile_list, /* out */
                               int* num_profiles /* out */) {
  if (LoadVALibrary()) {
    return vaQueryConfigProfilesFn(dpy, profile_list, num_profiles);
  }
  *num_profiles = 0;
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

const char* vaErrorStr(VAStatus error_status) {
  if (LoadVALibrary()) {
    return vaErrorStrFn(error_status);
  }
  static char tmp[] = "Unimplemented";
  return tmp;
}

VAStatus vaCreateConfig(VADisplay dpy, VAProfile profile,
                        VAEntrypoint entrypoint, VAConfigAttrib* attrib_list,
                        int num_attribs, VAConfigID* config_id /* out */) {
  if (LoadVALibrary()) {
    return vaCreateConfigFn(dpy, profile, entrypoint, attrib_list, num_attribs,
                            config_id);
  }
  *config_id = 0;
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaDestroyConfig(VADisplay dpy, VAConfigID config_id) {
  if (LoadVALibrary()) {
    return vaDestroyConfigFn(dpy, config_id);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

int vaMaxNumImageFormats(VADisplay dpy) {
  if (LoadVALibrary()) {
    return vaMaxNumImageFormatsFn(dpy);
  }
  return 0;
}

VAStatus vaQueryImageFormats(VADisplay dpy, VAImageFormat* format_list,
                             int* num_formats) {
  if (LoadVALibrary()) {
    return vaQueryImageFormatsFn(dpy, format_list, num_formats);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

const char* vaQueryVendorString(VADisplay dpy) {
  if (LoadVALibrary()) {
    return vaQueryVendorStringFn(dpy);
  }
  return NULL;
}

VAStatus vaDestroySurfaces(VADisplay dpy, VASurfaceID* surfaces,
                           int num_surfaces) {
  if (LoadVALibrary()) {
    return vaDestroySurfacesFn(dpy, surfaces, num_surfaces);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaCreateSurfaces(VADisplay dpy, unsigned int format,
                          unsigned int width, unsigned int height,
                          VASurfaceID* surfaces, unsigned int num_surfaces,
                          VASurfaceAttrib* attrib_list,
                          unsigned int num_attribs) {
  if (LoadVALibrary()) {
    return vaCreateSurfacesFn(dpy, format, width, height, surfaces,
                              num_surfaces, attrib_list, num_attribs);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaDeriveImage(VADisplay dpy, VASurfaceID surface,
                       VAImage* image /* out */) {
  if (LoadVALibrary()) {
    return vaDeriveImageFn(dpy, surface, image);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaDestroyImage(VADisplay dpy, VAImageID image) {
  if (LoadVALibrary()) {
    return vaDestroyImageFn(dpy, image);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaPutImage(VADisplay dpy, VASurfaceID surface, VAImageID image,
                    int src_x, int src_y, unsigned int src_width,
                    unsigned int src_height, int dest_x, int dest_y,
                    unsigned int dest_width, unsigned int dest_height) {
  if (LoadVALibrary()) {
    return vaPutImageFn(dpy, surface, image, src_x, src_y, src_width,
                        src_height, dest_x, dest_y, dest_width, dest_height);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaSyncSurface(VADisplay dpy, VASurfaceID render_target) {
  if (LoadVALibrary()) {
    return vaSyncSurfaceFn(dpy, render_target);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaCreateImage(VADisplay dpy, VAImageFormat* format, int width,
                       int height, VAImage* image /* out */) {
  if (LoadVALibrary()) {
    return vaCreateImageFn(dpy, format, width, height, image);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaGetImage(VADisplay dpy, VASurfaceID surface,
                    int x, /* coordinates of the upper left source pixel */
                    int y,
                    unsigned int width, /* width and height of the region */
                    unsigned int height, VAImageID image) {
  if (LoadVALibrary()) {
    return vaGetImageFn(dpy, surface, x, y, width, height, image);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaMapBuffer(VADisplay dpy, VABufferID buf_id, /* in */
                     void** pbuf /* out */) {
  if (LoadVALibrary()) {
    return vaMapBufferFn(dpy, buf_id, pbuf);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaUnmapBuffer(VADisplay dpy, VABufferID buf_id /* in */) {
  if (LoadVALibrary()) {
    return vaUnmapBufferFn(dpy, buf_id);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaTerminate(VADisplay dpy) {
  if (LoadVALibrary()) {
    return vaTerminateFn(dpy);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaInitialize(VADisplay dpy, int* major_version, /* out */
                      int* minor_version /* out */) {
  if (LoadVALibrary()) {
    return vaInitializeFn(dpy, major_version, minor_version);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus vaSetDriverName(VADisplay dpy, char* driver_name) {
  if (LoadVALibrary()) {
    return vaSetDriverNameFn(dpy, driver_name);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

int vaMaxNumEntrypoints(VADisplay dpy) {
  if (LoadVALibrary()) {
    return vaMaxNumEntrypointsFn(dpy);
  }
  return 0;
}

VAStatus vaQueryConfigEntrypoints(VADisplay dpy, VAProfile profile,
                                  VAEntrypoint* entrypoint_list,
                                  int* num_entrypoints) {
  if (LoadVALibrary()) {
    return vaQueryConfigEntrypointsFn(dpy, profile, entrypoint_list,
                                      num_entrypoints);
  }
  return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAMessageCallback vaSetErrorCallback(VADisplay dpy, VAMessageCallback callback,
                                     void* user_context) {
  if (LoadVALibrary()) {
    return vaSetErrorCallbackFn(dpy, callback, user_context);
  }
  return NULL;
}

VAMessageCallback vaSetInfoCallback(VADisplay dpy, VAMessageCallback callback,
                                    void* user_context) {
  if (LoadVALibrary()) {
    return vaSetInfoCallbackFn(dpy, callback, user_context);
  }
  return NULL;
}

#pragma GCC visibility pop

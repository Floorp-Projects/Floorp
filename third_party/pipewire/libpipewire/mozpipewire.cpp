/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <glib.h>
#include "mozilla/Types.h"
#include "prlink.h"

#include <pipewire/pipewire.h>

#define GET_FUNC(func, lib)                                  \
    func##_fn =                                              \
      (decltype(func##_fn))PR_FindFunctionSymbol(lib, #func) \

#define IS_FUNC_LOADED(func)                                          \
    (func != nullptr)                                                 \

struct GUnixFDList;

extern "C" gint
g_unix_fd_list_get(struct GUnixFDList *list,
                   gint index_,
                   GError **error)
{
  static PRLibrary* gioLib = nullptr;
  static bool gioInitialized = false;
  static gint (*g_unix_fd_list_get_fn)(struct GUnixFDList *list,
               gint index_, GError **error) = nullptr;

  if (!gioInitialized) {
    gioInitialized = true;
    gioLib = PR_LoadLibrary("libgio-2.0.so.0");
    if (!gioLib) {
      return -1;
    }
    GET_FUNC(g_unix_fd_list_get, gioLib);
  }

  if (!g_unix_fd_list_get_fn) {
    return -1;
  }

  return g_unix_fd_list_get_fn(list, index_, error);
}

static struct pw_core * (*pw_context_connect_fn)(struct pw_context *context,
                                              struct pw_properties *properties,
                                              size_t user_data_size);
static struct pw_core * (*pw_context_connect_fd_fn)(struct pw_context *context,
                                                    int fd,
                                                    struct pw_properties *properties,
                                                    size_t user_data_size);
static void (*pw_context_destroy_fn)(struct pw_context *context);
struct pw_context * (*pw_context_new_fn)(struct pw_loop *main_loop,
                                      struct pw_properties *props,
                                      size_t user_data_size);
static int (*pw_core_disconnect_fn)(struct pw_core *core);
static void (*pw_init_fn)(int *argc, char **argv[]);
static void (*pw_stream_add_listener_fn)(struct pw_stream *stream,
                                      struct spa_hook *listener,
                                      const struct pw_stream_events *events,
                                      void *data);
static int (*pw_stream_connect_fn)(struct pw_stream *stream,
                                enum pw_direction direction,
                                uint32_t target_id,
                                enum pw_stream_flags flags,
                                const struct spa_pod **params,
                                uint32_t n_params);
static struct pw_buffer* (*pw_stream_dequeue_buffer_fn)(struct pw_stream *stream);
static void (*pw_stream_destroy_fn)(struct pw_stream *stream);
static struct pw_stream* (*pw_stream_new_fn)(struct pw_core *core,
                                          const char *name,
                                          struct pw_properties *props);
static int (*pw_stream_queue_buffer_fn)(struct pw_stream *stream,
                                     struct pw_buffer *buffer);
static int (*pw_stream_update_params_fn)(struct pw_stream *stream,
                                      const struct spa_pod **params,
                                      uint32_t n_params);
static void (*pw_thread_loop_destroy_fn)(struct pw_thread_loop *loop);
static struct pw_loop* (*pw_thread_loop_get_loop_fn)(struct pw_thread_loop *loop);
static struct pw_thread_loop* (*pw_thread_loop_new_fn)(const char *name,
                                                const struct spa_dict *props);
static int (*pw_thread_loop_start_fn)(struct pw_thread_loop *loop);
static void (*pw_thread_loop_stop_fn)(struct pw_thread_loop *loop);

bool IsPwLibraryLoaded() {
  static bool isLoaded =
         (IS_FUNC_LOADED(pw_context_connect_fn) &&
          IS_FUNC_LOADED(pw_context_connect_fd_fn) &&
          IS_FUNC_LOADED(pw_context_destroy_fn) &&
          IS_FUNC_LOADED(pw_context_new_fn) &&
          IS_FUNC_LOADED(pw_core_disconnect_fn) &&
          IS_FUNC_LOADED(pw_init_fn) &&
          IS_FUNC_LOADED(pw_stream_add_listener_fn) &&
          IS_FUNC_LOADED(pw_stream_connect_fn) &&
          IS_FUNC_LOADED(pw_stream_dequeue_buffer_fn) &&
          IS_FUNC_LOADED(pw_stream_destroy_fn) &&
          IS_FUNC_LOADED(pw_stream_new_fn) &&
          IS_FUNC_LOADED(pw_stream_queue_buffer_fn) &&
          IS_FUNC_LOADED(pw_stream_update_params_fn) &&
          IS_FUNC_LOADED(pw_thread_loop_destroy_fn) &&
          IS_FUNC_LOADED(pw_thread_loop_get_loop_fn) &&
          IS_FUNC_LOADED(pw_thread_loop_new_fn) &&
          IS_FUNC_LOADED(pw_thread_loop_start_fn) &&
          IS_FUNC_LOADED(pw_thread_loop_stop_fn));

  return isLoaded;
}

bool LoadPWLibrary() {
  static PRLibrary* pwLib = nullptr;
  static bool pwInitialized = false;

  //TODO Thread safe
  if (!pwInitialized) {
    pwInitialized = true;
    pwLib = PR_LoadLibrary("libpipewire-0.3.so.0");
    if (!pwLib) {
      return false;
    }

    GET_FUNC(pw_context_connect, pwLib);
    GET_FUNC(pw_context_connect_fd, pwLib);
    GET_FUNC(pw_context_destroy, pwLib);
    GET_FUNC(pw_context_new, pwLib);
    GET_FUNC(pw_core_disconnect, pwLib);
    GET_FUNC(pw_init, pwLib);
    GET_FUNC(pw_stream_add_listener, pwLib);
    GET_FUNC(pw_stream_connect, pwLib);
    GET_FUNC(pw_stream_dequeue_buffer, pwLib);
    GET_FUNC(pw_stream_destroy, pwLib);
    GET_FUNC(pw_stream_new, pwLib);
    GET_FUNC(pw_stream_queue_buffer, pwLib);
    GET_FUNC(pw_stream_update_params, pwLib);
    GET_FUNC(pw_thread_loop_destroy, pwLib);
    GET_FUNC(pw_thread_loop_get_loop, pwLib);
    GET_FUNC(pw_thread_loop_new, pwLib);
    GET_FUNC(pw_thread_loop_start, pwLib);
    GET_FUNC(pw_thread_loop_stop, pwLib);
  }

  return IsPwLibraryLoaded();
}

struct pw_core *
pw_context_connect(struct pw_context *context,
                   struct pw_properties *properties,
                   size_t user_data_size)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_context_connect_fn(context, properties, user_data_size);
}

struct pw_core *
pw_context_connect_fd(struct pw_context *context,
                      int fd,
                      struct pw_properties *properties,
                      size_t user_data_size)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_context_connect_fd_fn(context, fd, properties, user_data_size);
}

void
pw_context_destroy(struct pw_context *context)
{
  if (!LoadPWLibrary()) {
    return;
  }
  pw_context_destroy_fn(context);
}

struct pw_context *
pw_context_new(struct pw_loop *main_loop,
               struct pw_properties *props,
               size_t user_data_size)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_context_new_fn(main_loop, props, user_data_size);
}

int
pw_core_disconnect(struct pw_core *core)
{
  if (!LoadPWLibrary()) {
    return 0;
  }
  return pw_core_disconnect_fn(core);
}

void
pw_init(int *argc, char **argv[])
{
  if (!LoadPWLibrary()) {
    return;
  }
  return pw_init_fn(argc, argv);
}

void
pw_stream_add_listener(struct pw_stream *stream,
                       struct spa_hook *listener,
                       const struct pw_stream_events *events,
                       void *data)
{
  if (!LoadPWLibrary()) {
    return;
  }
  return pw_stream_add_listener_fn(stream, listener, events, data);
}

int
pw_stream_connect(struct pw_stream *stream,
                  enum pw_direction direction,
                  uint32_t target_id,
                  enum pw_stream_flags flags,
                  const struct spa_pod **params,
                  uint32_t n_params)
{
  if (!LoadPWLibrary()) {
    return 0;
  }
  return pw_stream_connect_fn(stream, direction, target_id, flags,
                              params, n_params);
}

struct pw_buffer *
pw_stream_dequeue_buffer(struct pw_stream *stream)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_stream_dequeue_buffer_fn(stream);
}

void
pw_stream_destroy(struct pw_stream *stream)
{
  if (!LoadPWLibrary()) {
    return;
  }
  return pw_stream_destroy_fn(stream);
}

struct pw_stream *
pw_stream_new(struct pw_core *core,
              const char *name,
              struct pw_properties *props)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_stream_new_fn(core, name, props);
}

int
pw_stream_queue_buffer(struct pw_stream *stream,
                       struct pw_buffer *buffer)
{
  if (!LoadPWLibrary()) {
    return 0;
  }
  return pw_stream_queue_buffer_fn(stream, buffer);
}

int
pw_stream_update_params(struct pw_stream *stream,
                        const struct spa_pod **params,
                        uint32_t n_params)
{
  if (!LoadPWLibrary()) {
    return 0;
  }
  return pw_stream_update_params_fn(stream, params, n_params);
}

void
pw_thread_loop_destroy(struct pw_thread_loop *loop)
{
  if (!LoadPWLibrary()) {
    return;
  }
  return pw_thread_loop_destroy_fn(loop);
}

struct pw_loop *
pw_thread_loop_get_loop(struct pw_thread_loop *loop)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_thread_loop_get_loop_fn(loop);
}

struct pw_thread_loop *
pw_thread_loop_new(const char *name,
                   const struct spa_dict *props)
{
  if (!LoadPWLibrary()) {
    return nullptr;
  }
  return pw_thread_loop_new_fn(name, props);
}

int
pw_thread_loop_start(struct pw_thread_loop *loop)
{
  if (!LoadPWLibrary()) {
    return 0;
  }
  return pw_thread_loop_start_fn(loop);
}

void
pw_thread_loop_stop(struct pw_thread_loop *loop)
{
  if (!LoadPWLibrary()) {
    return;
  }
  return pw_thread_loop_stop_fn(loop);
}

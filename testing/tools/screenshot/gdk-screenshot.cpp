/*
 * Copyright (c) 2009, The Mozilla Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Mozilla Foundation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY The Mozilla Foundation ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL The Mozilla Foundation BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contributors:
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
 *   Karl Tomlinson <karlt+@karlt.net>
 */
/*
 * gdk-screenshot.cpp: Save a screenshot of the root window in .png format.
 *  If a filename is specified as the first argument on the commandline,
 *  then the image will be saved to that filename. Otherwise, the image will
 *  be written to stdout.
 */
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#ifdef HAVE_LIBXSS
#include <X11/extensions/scrnsaver.h>
#endif

#include <errno.h>
#include <stdio.h>

gboolean save_to_stdout(const gchar *buf, gsize count,
                        GError **error, gpointer data)
{
  size_t written = fwrite(buf, 1, count, stdout);
  if (written != count) {
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Write to stdout failed: %s", g_strerror(errno));
    return FALSE;
  }

  return TRUE;
}

int main(int argc, char** argv)
{
  gdk_init(&argc, &argv);

#if defined(HAVE_LIBXSS) && defined(MOZ_WIDGET_GTK)
  int event_base, error_base;
  Bool have_xscreensaver =
    XScreenSaverQueryExtension(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                               &event_base, &error_base);

  if (!have_xscreensaver) {
    fprintf(stderr, "No XScreenSaver extension on display\n");
  } else {
    XScreenSaverInfo* info = XScreenSaverAllocInfo();
    if (!info) {
      fprintf(stderr, "%s: Out of memory\n", argv[0]);
      return 1;
    }
    XScreenSaverQueryInfo(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                          GDK_ROOT_WINDOW(), info);

    const char* state;
    const char* til_or_since = nullptr;
    switch (info->state) {
    case ScreenSaverOff:
      state = "Off";
      til_or_since = "XScreenSaver will activate after another %lu seconds idle time\n";
      break;
    case ScreenSaverOn:
      state = "On";
      if (info->til_or_since) {
        til_or_since = "XScreenSaver idle timer activated %lu seconds ago\n";
      } else {
        til_or_since = "XScreenSaver idle activation is disabled\n";
      }
      break;
    case ScreenSaverDisabled:
      state = "Disabled";
      break;
    default:
      state = "unknown";
    }

    const char* kind;
    switch (info->kind) {
    case ScreenSaverBlanked:
      kind = "Blanked";
      break;
    case ScreenSaverInternal:
      state = "Internal";
      break;
    case ScreenSaverExternal:
      state = "External";
      break;
    default:
      state = "unknown";
    }

    fprintf(stderr, "XScreenSaver state: %s\n", state);

    if (til_or_since) {
      fprintf(stderr, "XScreenSaver kind: %s\n", kind);
      fprintf(stderr, til_or_since, info->til_or_since / 1000);
    }

    fprintf(stderr, "User input has been idle for %lu seconds\n", info->idle / 1000);

    XFree(info);
  }
#endif

  GdkPixbuf* screenshot = nullptr;
  GdkWindow* window = gdk_get_default_root_window();
  screenshot = gdk_pixbuf_get_from_window(window, 0, 0,
                                          gdk_window_get_width(window),
                                          gdk_window_get_height(window));
  if (!screenshot) {
    fprintf(stderr, "%s: failed to create screenshot GdkPixbuf\n", argv[0]);
    return 1;
  }

  GError* error = nullptr;
  if (argc > 1) {
    gdk_pixbuf_save(screenshot, argv[1], "png", &error, nullptr);
  } else {
    gdk_pixbuf_save_to_callback(screenshot, save_to_stdout, nullptr,
                                "png", &error, nullptr);
  }
  if (error) {
    fprintf(stderr, "%s: failed to write screenshot as png: %s\n",
            argv[0], error->message);
    return error->code;
  }

  return 0;
}

// These options are copied from mozglue/build/AsanOptions.cpp
#ifdef MOZ_ASAN
extern "C"
const char* __asan_default_options() {
  return "allow_user_segv_handler=1:alloc_dealloc_mismatch=0:detect_leaks=0";
}
#endif

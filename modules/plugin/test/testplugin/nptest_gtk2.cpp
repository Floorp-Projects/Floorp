/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Michael Ventnor <mventnor@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_platform.h"
#include "npapi.h"
#include <gdk/gdk.h>
#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif

void pluginDrawSolid(InstanceData* instanceData, GdkDrawable* gdkWindow);

NPError
pluginInstanceInit(InstanceData* instanceData)
{
#ifdef MOZ_X11
  return NPERR_NO_ERROR;
#else
  // we only support X11 here, since thats what the plugin system uses
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
#ifdef MOZ_X11
  XEvent *nsEvent = (XEvent *)event;
  gboolean handled = 0;

  if (nsEvent->type != GraphicsExpose)
    return 0;

  XGraphicsExposeEvent *expose = &nsEvent->xgraphicsexpose;
  instanceData->window.window = (void*)(expose->drawable);

  pluginDraw(instanceData);
#endif
  return 0;
}

static void 
SetCairoRGBA(cairo_t* cairoWindow, PRUint32 rgba)
{
  float b = (rgba & 0xFF) / 255.0;
  float g = ((rgba & 0xFF00) >> 8) / 255.0;
  float r = ((rgba & 0xFF0000) >> 16) / 255.0;
  float a = ((rgba & 0xFF000000) >> 24) / 255.0;

  cairo_set_source_rgba(cairoWindow, r, g, b, a);
}

void
pluginDrawSolid(InstanceData* instanceData, GdkDrawable* gdkWindow)
{
#ifdef MOZ_X11
  cairo_t* cairoWindow = gdk_cairo_create(gdkWindow);

  NPWindow window = instanceData->window;
  GdkRectangle windowRect;
  windowRect.x = window.x;
  windowRect.y = window.y;
  windowRect.width = window.width;
  windowRect.height = window.height;

  gdk_cairo_rectangle(cairoWindow, &windowRect);
  SetCairoRGBA(cairoWindow, instanceData->scriptableObject->drawColor);

  cairo_fill(cairoWindow);
  cairo_destroy(cairoWindow);
  g_object_unref(gdkWindow);
#endif
}

void
pluginDraw(InstanceData* instanceData)
{
#ifdef MOZ_X11
  if (!instanceData)
    return;

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  NPWindow window = instanceData->window;
  GdkNativeWindow nativeWinId = reinterpret_cast<XID>(window.window);
  GdkDrawable* gdkWindow = GDK_DRAWABLE(gdk_window_foreign_new(nativeWinId));

  if (instanceData->scriptableObject->drawMode == DM_SOLID_COLOR) {
    // drawing a solid color for reftests
    pluginDrawSolid(instanceData, gdkWindow);
    return;
  }

  GdkGC* gdkContext = gdk_gc_new(gdkWindow);
  PangoContext* pangoContext = gdk_pango_context_get();
  PangoLayout* pangoTextLayout = pango_layout_new(pangoContext);

  // draw a grey background for the plugin frame
  GdkColor grey;
  grey.red = grey.blue = grey.green = 32767;
  gdk_gc_set_rgb_fg_color(gdkContext, &grey);
  gdk_draw_rectangle(gdkWindow, gdkContext, TRUE, window.x, window.y,
                     window.width, window.height);

  // draw a 3-pixel-thick black frame around the plugin
  GdkColor black;
  black.red = black.green = black.blue = 0;
  gdk_gc_set_rgb_fg_color(gdkContext, &black);
  gdk_gc_set_line_attributes(gdkContext, 3, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
  gdk_draw_rectangle(gdkWindow, gdkContext, FALSE, window.x + 1, window.y + 1,
                     window.width - 3, window.height - 3);

  // paint the UA string
  pango_layout_set_width(pangoTextLayout, (window.width - 10) * PANGO_SCALE);
  pango_layout_set_text(pangoTextLayout, uaString, -1);
  gdk_draw_layout(gdkWindow, gdkContext, window.x + 5, window.y + 5, pangoTextLayout);

  g_object_unref(pangoTextLayout);
  g_object_unref(gdkContext);
  g_object_unref(gdkWindow);

#endif
}

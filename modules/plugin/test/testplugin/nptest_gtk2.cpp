/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include <gtk/gtk.h>

/**
 * XXX In various places in this file we use GDK APIs to inspect the
 * window ancestors of the plugin. These APIs will not work properly if
 * this plugin is used in a browser that does not use GDK for all its
 * widgets. They would also fail for out-of-process plugins. These should
 * be fixed to use raw X APIs instead.
 */

struct _PlatformData {
  Display* display;
  GtkWidget* plug;
};

bool
pluginSupportsWindowMode()
{
  return true;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
#ifdef MOZ_X11
  instanceData->platformData = static_cast<PlatformData*>
    (NPN_MemAlloc(sizeof(PlatformData)));
  if (!instanceData->platformData)
    return NPERR_OUT_OF_MEMORY_ERROR;

  instanceData->platformData->display = 0;
  instanceData->platformData->plug = 0;

  return NPERR_NO_ERROR;
#else
  // we only support X11 here, since thats what the plugin system uses
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
#endif
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
  if (instanceData->hasWidget) {
    Window window = reinterpret_cast<XID>(instanceData->window.window);

    if (window != None) {
      // This window XID should still be valid.
      // See bug 429604 and bug 454756.
      XWindowAttributes attributes;
      if (!XGetWindowAttributes(instanceData->platformData->display, window,
                                &attributes))
        g_error("XGetWindowAttributes failed at plugin instance shutdown");
    }
  }

  GtkWidget* plug = instanceData->platformData->plug;
  if (plug) {
    instanceData->platformData->plug = 0;
    gtk_widget_destroy(plug);
  }

  NPN_MemFree(instanceData->platformData);
  instanceData->platformData = 0;
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

static void
pluginDrawSolid(InstanceData* instanceData, GdkDrawable* gdkWindow,
                int x, int y, int width, int height)
{
  cairo_t* cairoWindow = gdk_cairo_create(gdkWindow);

  if (!instanceData->hasWidget) {
    NPRect* clip = &instanceData->window.clipRect;
    cairo_rectangle(cairoWindow, clip->left, clip->top,
                    clip->right - clip->left, clip->bottom - clip->top);
    cairo_clip(cairoWindow);
  }

  GdkRectangle windowRect = { x, y, width, height };
  gdk_cairo_rectangle(cairoWindow, &windowRect);
  SetCairoRGBA(cairoWindow, instanceData->scriptableObject->drawColor);

  cairo_fill(cairoWindow);
  cairo_destroy(cairoWindow);
}

static void
pluginDrawWindow(InstanceData* instanceData, GdkDrawable* gdkWindow)
{
  NPWindow& window = instanceData->window;
  // When we have a widget, window.x/y are meaningless since our
  // widget is always positioned correctly and we just draw into it at 0,0
  int x = instanceData->hasWidget ? 0 : window.x;
  int y = instanceData->hasWidget ? 0 : window.y;
  int width = window.width;
  int height = window.height;

  if (instanceData->scriptableObject->drawMode == DM_SOLID_COLOR) {
    // drawing a solid color for reftests
    pluginDrawSolid(instanceData, gdkWindow, x, y, width, height);
    return;
  }

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  const char* uaString = NPN_UserAgent(npp);
  if (!uaString)
    return;

  GdkGC* gdkContext = gdk_gc_new(gdkWindow);
  if (!gdkContext)
    return;

  if (!instanceData->hasWidget) {
    NPRect* clip = &window.clipRect;
    GdkRectangle gdkClip = { clip->left, clip->top, clip->right - clip->left,
                             clip->bottom - clip->top };
    gdk_gc_set_clip_rectangle(gdkContext, &gdkClip);
  }

  // draw a grey background for the plugin frame
  GdkColor grey;
  grey.red = grey.blue = grey.green = 32767;
  gdk_gc_set_rgb_fg_color(gdkContext, &grey);
  gdk_draw_rectangle(gdkWindow, gdkContext, TRUE, x, y, width, height);

  // draw a 3-pixel-thick black frame around the plugin
  GdkColor black;
  black.red = black.green = black.blue = 0;
  gdk_gc_set_rgb_fg_color(gdkContext, &black);
  gdk_gc_set_line_attributes(gdkContext, 3, GDK_LINE_SOLID, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);
  gdk_draw_rectangle(gdkWindow, gdkContext, FALSE, x + 1, y + 1,
                     width - 3, height - 3);

  // paint the UA string
  PangoContext* pangoContext = gdk_pango_context_get();
  PangoLayout* pangoTextLayout = pango_layout_new(pangoContext);
  pango_layout_set_width(pangoTextLayout, (width - 10) * PANGO_SCALE);
  pango_layout_set_text(pangoTextLayout, uaString, -1);
  gdk_draw_layout(gdkWindow, gdkContext, x + 5, y + 5, pangoTextLayout);
  g_object_unref(pangoTextLayout);

  g_object_unref(gdkContext);
}

static gboolean
ExposeWidget(GtkWidget* widget, GdkEventExpose* event,
             gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  pluginDrawWindow(instanceData, event->window);
  return TRUE;
}

static gboolean
DeleteWidget(GtkWidget* widget, GdkEvent* event, gpointer user_data)
{
  InstanceData* instanceData = static_cast<InstanceData*>(user_data);
  // Some plugins do not expect the plug to be removed from the socket before
  // the plugin instance is destroyed.  e.g. bug 485125
  if (instanceData->platformData->plug)
    g_error("plug removed"); // this aborts

  return FALSE;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;

  NPSetWindowCallbackStruct *ws_info =
    static_cast<NPSetWindowCallbackStruct*>(newWindow->ws_info);
  instanceData->platformData->display = ws_info->display;
}

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
#ifdef MOZ_X11
  GtkWidget* oldPlug = instanceData->platformData->plug;
  if (oldPlug) {
    instanceData->platformData->plug = 0;
    gtk_widget_destroy(oldPlug);
  }

  GdkNativeWindow nativeWinId =
    reinterpret_cast<XID>(instanceData->window.window);

  /* create a GtkPlug container */
  GtkWidget* plug = gtk_plug_new(nativeWinId);

  /* make sure the widget is capable of receiving focus */
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(plug), GTK_CAN_FOCUS);

  /* all the events that our widget wants to receive */
  gtk_widget_add_events(plug, GDK_EXPOSURE_MASK);
  g_signal_connect(G_OBJECT(plug), "expose-event", G_CALLBACK(ExposeWidget),
                   instanceData);
  g_signal_connect(G_OBJECT(plug), "delete-event", G_CALLBACK(DeleteWidget),
                   instanceData);
  gtk_widget_show(plug);

  instanceData->platformData->plug = plug;
#endif
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
#ifdef MOZ_X11
  XEvent *nsEvent = (XEvent *)event;

  if (nsEvent->type != GraphicsExpose)
    return 0;

  XGraphicsExposeEvent *expose = &nsEvent->xgraphicsexpose;
  instanceData->window.window = (void*)(expose->drawable);

  GdkNativeWindow nativeWinId =
    reinterpret_cast<XID>(instanceData->window.window);
  GdkDrawable* gdkWindow = GDK_DRAWABLE(gdk_window_foreign_new(nativeWinId));  
  pluginDrawWindow(instanceData, gdkWindow);
  g_object_unref(gdkWindow);
#endif
  return 0;
}

int32_t pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  if (!instanceData->hasWidget)
    return NPTEST_INT32_ERROR;
  GtkWidget* plug = instanceData->platformData->plug;
  if (!plug)
    return NPTEST_INT32_ERROR;
  GdkWindow* plugWnd = plug->window;
  if (!plugWnd)
    return NPTEST_INT32_ERROR;
  // XXX This only works because Gecko uses GdkWindows everywhere!
  GdkWindow* toplevelWnd = gdk_window_get_toplevel(plugWnd);
  if (!toplevelWnd)
    return NPTEST_INT32_ERROR;

  gint plugScreenX, plugScreenY;
  gdk_window_get_origin(plugWnd, &plugScreenX, &plugScreenY);
  gint toplevelFrameX, toplevelFrameY;
  gdk_window_get_root_origin(toplevelWnd, &toplevelFrameX, &toplevelFrameY);
  gint width, height;
  gdk_drawable_get_size(GDK_DRAWABLE(plugWnd), &width, &height);

  switch (edge) {
  case EDGE_LEFT:
    return plugScreenX - toplevelFrameX;
  case EDGE_TOP:
    return plugScreenY - toplevelFrameY;
  case EDGE_RIGHT:
    return plugScreenX + width - toplevelFrameX;
  case EDGE_BOTTOM:
    return plugScreenY + height - toplevelFrameY;
  }
  return NPTEST_INT32_ERROR;
}

int32_t pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  if (!instanceData->hasWidget)
    return NPTEST_INT32_ERROR;
  // XXX later we'll want to support XShape and be able to return a
  // complex region here
  return 1;
}

int32_t pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  if (!instanceData->hasWidget)
    return NPTEST_INT32_ERROR;

  GtkWidget* plug = instanceData->platformData->plug;
  if (!plug)
    return NPTEST_INT32_ERROR;
  GdkWindow* plugWnd = plug->window;
  if (!plugWnd)
    return NPTEST_INT32_ERROR;
  // XXX This only works because Gecko uses GdkWindows everywhere!
  GdkWindow* toplevelWnd = gdk_window_get_toplevel(plugWnd);
  if (!toplevelWnd)
    return NPTEST_INT32_ERROR;

  gint width, height;
  gdk_drawable_get_size(GDK_DRAWABLE(plugWnd), &width, &height);

  GdkRectangle rect = { 0, 0, width, height };
  GdkWindow* wnd = plugWnd;
  while (wnd != toplevelWnd) {
    gint x, y;
    gdk_window_get_position(wnd, &x, &y);
    rect.x += x;
    rect.y += y;

    // XXX This only works because Gecko uses GdkWindows everywhere!
    GdkWindow* parent = gdk_window_get_parent(wnd);
    gint parentWidth, parentHeight;
    gdk_drawable_get_size(GDK_DRAWABLE(parent), &parentWidth, &parentHeight);
    GdkRectangle parentRect = { 0, 0, parentWidth, parentHeight };
    gdk_rectangle_intersect(&rect, &parentRect, &rect);
    wnd = parent;
  }

  gint toplevelFrameX, toplevelFrameY;
  gdk_window_get_root_origin(toplevelWnd, &toplevelFrameX, &toplevelFrameY);
  gint toplevelOriginX, toplevelOriginY;
  gdk_window_get_origin(toplevelWnd, &toplevelOriginX, &toplevelOriginY);

  rect.x += toplevelOriginX - toplevelFrameX;
  rect.y += toplevelOriginY - toplevelFrameY;

  switch (edge) {
  case EDGE_LEFT:
    return rect.x;
  case EDGE_TOP:
    return rect.y;
  case EDGE_RIGHT:
    return rect.x + rect.width;
  case EDGE_BOTTOM:
    return rect.y + rect.height;
  }
  return NPTEST_INT32_ERROR;
}

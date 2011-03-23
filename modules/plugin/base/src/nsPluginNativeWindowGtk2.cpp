/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
*/
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robin Lu <robin.lu@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 *  This file is the Gtk2 implementation of plugin native window.
 */

#include "nsDebug.h"
#include "nsPluginNativeWindow.h"
#include "npapi.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#if (MOZ_PLATFORM_MAEMO == 5)
#define MOZ_COMPOSITED_PLUGINS
#endif

#ifdef MOZ_COMPOSITED_PLUGINS
extern "C" {
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
}
#endif

#include "gtk2xtbin.h"

class nsPluginNativeWindowGtk2 : public nsPluginNativeWindow {
public: 
  nsPluginNativeWindowGtk2();
  virtual ~nsPluginNativeWindowGtk2();

  virtual nsresult CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance);
private:
  NPSetWindowCallbackStruct mWsInfo;
  /**
   * Either a GtkSocket or a special GtkXtBin widget (derived from GtkSocket)
   * that encapsulates the Xt toolkit within a Gtk Application.
   */
  GtkWidget* mSocketWidget;
  nsresult  CreateXEmbedWindow();
  nsresult  CreateXtWindow();
  void      SetAllocation();
#ifdef MOZ_COMPOSITED_PLUGINS
  nsresult  CreateXCompositedWindow();
  static GdkFilterReturn    plugin_composite_filter_func (GdkXEvent *xevent,
    GdkEvent *event,
    gpointer data);

  Damage     mDamage;
  GtkWidget* mParentWindow;
#endif
};

static gboolean plug_removed_cb   (GtkWidget *widget, gpointer data);
static void socket_unrealize_cb   (GtkWidget *widget, gpointer data);

nsPluginNativeWindowGtk2::nsPluginNativeWindowGtk2() : nsPluginNativeWindow()
{
  // initialize the struct fields
  window = nsnull; 
  x = 0; 
  y = 0; 
  width = 0; 
  height = 0; 
  memset(&clipRect, 0, sizeof(clipRect));
  ws_info = &mWsInfo;
  type = NPWindowTypeWindow;
  mSocketWidget = 0;
  mWsInfo.type = 0;
  mWsInfo.display = nsnull;
  mWsInfo.visual = nsnull;
  mWsInfo.colormap = 0;
  mWsInfo.depth = 0;
#ifdef MOZ_COMPOSITED_PLUGINS
  mDamage = 0;
  mParentWindow = 0;
#endif
}

nsPluginNativeWindowGtk2::~nsPluginNativeWindowGtk2() 
{
  if(mSocketWidget) {
    gtk_widget_destroy(mSocketWidget);
  }

#ifdef MOZ_COMPOSITED_PLUGINS
  if (mParentWindow) {
    gtk_widget_destroy(mParentWindow);
  }

  if (mDamage) {
    gdk_window_remove_filter (nsnull, plugin_composite_filter_func, this);
  }
#endif
}

nsresult PLUG_NewPluginNativeWindow(nsPluginNativeWindow ** aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  *aPluginNativeWindow = new nsPluginNativeWindowGtk2();
  return *aPluginNativeWindow ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult PLUG_DeletePluginNativeWindow(nsPluginNativeWindow * aPluginNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aPluginNativeWindow);
  nsPluginNativeWindowGtk2 *p = (nsPluginNativeWindowGtk2 *)aPluginNativeWindow;
  delete p;
  return NS_OK;
}

#ifdef MOZ_COMPOSITED_PLUGINS
/* the base xdamage event number.*/
static int xdamage_event_base;

GdkFilterReturn
nsPluginNativeWindowGtk2::plugin_composite_filter_func (GdkXEvent *xevent,
    GdkEvent *event,
    gpointer data)
{
  nsPluginNativeWindowGtk2 *native_window = (nsPluginNativeWindowGtk2*)data;
  XDamageNotifyEvent *ev;
  ev = (XDamageNotifyEvent *) xevent;
  if (ev->type != xdamage_event_base + XDamageNotify)
    return GDK_FILTER_CONTINUE;

  //printf("Damage event %d %d %d %d\n",ev->area.x, ev->area.y, ev->area.width, ev->area.height);
  XDamageSubtract (GDK_DISPLAY(), native_window->mDamage, None, None);

  /* We try to do our area invalidation here */
  NPRect rect;
  rect.top = ev->area.x;
  rect.left = ev->area.y;
  rect.right = ev->area.x + ev->area.width;
  rect.bottom = ev->area.y + ev->area.height;

  if (native_window->mPluginInstance)
    native_window->mPluginInstance->InvalidateRect(&rect);

  return GDK_FILTER_REMOVE;
}
#endif

nsresult nsPluginNativeWindowGtk2::CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
{
  if (aPluginInstance) {
    if (type == NPWindowTypeWindow) {
      if (!mSocketWidget) {
        nsresult rv;

        PRBool needXEmbed = PR_FALSE;
        rv = aPluginInstance->GetValueFromPlugin(NPPVpluginNeedsXEmbed, &needXEmbed);
        // If the call returned an error code make sure we still use our default value.
        if (NS_FAILED(rv)) {
          needXEmbed = PR_FALSE;
        }
#ifdef DEBUG
        printf("nsPluginNativeWindowGtk2: NPPVpluginNeedsXEmbed=%d\n", needXEmbed);
#endif

        if (needXEmbed) {
#ifdef MOZ_COMPOSITED_PLUGINS
          rv = CreateXCompositedWindow();
#else
          rv = CreateXEmbedWindow();
#endif
        }
        else {
          rv = CreateXtWindow();
        }

        if (NS_FAILED(rv)) {
          return NS_ERROR_FAILURE;
        }
      }

      if (!mSocketWidget) {
        return NS_ERROR_FAILURE;
      }

      // Make sure to resize and re-place the window if required.
      // Need to reset "window" each time as nsObjectFrame::DidReflow sets it
      // to the ancestor window.
      if (GTK_IS_XTBIN(mSocketWidget)) {
        gtk_xtbin_resize(mSocketWidget, width, height);
        // Point the NPWindow structures window to the actual X window
        window = (void*)GTK_XTBIN(mSocketWidget)->xtwindow;
      }
      else { // XEmbed
        SetAllocation();
        window = (void*)gtk_socket_get_id(GTK_SOCKET(mSocketWidget));
      }
#ifdef DEBUG
      printf("nsPluginNativeWindowGtk2: call SetWindow with xid=%p\n", (void *)window);
#endif
    } // NPWindowTypeWindow
    aPluginInstance->SetWindow(this);
  }
  else if (mPluginInstance)
    mPluginInstance->SetWindow(nsnull);

  SetPluginInstance(aPluginInstance);
  return NS_OK;
}

nsresult nsPluginNativeWindowGtk2::CreateXEmbedWindow() {
  NS_ASSERTION(!mSocketWidget,"Already created a socket widget!");

  GdkWindow *parent_win = gdk_window_lookup((XID)window);
  mSocketWidget = gtk_socket_new();

  //attach the socket to the container widget
  gtk_widget_set_parent_window(mSocketWidget, parent_win);

  // Make sure to handle the plug_removed signal.  If we don't the
  // socket will automatically be destroyed when the plug is
  // removed, which means we're destroying it more than once.
  // SYNTAX ERROR.
  g_signal_connect(mSocketWidget, "plug_removed",
                   G_CALLBACK(plug_removed_cb), NULL);

  g_signal_connect(mSocketWidget, "unrealize",
                   G_CALLBACK(socket_unrealize_cb), NULL);

  g_signal_connect(mSocketWidget, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &mSocketWidget);

  gpointer user_data = NULL;
  gdk_window_get_user_data(parent_win, &user_data);

  GtkContainer *container = GTK_CONTAINER(user_data);
  gtk_container_add(container, mSocketWidget);
  gtk_widget_realize(mSocketWidget);

  // The GtkSocket has a visible window, but the plugin's XEmbed plug will
  // cover this window.  Normally GtkSockets let the X server paint their
  // background and this would happen immediately (before the plug is
  // created).  Setting the background to None prevents the server from
  // painting this window, avoiding flicker.
  gdk_window_set_back_pixmap(mSocketWidget->window, NULL, FALSE);

  // Resize before we show
  SetAllocation();

  gtk_widget_show(mSocketWidget);

  gdk_flush();
  window = (void*)gtk_socket_get_id(GTK_SOCKET(mSocketWidget));

  // Fill out the ws_info structure.
  // (The windowless case is done in nsObjectFrame.cpp.)
  GdkWindow *gdkWindow = gdk_window_lookup((XID)window);
  if(!gdkWindow)
    return NS_ERROR_FAILURE;

  mWsInfo.display = GDK_WINDOW_XDISPLAY(gdkWindow);
  mWsInfo.colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(gdkWindow));
  GdkVisual* gdkVisual = gdk_drawable_get_visual(gdkWindow);
  mWsInfo.visual = GDK_VISUAL_XVISUAL(gdkVisual);
  mWsInfo.depth = gdkVisual->depth;

  return NS_OK;
}

#ifdef MOZ_COMPOSITED_PLUGINS
#include <dlfcn.h>
nsresult nsPluginNativeWindowGtk2::CreateXCompositedWindow() {
  NS_ASSERTION(!mSocketWidget,"Already created a socket widget!");

  mParentWindow = gtk_window_new(GTK_WINDOW_POPUP);
  mSocketWidget = gtk_socket_new();
  GdkWindow *parent_win = mParentWindow->window;

  //attach the socket to the container widget
  gtk_widget_set_parent_window(mSocketWidget, parent_win);

  // Make sure to handle the plug_removed signal.  If we don't the
  // socket will automatically be destroyed when the plug is
  // removed, which means we're destroying it more than once.
  // SYNTAX ERROR.
  g_signal_connect(mSocketWidget, "plug_removed",
                   G_CALLBACK(plug_removed_cb), NULL);

  g_signal_connect(mSocketWidget, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &mSocketWidget);

  /*gpointer user_data = NULL;
  gdk_window_get_user_data(parent_win, &user_data);
  */
  GtkContainer *container = GTK_CONTAINER(mParentWindow);
  gtk_container_add(container, mSocketWidget);
  gtk_widget_realize(mSocketWidget);

  // Resize before we show
  SetAllocation();
  gtk_widget_set_size_request (mSocketWidget, width, height);
  /* move offscreen */
  gtk_window_move (GTK_WINDOW(mParentWindow), width+1000, height+1000);


  gtk_widget_show(mSocketWidget);
  gtk_widget_show_all(mParentWindow);

  /* store away a reference to the socketwidget */
  mPlugWindow = (mSocketWidget);

  gdk_flush();
  window = (void*)gtk_socket_get_id(GTK_SOCKET(mSocketWidget));

  /* This is useful if we still have the plugin window inline
   * i.e. firefox vs. fennec */
  // gdk_window_set_composited(mSocketWidget->window, TRUE);

  if (!mDamage) {
    /* we install a general handler instead of one specific to a particular window
     * because we don't have a GdkWindow for the plugin window */
    gdk_window_add_filter (parent_win, plugin_composite_filter_func, this);

    int junk;
    if (!XDamageQueryExtension (GDK_DISPLAY (), &xdamage_event_base, &junk))
      printf ("This requires the XDamage extension");

    mDamage = XDamageCreate(GDK_DISPLAY(), (Drawable)window, XDamageReportNonEmpty);
    XCompositeRedirectWindow (GDK_DISPLAY(),
        (Drawable)window,
        CompositeRedirectManual);

    /* this is a hack to avoid having flash causing a crash when it is unloaded.
     * libplayback sets up dbus_connection_filters. When flash is unloaded it takes
     * libplayback with it, however the connection filters are not removed
     * which causes a crash when dbus tries to execute them. dlopening libplayback
     * ensures that those functions stay around even after flash is gone. */
    static void *libplayback_handle;
    if (!libplayback_handle) {
      libplayback_handle = dlopen("libplayback-1.so.0", RTLD_NOW);
    }

  }

  // Fill out the ws_info structure.
  // (The windowless case is done in nsObjectFrame.cpp.)
  GdkWindow *gdkWindow = gdk_window_lookup((XID)window);
  mWsInfo.display = GDK_WINDOW_XDISPLAY(gdkWindow);
  mWsInfo.colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(gdkWindow));
  GdkVisual* gdkVisual = gdk_drawable_get_visual(gdkWindow);
  mWsInfo.visual = GDK_VISUAL_XVISUAL(gdkVisual);
  mWsInfo.depth = gdkVisual->depth;

  return NS_OK;
}
#endif

void nsPluginNativeWindowGtk2::SetAllocation() {
  if (!mSocketWidget)
    return;

  GtkAllocation new_allocation;
  new_allocation.x = 0;
  new_allocation.y = 0;
  new_allocation.width = width;
  new_allocation.height = height;
  gtk_widget_size_allocate(mSocketWidget, &new_allocation);
}

nsresult nsPluginNativeWindowGtk2::CreateXtWindow() {
  NS_ASSERTION(!mSocketWidget,"Already created a socket widget!");

#ifdef NS_DEBUG      
  printf("About to create new xtbin of %i X %i from %p...\n",
         width, height, (void*)window);
#endif
  GdkWindow *gdkWindow = gdk_window_lookup((XID)window);
  mSocketWidget = gtk_xtbin_new(gdkWindow, 0);
  // Check to see if creating the xtbin failed for some reason.
  // if it did, we can't go any further.
  if (!mSocketWidget)
    return NS_ERROR_FAILURE;

  g_signal_connect(mSocketWidget, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &mSocketWidget);

  gtk_widget_set_size_request(mSocketWidget, width, height);

#ifdef NS_DEBUG
  printf("About to show xtbin(%p)...\n", (void*)mSocketWidget); fflush(NULL);
#endif
  gtk_widget_show(mSocketWidget);
#ifdef NS_DEBUG
  printf("completed gtk_widget_show(%p)\n", (void*)mSocketWidget); fflush(NULL);
#endif

  // Fill out the ws_info structure.
  GtkXtBin* xtbin = GTK_XTBIN(mSocketWidget);
  // The xtbin has its own Display structure.
  mWsInfo.display = xtbin->xtdisplay;
  mWsInfo.colormap = xtbin->xtclient.xtcolormap;
  mWsInfo.visual = xtbin->xtclient.xtvisual;
  mWsInfo.depth = xtbin->xtclient.xtdepth;
  // Leave mWsInfo.type = 0 - Who knows what this is meant to be?

  XFlush(mWsInfo.display);

  return NS_OK;
}

/* static */
gboolean
plug_removed_cb (GtkWidget *widget, gpointer data)
{
  // Gee, thanks for the info!
  return TRUE;
}

static void
socket_unrealize_cb(GtkWidget *widget, gpointer data)
{
  // Unmap and reparent any child windows that GDK does not yet know about.
  // (See bug 540114 comment 10.)
  GdkWindow* socket_window = widget->window;
  Display* display = GDK_DISPLAY();

  // Ignore X errors that may happen if windows get destroyed (possibly
  // requested by the plugin) between XQueryTree and when we operate on them.
  gdk_error_trap_push();

  Window root, parent;
  Window* children;
  unsigned int nchildren;
  if (!XQueryTree(display, gdk_x11_drawable_get_xid(socket_window),
                  &root, &parent, &children, &nchildren))
    return;

  for (unsigned int i = 0; i < nchildren; ++i) {
    Window child = children[i];
    if (!gdk_window_lookup(child)) {
      // This window is not known to GDK.
      XUnmapWindow(display, child);
      XReparentWindow(display, child, DefaultRootWindow(display), 0, 0);
    }
  }

  if (children) XFree(children);

  XSync(display, False);
  gdk_error_trap_pop();
}

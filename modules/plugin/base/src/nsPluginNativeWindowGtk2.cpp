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
#include "gtk2xtbin.h"
#ifdef OJI
#include "plstr.h"
#include "nsIPlugin.h"
#include "nsIPluginHost.h"

static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
#endif

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
  PRBool    CanGetValueFromPlugin(nsCOMPtr<nsIPluginInstance> &aPluginInstance);
};

static gboolean plug_removed_cb   (GtkWidget *widget, gpointer data);

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
  type = nsPluginWindowType_Window;
  mSocketWidget = 0;
  mWsInfo.type = 0;
  mWsInfo.display = nsnull;
  mWsInfo.visual = nsnull;
  mWsInfo.colormap = 0;
  mWsInfo.depth = 0;
}

nsPluginNativeWindowGtk2::~nsPluginNativeWindowGtk2() 
{
  if(mSocketWidget) {
    gtk_widget_destroy(mSocketWidget);
  }
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

nsresult nsPluginNativeWindowGtk2::CallSetWindow(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
{
  if(aPluginInstance) {
    if (type == nsPluginWindowType_Window) {
      nsresult rv;
      if(!mSocketWidget) {
        PRBool needXEmbed = PR_FALSE;
        if (CanGetValueFromPlugin(aPluginInstance)) {
          rv = aPluginInstance->GetValue
            ((nsPluginInstanceVariable)NPPVpluginNeedsXEmbed, &needXEmbed);
#ifdef DEBUG
          printf("nsPluginNativeWindowGtk2: NPPVpluginNeedsXEmbed=%d\n", needXEmbed);
#endif
        }
        if(needXEmbed) {
          CreateXEmbedWindow();
        }
        else {
          CreateXtWindow();
        }
      }

      if(!mSocketWidget)
        return NS_ERROR_FAILURE;

      // Make sure to resize and re-place the window if required.
      // Need to reset "window" each time as nsObjectFrame::DidReflow sets it
      // to the ancestor window.
      if(GTK_IS_XTBIN(mSocketWidget)) {
        gtk_xtbin_resize(mSocketWidget, width, height);
        // Point the NPWindow structures window to the actual X window
        window = (nsPluginPort *)GTK_XTBIN(mSocketWidget)->xtwindow;
      }
      else { // XEmbed
        SetAllocation();
        window = (nsPluginPort *)gtk_socket_get_id(GTK_SOCKET(mSocketWidget));
      }
#ifdef DEBUG
      printf("nsPluginNativeWindowGtk2: call SetWindow with xid=%p\n", (void *)window);
#endif
    } // nsPluginWindowType_Window
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

  g_signal_connect(mGtkSocket, "destroy",
                   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &mGtkSocket);

  gpointer user_data = NULL;
  gdk_window_get_user_data(parent_win, &user_data);

  GtkContainer *container = GTK_CONTAINER(user_data);
  gtk_container_add(container, mSocketWidget);
  gtk_widget_realize(mSocketWidget);

  // Resize before we show
  SetAllocation();

  gtk_widget_show(mSocketWidget);

  gdk_flush();
  window = (nsPluginPort *)gtk_socket_get_id(GTK_SOCKET(mSocketWidget));

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

PRBool nsPluginNativeWindowGtk2::CanGetValueFromPlugin(nsCOMPtr<nsIPluginInstance> &aPluginInstance)
{
#ifdef OJI
  if(aPluginInstance) {
    nsresult rv;
    nsCOMPtr<nsIPluginInstancePeer> peer;

    rv = aPluginInstance->GetPeer(getter_AddRefs(peer));
    if (NS_SUCCEEDED(rv) && peer) {
      const char *aMimeType = nsnull;

      peer->GetMIMEType((nsMIMEType*)&aMimeType);
      if (aMimeType &&
          (PL_strncasecmp(aMimeType, "application/x-java-vm", 21) == 0 ||
           PL_strncasecmp(aMimeType, "application/x-java-applet", 25) == 0)) {
        nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(kPluginManagerCID, &rv);
        if (NS_SUCCEEDED(rv) && pluginHost) {
          nsIPlugin* pluginFactory = NULL;

          rv = pluginHost->GetPluginFactory("application/x-java-vm", &pluginFactory);
          if (NS_SUCCEEDED(rv) && pluginFactory) {
            const char * jpiDescription;

            pluginFactory->GetValue(nsPluginVariable_DescriptionString, (void*)&jpiDescription);
            /** 
             * "Java(TM) Plug-in" is Sun's Java Plugin Trademark,
             * so we are sure that this is Sun 's Java Plugin if 
             * the description start with "Java(TM) Plug-in"
             **/
            if (PL_strncasecmp(jpiDescription, "Java(TM) Plug-in", 16) == 0) {
              // Java Plugin support Xembed from JRE 1.5
              if (PL_strcasecmp(jpiDescription + 17, "1.5") < 0)
                return PR_FALSE;
            }
            if (PL_strncasecmp(jpiDescription, "<a href=\"http://www.blackdown.org/java-linux.html\">", 51) == 0) {
              // Java Plugin support Xembed from JRE 1.5
              if (PL_strcasecmp(jpiDescription + 92, "1.5") < 0)
                return PR_FALSE;
            }
            if (PL_strncasecmp(jpiDescription, "IBM Java(TM) Plug-in", 20) == 0) {
              // Java Plugin support Xembed from JRE 1.5
              if (PL_strcasecmp(jpiDescription + 27, "1.5") < 0)
                return PR_FALSE;
            }
          }
        }
      }
    }
  }
#endif

  return PR_TRUE;
}

/* static */
gboolean
plug_removed_cb (GtkWidget *widget, gpointer data)
{
  // Gee, thanks for the info!
  return TRUE;
}



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
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003
 * the Sun Microsystems, Inc. All Rights Reserved.
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
  GtkWidget*  mGtkSocket;
  nsresult  CreateXEmbedWindow();
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
  ws_info = nsnull;
  type = nsPluginWindowType_Window;
  mGtkSocket = 0;
}

nsPluginNativeWindowGtk2::~nsPluginNativeWindowGtk2() 
{
  if(mGtkSocket) {
    gtk_widget_destroy(mGtkSocket);
    mGtkSocket = 0;
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
    nsresult rv;
    PRBool val = PR_FALSE;
    if(!mGtkSocket) {
      if (CanGetValueFromPlugin(aPluginInstance))
        rv = aPluginInstance->GetValue
               ((nsPluginInstanceVariable)NPPVpluginNeedsXEmbed, &val);
    }
#ifdef DEBUG
    printf("nsPluginNativeWindowGtk2: NPPVpluginNeedsXEmbed=%d\n", val);
#endif
    if(val) {
      CreateXEmbedWindow();
    }

    if(mGtkSocket) {
      // Make sure to resize and re-place the window if required
      SetAllocation();
      window = (nsPluginPort *)gtk_socket_get_id(GTK_SOCKET(mGtkSocket));
    }
#ifdef DEBUG
    printf("nsPluginNativeWindowGtk2: call SetWindow with xid=%p\n", (void *)window);
#endif
    aPluginInstance->SetWindow(this);
  }
  else if (mPluginInstance)
    mPluginInstance->SetWindow(nsnull);

  SetPluginInstance(aPluginInstance);
  return NS_OK;
}

nsresult nsPluginNativeWindowGtk2::CreateXEmbedWindow() {
  if(!mGtkSocket) {
    GdkWindow *win = gdk_window_lookup((XID)window);
    mGtkSocket = gtk_socket_new();

    //attach the socket to the container widget
    gtk_widget_set_parent_window(mGtkSocket, win);

    // Make sure to handle the plug_removed signal.  If we don't the
    // socket will automatically be destroyed when the plug is
    // removed, which means we're destroying it more than once.
    // SYNTAX ERROR.
    g_signal_connect(mGtkSocket, "plug_removed",
                     G_CALLBACK(plug_removed_cb), NULL);

    gpointer user_data = NULL;
    gdk_window_get_user_data(win, &user_data);

    GtkContainer *container = GTK_CONTAINER(user_data);
    gtk_container_add(container, mGtkSocket);
    gtk_widget_realize(mGtkSocket);

    // Resize before we show
    SetAllocation();

    gtk_widget_show(mGtkSocket);

    gdk_flush();
    window = (nsPluginPort *)gtk_socket_get_id(GTK_SOCKET(mGtkSocket));
  }

  return NS_OK;
}

void nsPluginNativeWindowGtk2::SetAllocation() {
  if (!mGtkSocket)
    return;

  GtkAllocation new_allocation;
  new_allocation.x = 0;
  new_allocation.y = 0;
  new_allocation.width = width;
  new_allocation.height = height;
  gtk_widget_size_allocate(mGtkSocket, &new_allocation);
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



/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef ns4xPluginInstance_h__
#define ns4xPluginInstance_h__

#define _UINT32

/* On HPUX, int32 is already defined in /usr/include/moduel.h */
/* #ifndef hpux */
#ifndef HPUX11
#define _INT32
#endif /* HPUX11 */

#include "nsplugin.h"
#include "npupp.h"
#include "jri.h"
#include "prlink.h"  // for PRLibrary
#include "nsIScriptablePlugin.h"

#ifdef MOZ_WIDGET_GTK
#include <gtk/gtk.h>
#endif

////////////////////////////////////////////////////////////////////////

class ns4xPluginStreamListener;

struct nsInstanceStream
{
  nsInstanceStream *mNext;
  ns4xPluginStreamListener *mPluginStreamListener;

  nsInstanceStream();
  ~nsInstanceStream();
};

class ns4xPluginInstance : public nsIPluginInstance,
                           public nsIScriptablePlugin
{
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////
    // nsIPluginInstance methods

    /**
     * Actually initialize the plugin instance. This calls the 4.x <b>newp</b>
     * callback, and may return an error (which is why it is distinct from the
     * constructor.) If an error is returned, the caller should <i>not</i>
     * continue to use the <b>ns4xPluginInstance</b> object.
     */
    NS_METHOD Initialize(nsIPluginInstancePeer* peer);

    NS_IMETHOD GetPeer(nsIPluginInstancePeer* *resultingPeer);

    NS_IMETHOD Start(void);

    NS_IMETHOD Stop(void);

    NS_IMETHOD Destroy(void);

    NS_IMETHOD SetWindow(nsPluginWindow* window);

    NS_IMETHOD NewStream(nsIPluginStreamListener** listener);

    NS_IMETHOD Print(nsPluginPrint* platformPrint);

    NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);

    NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);
    
    ////////////////////////////////////////////////////////////////////////
    // nsIScriptablePlugin methods

    NS_IMETHOD GetScriptablePeer(void * *aScriptablePeer);

    NS_IMETHOD GetScriptableInterface(nsIID * *aScriptableInterface);

    ////////////////////////////////////////////////////////////////////////
    // ns4xPluginInstance-specific methods

    /**
     * Return the 4.x-style interface object.
     */
    nsresult GetNPP(NPP * aNPP);

    /**
     * Return the callbacks for the plugin instance.
     */
    nsresult GetCallbacks(const NPPluginFuncs ** aCallbacks);

    nsresult SetWindowless(PRBool aWindowless);

    nsresult SetTransparent(PRBool aTransparent);

    nsresult NewNotifyStream(nsIPluginStreamListener** listener, void* notifyData);

    /**
     * Construct a new 4.x plugin instance with the specified peer
     * and callbacks.
     */
    ns4xPluginInstance(NPPluginFuncs* callbacks, PRLibrary* aLibrary);

    // Use Release() to destroy this
    virtual ~ns4xPluginInstance(void);

    // returns the state of mStarted
    PRBool IsStarted(void);
    
protected:

    nsresult InitializePlugin(nsIPluginInstancePeer* peer);
    
    /**
     * The plugin instance peer for this instance.
     */
    nsCOMPtr<nsIPluginInstancePeer> mPeer;

    /**
     * A pointer to the plugin's callback functions. This information
     * is actually stored in the plugin class (<b>nsPluginClass</b>),
     * and is common for all plugins of the class.
     */
    NPPluginFuncs* fCallbacks;

#ifdef MOZ_WIDGET_GTK
   /**
    * Special GtkXtBin widget that encapsulates the Xt toolkit
    * within a Gtk Application
    */
   GtkWidget *mXtBin;
#endif

    /**
     * The 4.x-style structure used to communicate between the plugin
     * instance and the browser.
     */
    NPP_t fNPP;

    //these are used to store the windowless properties
    //which the browser will later query

    PRBool  mWindowless;
    PRBool  mTransparent;
    PRBool  mStarted;

public:
    PRLibrary* fLibrary;
    nsInstanceStream *mStreams;
};

#endif // ns4xPluginInstance_h__

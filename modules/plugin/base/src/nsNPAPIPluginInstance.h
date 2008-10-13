/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef nsNPAPIPluginInstance_h_
#define nsNPAPIPluginInstance_h_

#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsIPlugin.h"
#include "nsIPluginInstance.h"
#include "nsIPluginInstancePeer.h"
#include "nsIPluginTagInfo2.h"
#include "nsIScriptablePlugin.h"
#include "nsIPluginInstanceInternal.h"

#include "npfunctions.h"
#include "prlink.h"

class nsNPAPIPluginStreamListener;
class nsPIDOMWindow;

struct nsInstanceStream
{
    nsInstanceStream *mNext;
    nsNPAPIPluginStreamListener *mPluginStreamListener;

    nsInstanceStream();
    ~nsInstanceStream();
};

class nsNPAPIPluginInstance : public nsIPluginInstance,
                              public nsIScriptablePlugin,
                              public nsIPluginInstanceInternal
{
public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPLUGININSTANCE
    NS_DECL_NSISCRIPTABLEPLUGIN

    // nsIPluginInstanceInternal methods

    virtual JSObject *GetJSObject(JSContext *cx);

    virtual nsresult GetFormValue(nsAString& aValue);

    virtual void PushPopupsEnabledState(PRBool aEnabled);
    virtual void PopPopupsEnabledState();

    virtual PRUint16 GetPluginAPIVersion();

    virtual void DefineJavaProperties();

    // nsNPAPIPluginInstance-specific methods

    // Return the 4.x-style interface object.
    nsresult GetNPP(NPP * aNPP);

    // Return the callbacks for the plugin instance.
    nsresult GetCallbacks(const NPPluginFuncs ** aCallbacks);

    NPError SetWindowless(PRBool aWindowless);

    NPError SetTransparent(PRBool aTransparent);

    NPError SetWantsAllNetworkStreams(PRBool aWantsAllNetworkStreams);

#ifdef XP_MACOSX
    void SetDrawingModel(NPDrawingModel aModel);
    NPDrawingModel GetDrawingModel();
#endif

    nsresult NewNotifyStream(nsIPluginStreamListener** listener, 
                             void* notifyData, 
                             PRBool aCallNotify,
                             const char * aURL);

    // Construct a new 4.x plugin instance with the specified peer
    // and callbacks.
    nsNPAPIPluginInstance(NPPluginFuncs* callbacks, PRLibrary* aLibrary);

    // Use Release() to destroy this
    virtual ~nsNPAPIPluginInstance(void);

    // returns the state of mStarted
    PRBool IsStarted(void);

    // cache this 4.x plugin like an XPCOM plugin
    nsresult SetCached(PRBool aCache) { mCached = aCache; return NS_OK; }

    // Non-refcounting accessor for faster access to the peer.
    nsIPluginInstancePeer *Peer()
    {
        return mPeer;
    }

    already_AddRefed<nsPIDOMWindow> GetDOMWindow();

protected:

    nsresult InitializePlugin(nsIPluginInstancePeer* peer);

    // Calls NPP_GetValue
    nsresult GetValueInternal(NPPVariable variable, void* value);

    // The plugin instance peer for this instance.
    nsCOMPtr<nsIPluginInstancePeer> mPeer;

    // A pointer to the plugin's callback functions. This information
    // is actually stored in the plugin class (<b>nsPluginClass</b>),
    // and is common for all plugins of the class.
    NPPluginFuncs* fCallbacks;

    // The 4.x-style structure used to communicate between the plugin
    // instance and the browser.
    NPP_t fNPP;

#ifdef XP_MACOSX
    NPDrawingModel mDrawingModel;
#endif

    // these are used to store the windowless properties
    // which the browser will later query
    PRPackedBool  mWindowless;
    PRPackedBool  mTransparent;
    PRPackedBool  mStarted;
    PRPackedBool  mCached;
    PRPackedBool  mIsJavaPlugin;
    PRPackedBool  mWantsAllNetworkStreams;

public:
    // True while creating the plugin, or calling NPP_SetWindow() on it.
    PRPackedBool  mInPluginInitCall;
    PRLibrary* fLibrary;
    nsInstanceStream *mStreams;

    nsVoidArray mPopupStates;
};

#endif // nsNPAPIPluginInstance_h_

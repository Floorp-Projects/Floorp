/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef ns4xPluginInstance_h__
#define ns4xPluginInstance_h__

#define _UINT32
#define _INT32

#include "nsplugin.h"
#include "npupp.h"
#include "jri.h"

////////////////////////////////////////////////////////////////////////

class ns4xPluginInstance : public nsIPluginInstance
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
    NS_METHOD
    Initialize(nsIPluginInstancePeer* peer);

    NS_IMETHOD
    GetPeer(nsIPluginInstancePeer* *resultingPeer);

    NS_IMETHOD
    Start(void);

    NS_IMETHOD
    Stop(void);

    NS_IMETHOD
    Destroy(void);

    NS_IMETHOD
    SetWindow(nsPluginWindow* window);

    NS_IMETHOD
    NewStream(nsIPluginStreamListener** listener);

    NS_IMETHOD
    Print(nsPluginPrint* platformPrint);

    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value);

	NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled);
    
	////////////////////////////////////////////////////////////////////////
    // ns4xPluginInstance-specific methods

    /**
     * Return the 4.x-style interface object.
     */
    nsresult
    GetNPP(NPP * aNPP);

    /**
     * Return the callbacks for the plugin instance.
     */
    nsresult
    GetCallbacks(const NPPluginFuncs ** aCallbacks);

    nsresult
    SetWindowless(PRBool aWindowless);

    nsresult
    SetTransparent(PRBool aTransparent);

	nsresult
	NewNotifyStream(nsIPluginStreamListener** listener, void* notifyData);

	/**
     * Construct a new 4.x plugin instance with the specified peer
     * and callbacks.
     */
    ns4xPluginInstance(NPPluginFuncs* callbacks);

    // Use Release() to destroy this
    virtual ~ns4xPluginInstance(void);

protected:

    /**
     * The plugin instance peer for this instance.
     */
    nsIPluginInstancePeer* fPeer;

    /**
     * A pointer to the plugin's callback functions. This information
     * is actually stored in the plugin class (<b>nsPluginClass</b>),
     * and is common for all plugins of the class.
     */
    NPPluginFuncs* fCallbacks;

    /**
     * The 4.x-style structure used to communicate between the plugin
     * instance and the browser.
     */
    NPP_t fNPP;

    //these are used to store the windowless properties
    //which the browser will later query

    PRBool  mWindowless;
    PRBool  mTransparent;
};


#endif // ns4xPluginInstance_h__

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsPluginInstancePeer_h__
#define nsPluginInstancePeer_h__

#include "nsIPluginInstancePeer.h"
#include "nsILiveConnectPlugInstPeer.h"
#include "nsIWindowlessPlugInstPeer.h"
#include "npglue.h"

typedef struct JSContext JSContext;

class nsPluginTagInfo;

class nsPluginInstancePeer : public nsIPluginInstancePeer,
                             public nsILiveConnectPluginInstancePeer,
                             public nsIWindowlessPluginInstancePeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInstancePeer:

    NS_IMETHOD
    GetValue(nsPluginInstancePeerVariable variable, void *value);

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD
    GetMode(nsPluginMode *result);

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    // (Corresponds to NPN_Status.)
    NS_IMETHOD
    ShowStatus(const char* message);

    NS_IMETHOD
    SetWindowSize(PRUint32 width, PRUint32 height);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIJRILiveConnectPluginInstancePeer:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD
    GetJavaPeer(jobject *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIWindowlessPluginInstancePeer:

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD
    InvalidateRect(nsPluginRect *invalidRect);

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD
    InvalidateRegion(nsPluginRegion invalidRegion);

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD
    ForceRedraw(void);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginInstancePeer specific methods:

    nsPluginInstancePeer(NPP npp);
    virtual ~nsPluginInstancePeer(void);

    NS_DECL_ISUPPORTS

    void SetPluginInstance(nsIPluginInstance* inst);
    nsIPluginInstance* GetPluginInstance(void);
    
    NPP GetNPP(void);
    JSContext *GetJSContext(void);
    MWContext *GetMWContext(void);
protected:

    // NPP is the old plugin structure. If we were implementing this
    // from scratch we wouldn't use it, but for now we're calling the old
    // npglue.cpp routines wherever possible.
    NPP fNPP;

    nsIPluginInstance*  fPluginInst;
    nsPluginTagInfo*    fTagInfo;
};

#define NS_PLUGININSTANCEPEER_CID                    \
{ /* 766432d0-01ba-11d2-815b-006008119d7a */         \
    0x766432d0,                                      \
    0x01ba,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // nsPluginInstancePeer_h__

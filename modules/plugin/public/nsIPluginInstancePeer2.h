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

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).</B>
 *
 * <P>This superscedes the old plugin API (npapi.h, npupp.h), and 
 * eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp that
 * get linked with the plugin. You will however need to link with the "backward
 * adapter" (badapter.cpp) in order to allow your plugin to run in pre-5.0
 * browsers. 
 *
 * <P>See nsplugin.h for an overview of how this interface fits with the 
 * overall plugin architecture.
 */
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIPluginInstancePeer2_h___
#define nsIPluginInstancePeer2_h___

#include "nsplugindefs.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer 2 Interface
// These extensions to nsIPluginManager are only available in Communicator 5.0.

class nsIPluginInstancePeer2 : public nsIPluginInstancePeer {
public:
    
    ////////////////////////////////////////////////////////////////////////////
    // New top-level window handling calls for Mac:
    
    NS_IMETHOD_(void)
    RegisterWindow(void* window) = 0;
    
    NS_IMETHOD_(void)
    UnregisterWindow(void* window) = 0;

	// Menu ID allocation calls for Mac:
    NS_IMETHOD_(PRInt16)
	AllocateMenuID(PRBool isSubmenu) = 0;

	// On the mac (and most likely win16), network activity can
    // only occur on the main thread. Therefore, we provide a hook
    // here for the case that the main thread needs to tickle itself.
    // In this case, we make sure that we give up the monitor so that
    // the tickle code can notify it without freezing.
    NS_IMETHOD_(PRBool)
    Tickle(void) = 0;

};

#define NS_IPLUGININSTANCEPEER2_IID                  \
{ /* 51b52b80-019b-11d2-815b-006008119d7a */         \
    0x51b52b80,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstancePeer2_h___ */

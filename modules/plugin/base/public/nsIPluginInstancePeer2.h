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

#ifndef nsIPluginInstancePeer2_h___
#define nsIPluginInstancePeer2_h___

#include "nsIPluginInstancePeer.h"

struct JSObject;

// {e7d48c00-e1f1-11d2-8360-fbc8abc4ae7c}
#define NS_IPLUGININSTANCEPEER2_IID \
{ 0xe7d48c00, 0xe1f1, 0x11d2, { 0x83, 0x60, 0xfb, 0xc8, 0xab, 0xc4, 0xae, 0x7c } }

/**
 * The nsIPluginInstancePeer2 interface extends the nsIPluginInstancePeer
 * interface, providing access to functionality provided by newer browsers.
 * All functionality in nsIPluginInstancePeer can be mapped to the 4.X
 * plugin API.
 */
class nsIPluginInstancePeer2 : public nsIPluginInstancePeer {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGININSTANCEPEER2_IID)
	
    /**
     * Get the JavaScript window object corresponding to this plugin instance.
     *
     * @param outJSWindow - the resulting JavaScript window object
     * @result - NS_OK if this operation was successful
     */
	NS_IMETHOD
	GetJSWindow(JSObject* *outJSWindow) = 0;
	
	/**
	 * Get the JavaScript execution thread corresponding to this plugin instance.
	 *
	 * @param outThreadID - the resulting JavaScript thread
	 * @result - NS_OK if this operation was successful
	 */
	NS_IMETHOD
	GetJSThread(PRUint32 *outThreadID) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstancePeer2_h___ */

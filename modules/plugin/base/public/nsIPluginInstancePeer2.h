/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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

#ifndef nsIPluginInstancePeer2_h___
#define nsIPluginInstancePeer2_h___

#include "nsIPluginInstancePeer.h"

struct JSObject;
struct JSContext;

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

	/**
	 * Get the JavaScript context to this plugin instance.
	 *
	 * @param outContext - the resulting JavaScript context
	 * @result - NS_OK if this operation was successful
	 */
	NS_IMETHOD
	GetJSContext(JSContext* *outContext) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginInstancePeer2_h___ */

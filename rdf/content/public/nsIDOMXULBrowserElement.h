/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMXULBrowserElement_h__
#define nsIDOMXULBrowserElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIWebBrowser;

#define NS_IDOMXULBROWSERELEMENT_IID \
 { 0xd31208d0, 0xe348, 0x11d3, \
	 { 0xb0, 0x6b, 0x0, 0xa0, 0x24, 0xff, 0xc0, 0x8c } } 

class nsIDOMXULBrowserElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULBROWSERELEMENT_IID; return iid; }

  NS_IMETHOD    GetWebBrowser(nsIWebBrowser** aWebBrowser)=0;
};


#define NS_DECL_IDOMXULBROWSERELEMENT   \
  NS_IMETHOD    GetWebBrowser(nsIWebBrowser** aWebBrowser);  \



#define NS_FORWARD_IDOMXULBROWSERELEMENT(_to)  \
  NS_IMETHOD    GetWebBrowser(nsIWebBrowser** aWebBrowser) { return _to GetWebBrowser(aWebBrowser); } \


extern "C" NS_DOM nsresult NS_InitXULBrowserElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULBrowserElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULBrowserElement_h__

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

#ifndef nsIDOMXULIFrameElement_h__
#define nsIDOMXULIFrameElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIDocShell;

#define NS_IDOMXULIFRAMEELEMENT_IID \
 { 0xd31208d1, 0xe348, 0x11d3, \
	 { 0xb0, 0x6b, 0x0, 0xa0, 0x24, 0xff, 0xc0, 0x8c } } 

class nsIDOMXULIFrameElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULIFRAMEELEMENT_IID; return iid; }

  NS_IMETHOD    GetDocShell(nsIDocShell** aDocShell)=0;
};


#define NS_DECL_IDOMXULIFRAMEELEMENT   \
  NS_IMETHOD    GetDocShell(nsIDocShell** aDocShell);  \



#define NS_FORWARD_IDOMXULIFRAMEELEMENT(_to)  \
  NS_IMETHOD    GetDocShell(nsIDocShell** aDocShell) { return _to GetDocShell(aDocShell); } \


extern "C" NS_DOM nsresult NS_InitXULIFrameElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULIFrameElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULIFrameElement_h__

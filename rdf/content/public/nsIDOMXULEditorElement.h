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

#ifndef nsIDOMXULEditorElement_h__
#define nsIDOMXULEditorElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIEditorShell;

#define NS_IDOMXULEDITORELEMENT_IID \
 { 0x9a248050, 0x82d8, 0x11d3, \
	 { 0xaf, 0x76, 0x0, 0xa0, 0x24, 0xff, 0xc0, 0x8c } } 

class nsIDOMXULEditorElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULEDITORELEMENT_IID; return iid; }

  NS_IMETHOD    GetEditorShell(nsIEditorShell** aEditorShell)=0;
};


#define NS_DECL_IDOMXULEDITORELEMENT   \
  NS_IMETHOD    GetEditorShell(nsIEditorShell** aEditorShell);  \



#define NS_FORWARD_IDOMXULEDITORELEMENT(_to)  \
  NS_IMETHOD    GetEditorShell(nsIEditorShell** aEditorShell) { return _to GetEditorShell(aEditorShell); } \


extern "C" NS_DOM nsresult NS_InitXULEditorElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULEditorElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULEditorElement_h__

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

#ifndef nsIDOMXULRadioGroupElement_h__
#define nsIDOMXULRadioGroupElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIDOMXULRadioElement;

#define NS_IDOMXULRADIOGROUPELEMENT_IID \
 { 0xc2dd83e1, 0xef22, 0x11d3, \
{ 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } } 

class nsIDOMXULRadioGroupElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULRADIOGROUPELEMENT_IID; return iid; }

  NS_IMETHOD    GetSelectedItem(nsIDOMXULRadioElement** aSelectedItem)=0;
  NS_IMETHOD    SetSelectedItem(nsIDOMXULRadioElement* aSelectedItem)=0;
};


#define NS_DECL_IDOMXULRADIOGROUPELEMENT   \
  NS_IMETHOD    GetSelectedItem(nsIDOMXULRadioElement** aSelectedItem);  \
  NS_IMETHOD    SetSelectedItem(nsIDOMXULRadioElement* aSelectedItem);  \



#define NS_FORWARD_IDOMXULRADIOGROUPELEMENT(_to)  \
  NS_IMETHOD    GetSelectedItem(nsIDOMXULRadioElement** aSelectedItem) { return _to GetSelectedItem(aSelectedItem); } \
  NS_IMETHOD    SetSelectedItem(nsIDOMXULRadioElement* aSelectedItem) { return _to SetSelectedItem(aSelectedItem); } \


extern "C" NS_DOM nsresult NS_InitXULRadioGroupElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULRadioGroupElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULRadioGroupElement_h__

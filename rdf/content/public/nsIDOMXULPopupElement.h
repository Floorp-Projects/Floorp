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

#ifndef nsIDOMXULPopupElement_h__
#define nsIDOMXULPopupElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIDOMElement;

#define NS_IDOMXULPOPUPELEMENT_IID \
 { 0x8fefe4a1, 0xd334, 0x11d3, \
    {0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27} } 

class nsIDOMXULPopupElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULPOPUPELEMENT_IID; return iid; }

  NS_IMETHOD    OpenPopup(nsIDOMElement* aElement, PRInt32 aXPos, PRInt32 aYPos, const nsString& aPopupType, const nsString& aAnchorAlignment, const nsString& aPopupAlignment)=0;

  NS_IMETHOD    ClosePopup()=0;
};


#define NS_DECL_IDOMXULPOPUPELEMENT   \
  NS_IMETHOD    OpenPopup(nsIDOMElement* aElement, PRInt32 aXPos, PRInt32 aYPos, const nsString& aPopupType, const nsString& aAnchorAlignment, const nsString& aPopupAlignment);  \
  NS_IMETHOD    ClosePopup();  \



#define NS_FORWARD_IDOMXULPOPUPELEMENT(_to)  \
  NS_IMETHOD    OpenPopup(nsIDOMElement* aElement, PRInt32 aXPos, PRInt32 aYPos, const nsString& aPopupType, const nsString& aAnchorAlignment, const nsString& aPopupAlignment) { return _to OpenPopup(aElement, aXPos, aYPos, aPopupType, aAnchorAlignment, aPopupAlignment); }  \
  NS_IMETHOD    ClosePopup() { return _to ClosePopup(); }  \


extern "C" NS_DOM nsresult NS_InitXULPopupElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULPopupElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULPopupElement_h__

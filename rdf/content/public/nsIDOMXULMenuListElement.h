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

#ifndef nsIDOMXULMenuListElement_h__
#define nsIDOMXULMenuListElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIDOMElement;

#define NS_IDOMXULMENULISTELEMENT_IID \
 { 0xb30561c1, 0xea1a, 0x11d3, \
  { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } } 

class nsIDOMXULMenuListElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULMENULISTELEMENT_IID; return iid; }

  NS_IMETHOD    GetValue(nsAWritableString& aValue)=0;
  NS_IMETHOD    SetValue(const nsAReadableString& aValue)=0;

  NS_IMETHOD    GetData(nsAWritableString& aData)=0;
  NS_IMETHOD    SetData(const nsAReadableString& aData)=0;

  NS_IMETHOD    GetSelectedItem(nsIDOMElement** aSelectedItem)=0;
  NS_IMETHOD    SetSelectedItem(nsIDOMElement* aSelectedItem)=0;

  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex)=0;
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex)=0;

  NS_IMETHOD    GetCrop(nsAWritableString& aCrop)=0;
  NS_IMETHOD    SetCrop(const nsAReadableString& aCrop)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetSrc(nsAWritableString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc)=0;
};


#define NS_DECL_IDOMXULMENULISTELEMENT   \
  NS_IMETHOD    GetValue(nsAWritableString& aValue);  \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue);  \
  NS_IMETHOD    GetData(nsAWritableString& aData);  \
  NS_IMETHOD    SetData(const nsAReadableString& aData);  \
  NS_IMETHOD    GetSelectedItem(nsIDOMElement** aSelectedItem);  \
  NS_IMETHOD    SetSelectedItem(nsIDOMElement* aSelectedItem);  \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex);  \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex);  \
  NS_IMETHOD    GetCrop(nsAWritableString& aCrop);  \
  NS_IMETHOD    SetCrop(const nsAReadableString& aCrop);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc);  \



#define NS_FORWARD_IDOMXULMENULISTELEMENT(_to)  \
  NS_IMETHOD    GetValue(nsAWritableString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsAReadableString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    GetData(nsAWritableString& aData) { return _to GetData(aData); } \
  NS_IMETHOD    SetData(const nsAReadableString& aData) { return _to SetData(aData); } \
  NS_IMETHOD    GetSelectedItem(nsIDOMElement** aSelectedItem) { return _to GetSelectedItem(aSelectedItem); } \
  NS_IMETHOD    SetSelectedItem(nsIDOMElement* aSelectedItem) { return _to SetSelectedItem(aSelectedItem); } \
  NS_IMETHOD    GetSelectedIndex(PRInt32* aSelectedIndex) { return _to GetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    SetSelectedIndex(PRInt32 aSelectedIndex) { return _to SetSelectedIndex(aSelectedIndex); } \
  NS_IMETHOD    GetCrop(nsAWritableString& aCrop) { return _to GetCrop(aCrop); } \
  NS_IMETHOD    SetCrop(const nsAReadableString& aCrop) { return _to SetCrop(aCrop); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetSrc(nsAWritableString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsAReadableString& aSrc) { return _to SetSrc(aSrc); } \


extern "C" NS_DOM nsresult NS_InitXULMenuListElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULMenuListElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULMenuListElement_h__

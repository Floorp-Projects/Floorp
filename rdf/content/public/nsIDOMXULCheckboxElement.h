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

#ifndef nsIDOMXULCheckboxElement_h__
#define nsIDOMXULCheckboxElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"


#define NS_IDOMXULCHECKBOXELEMENT_IID \
 { 0xa5f00fa2, 0xe874, 0x11d3, \
  { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } } 

class nsIDOMXULCheckboxElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULCHECKBOXELEMENT_IID; return iid; }

  NS_IMETHOD    GetValue(nsString& aValue)=0;
  NS_IMETHOD    SetValue(const nsString& aValue)=0;

  NS_IMETHOD    GetCrop(nsString& aCrop)=0;
  NS_IMETHOD    SetCrop(const nsString& aCrop)=0;

  NS_IMETHOD    GetDisabled(PRBool* aDisabled)=0;
  NS_IMETHOD    SetDisabled(PRBool aDisabled)=0;

  NS_IMETHOD    GetSrc(nsString& aSrc)=0;
  NS_IMETHOD    SetSrc(const nsString& aSrc)=0;

  NS_IMETHOD    GetImgalign(nsString& aImgalign)=0;
  NS_IMETHOD    SetImgalign(const nsString& aImgalign)=0;

  NS_IMETHOD    GetAccesskey(nsString& aAccesskey)=0;
  NS_IMETHOD    SetAccesskey(const nsString& aAccesskey)=0;

  NS_IMETHOD    GetChecked(PRBool* aChecked)=0;
  NS_IMETHOD    SetChecked(PRBool aChecked)=0;
};


#define NS_DECL_IDOMXULCHECKBOXELEMENT   \
  NS_IMETHOD    GetValue(nsString& aValue);  \
  NS_IMETHOD    SetValue(const nsString& aValue);  \
  NS_IMETHOD    GetCrop(nsString& aCrop);  \
  NS_IMETHOD    SetCrop(const nsString& aCrop);  \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled);  \
  NS_IMETHOD    SetDisabled(PRBool aDisabled);  \
  NS_IMETHOD    GetSrc(nsString& aSrc);  \
  NS_IMETHOD    SetSrc(const nsString& aSrc);  \
  NS_IMETHOD    GetImgalign(nsString& aImgalign);  \
  NS_IMETHOD    SetImgalign(const nsString& aImgalign);  \
  NS_IMETHOD    GetAccesskey(nsString& aAccesskey);  \
  NS_IMETHOD    SetAccesskey(const nsString& aAccesskey);  \
  NS_IMETHOD    GetChecked(PRBool* aChecked);  \
  NS_IMETHOD    SetChecked(PRBool aChecked);  \



#define NS_FORWARD_IDOMXULCHECKBOXELEMENT(_to)  \
  NS_IMETHOD    GetValue(nsString& aValue) { return _to GetValue(aValue); } \
  NS_IMETHOD    SetValue(const nsString& aValue) { return _to SetValue(aValue); } \
  NS_IMETHOD    GetCrop(nsString& aCrop) { return _to GetCrop(aCrop); } \
  NS_IMETHOD    SetCrop(const nsString& aCrop) { return _to SetCrop(aCrop); } \
  NS_IMETHOD    GetDisabled(PRBool* aDisabled) { return _to GetDisabled(aDisabled); } \
  NS_IMETHOD    SetDisabled(PRBool aDisabled) { return _to SetDisabled(aDisabled); } \
  NS_IMETHOD    GetSrc(nsString& aSrc) { return _to GetSrc(aSrc); } \
  NS_IMETHOD    SetSrc(const nsString& aSrc) { return _to SetSrc(aSrc); } \
  NS_IMETHOD    GetImgalign(nsString& aImgalign) { return _to GetImgalign(aImgalign); } \
  NS_IMETHOD    SetImgalign(const nsString& aImgalign) { return _to SetImgalign(aImgalign); } \
  NS_IMETHOD    GetAccesskey(nsString& aAccesskey) { return _to GetAccesskey(aAccesskey); } \
  NS_IMETHOD    SetAccesskey(const nsString& aAccesskey) { return _to SetAccesskey(aAccesskey); } \
  NS_IMETHOD    GetChecked(PRBool* aChecked) { return _to GetChecked(aChecked); } \
  NS_IMETHOD    SetChecked(PRBool aChecked) { return _to SetChecked(aChecked); } \


extern "C" NS_DOM nsresult NS_InitXULCheckboxElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULCheckboxElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULCheckboxElement_h__

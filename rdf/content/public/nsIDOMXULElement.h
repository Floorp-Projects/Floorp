/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#ifndef nsIDOMXULElement_h__
#define nsIDOMXULElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMElement.h"

class nsIDOMElement;
class nsIRDFResource;
class nsIDOMNodeList;

#define NS_IDOMXULELEMENT_IID \
 { 0x574ed81, 0xc088, 0x11d2, \
  { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } } 

class nsIDOMXULElement : public nsIDOMElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULELEMENT_IID; return iid; }

  NS_IMETHOD    GetResource(nsIRDFResource** aResource)=0;

  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    DoCommand()=0;

  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMXULELEMENT   \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource);  \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    DoCommand();  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMXULELEMENT(_to)  \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource) { return _to GetResource(aResource); } \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement) { return _to AddBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement) { return _to RemoveBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    DoCommand() { return _to DoCommand(); }  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn) { return _to GetElementsByAttribute(aName, aValue, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitXULElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULElement_h__

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

#ifndef nsIDOMElementObserver_h__
#define nsIDOMElementObserver_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMElement;
class nsIDOMAttr;

#define NS_IDOMELEMENTOBSERVER_IID \
 { 0x17ddd8c2, 0xc5f8, 0x11d2, \
        { 0xa6, 0xae, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } } 

class nsIDOMElementObserver : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMELEMENTOBSERVER_IID; return iid; }

  NS_IMETHOD    OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue)=0;

  NS_IMETHOD    OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName)=0;

  NS_IMETHOD    OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr)=0;

  NS_IMETHOD    OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr)=0;
};


#define NS_DECL_IDOMELEMENTOBSERVER   \
  NS_IMETHOD    OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue);  \
  NS_IMETHOD    OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName);  \
  NS_IMETHOD    OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr);  \
  NS_IMETHOD    OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr);  \



#define NS_FORWARD_IDOMELEMENTOBSERVER(_to)  \
  NS_IMETHOD    OnSetAttribute(nsIDOMElement* aElement, const nsString& aName, const nsString& aValue) { return _to OnSetAttribute(aElement, aName, aValue); }  \
  NS_IMETHOD    OnRemoveAttribute(nsIDOMElement* aElement, const nsString& aName) { return _to OnRemoveAttribute(aElement, aName); }  \
  NS_IMETHOD    OnSetAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aNewAttr) { return _to OnSetAttributeNode(aElement, aNewAttr); }  \
  NS_IMETHOD    OnRemoveAttributeNode(nsIDOMElement* aElement, nsIDOMAttr* aOldAttr) { return _to OnRemoveAttributeNode(aElement, aOldAttr); }  \


extern "C" NS_DOM nsresult NS_InitElementObserverClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptElementObserver(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMElementObserver_h__

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

class nsIController;
class nsIDOMElement;
class nsIDOMCSSStyleDeclaration;
class nsIRDFCompositeDataSource;
class nsIRDFResource;
class nsIDOMNodeList;

#define NS_IDOMXULELEMENT_IID \
 { 0x574ed81, 0xc088, 0x11d2, \
  { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } } 

class nsIDOMXULElement : public nsIDOMElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULELEMENT_IID; return iid; }

  NS_IMETHOD    GetId(nsString& aId)=0;
  NS_IMETHOD    SetId(const nsString& aId)=0;

  NS_IMETHOD    GetClassName(nsString& aClassName)=0;
  NS_IMETHOD    SetClassName(const nsString& aClassName)=0;

  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle)=0;

  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase)=0;
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase)=0;

  NS_IMETHOD    GetResource(nsIRDFResource** aResource)=0;

  NS_IMETHOD    GetController(nsIController** aController)=0;
  NS_IMETHOD    SetController(nsIController* aController)=0;

  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    DoCommand()=0;

  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMXULELEMENT   \
  NS_IMETHOD    GetId(nsString& aId);  \
  NS_IMETHOD    SetId(const nsString& aId);  \
  NS_IMETHOD    GetClassName(nsString& aClassName);  \
  NS_IMETHOD    SetClassName(const nsString& aClassName);  \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);  \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase);  \
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase);  \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource);  \
  NS_IMETHOD    GetController(nsIController** aController);  \
  NS_IMETHOD    SetController(nsIController* aController);  \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    DoCommand();  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMXULELEMENT(_to)  \
  NS_IMETHOD    GetId(nsString& aId) { return _to GetId(aId); } \
  NS_IMETHOD    SetId(const nsString& aId) { return _to SetId(aId); } \
  NS_IMETHOD    GetClassName(nsString& aClassName) { return _to GetClassName(aClassName); } \
  NS_IMETHOD    SetClassName(const nsString& aClassName) { return _to SetClassName(aClassName); } \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { return _to GetStyle(aStyle); } \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase) { return _to GetDatabase(aDatabase); } \
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase) { return _to SetDatabase(aDatabase); } \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource) { return _to GetResource(aResource); } \
  NS_IMETHOD    GetController(nsIController** aController) { return _to GetController(aController); } \
  NS_IMETHOD    SetController(nsIController* aController) { return _to SetController(aController); } \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement) { return _to AddBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMElement* aElement) { return _to RemoveBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    DoCommand() { return _to DoCommand(); }  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn) { return _to GetElementsByAttribute(aName, aValue, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitXULElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULElement_h__

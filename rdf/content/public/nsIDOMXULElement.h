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

#ifndef nsIDOMXULElement_h__
#define nsIDOMXULElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMElement.h"

class nsIDOMElement;
class nsIDOMCSSStyleDeclaration;
class nsIRDFCompositeDataSource;
class nsIXULTemplateBuilder;
class nsIRDFResource;
class nsIBoxObject;
class nsIControllers;
class nsIDOMNodeList;

#define NS_IDOMXULELEMENT_IID \
 { 0x574ed81, 0xc088, 0x11d2, \
  { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } } 

class nsIDOMXULElement : public nsIDOMElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULELEMENT_IID; return iid; }

  NS_IMETHOD    GetId(nsAWritableString& aId)=0;
  NS_IMETHOD    SetId(const nsAReadableString& aId)=0;

  NS_IMETHOD    GetClassName(nsAWritableString& aClassName)=0;
  NS_IMETHOD    SetClassName(const nsAReadableString& aClassName)=0;

  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle)=0;

  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase)=0;

  NS_IMETHOD    GetBuilder(nsIXULTemplateBuilder** aBuilder)=0;

  NS_IMETHOD    GetResource(nsIRDFResource** aResource)=0;

  NS_IMETHOD    GetControllers(nsIControllers** aControllers)=0;

  NS_IMETHOD    GetBoxObject(nsIBoxObject** aBoxObject)=0;

  NS_IMETHOD    AddBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    RemoveBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement)=0;

  NS_IMETHOD    DoCommand()=0;

  NS_IMETHOD    Focus()=0;

  NS_IMETHOD    Blur()=0;

  NS_IMETHOD    Click()=0;

  NS_IMETHOD    GetElementsByAttribute(const nsAReadableString& aName, const nsAReadableString& aValue, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMXULELEMENT   \
  NS_IMETHOD    GetId(nsAWritableString& aId);  \
  NS_IMETHOD    SetId(const nsAReadableString& aId);  \
  NS_IMETHOD    GetClassName(nsAWritableString& aClassName);  \
  NS_IMETHOD    SetClassName(const nsAReadableString& aClassName);  \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle);  \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase);  \
  NS_IMETHOD    GetBuilder(nsIXULTemplateBuilder** aBuilder);  \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource);  \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers);  \
  NS_IMETHOD    GetBoxObject(nsIBoxObject** aBoxObject);  \
  NS_IMETHOD    AddBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    RemoveBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement);  \
  NS_IMETHOD    DoCommand();  \
  NS_IMETHOD    Focus();  \
  NS_IMETHOD    Blur();  \
  NS_IMETHOD    Click();  \
  NS_IMETHOD    GetElementsByAttribute(const nsAReadableString& aName, const nsAReadableString& aValue, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMXULELEMENT(_to)  \
  NS_IMETHOD    GetId(nsAWritableString& aId) { return _to GetId(aId); } \
  NS_IMETHOD    SetId(const nsAReadableString& aId) { return _to SetId(aId); } \
  NS_IMETHOD    GetClassName(nsAWritableString& aClassName) { return _to GetClassName(aClassName); } \
  NS_IMETHOD    SetClassName(const nsAReadableString& aClassName) { return _to SetClassName(aClassName); } \
  NS_IMETHOD    GetStyle(nsIDOMCSSStyleDeclaration** aStyle) { return _to GetStyle(aStyle); } \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase) { return _to GetDatabase(aDatabase); } \
  NS_IMETHOD    GetBuilder(nsIXULTemplateBuilder** aBuilder) { return _to GetBuilder(aBuilder); } \
  NS_IMETHOD    GetResource(nsIRDFResource** aResource) { return _to GetResource(aResource); } \
  NS_IMETHOD    GetControllers(nsIControllers** aControllers) { return _to GetControllers(aControllers); } \
  NS_IMETHOD    GetBoxObject(nsIBoxObject** aBoxObject) { return _to GetBoxObject(aBoxObject); } \
  NS_IMETHOD    AddBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement) { return _to AddBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    RemoveBroadcastListener(const nsAReadableString& aAttr, nsIDOMElement* aElement) { return _to RemoveBroadcastListener(aAttr, aElement); }  \
  NS_IMETHOD    DoCommand() { return _to DoCommand(); }  \
  NS_IMETHOD    Focus() { return _to Focus(); }  \
  NS_IMETHOD    Blur() { return _to Blur(); }  \
  NS_IMETHOD    Click() { return _to Click(); }  \
  NS_IMETHOD    GetElementsByAttribute(const nsAReadableString& aName, const nsAReadableString& aValue, nsIDOMNodeList** aReturn) { return _to GetElementsByAttribute(aName, aValue, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitXULElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULElement_h__

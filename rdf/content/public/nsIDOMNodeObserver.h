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

#ifndef nsIDOMNodeObserver_h__
#define nsIDOMNodeObserver_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMNODEOBSERVER_IID \
 { 0x3e969070, 0xc301, 0x11d2, \
        { 0xa6, 0xae, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } } 

class nsIDOMNodeObserver : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMNODEOBSERVER_IID; return iid; }

  NS_IMETHOD    OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue)=0;

  NS_IMETHOD    OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild)=0;

  NS_IMETHOD    OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild)=0;

  NS_IMETHOD    OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild)=0;

  NS_IMETHOD    OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild)=0;
};


#define NS_DECL_IDOMNODEOBSERVER   \
  NS_IMETHOD    OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue);  \
  NS_IMETHOD    OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild);  \
  NS_IMETHOD    OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild);  \
  NS_IMETHOD    OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild);  \
  NS_IMETHOD    OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild);  \



#define NS_FORWARD_IDOMNODEOBSERVER(_to)  \
  NS_IMETHOD    OnSetNodeValue(nsIDOMNode* aNode, const nsString& aValue) { return _to OnSetNodeValue(aNode, aValue); }  \
  NS_IMETHOD    OnInsertBefore(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aRefChild) { return _to OnInsertBefore(aParent, aNewChild, aRefChild); }  \
  NS_IMETHOD    OnReplaceChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild, nsIDOMNode* aOldChild) { return _to OnReplaceChild(aParent, aNewChild, aOldChild); }  \
  NS_IMETHOD    OnRemoveChild(nsIDOMNode* aParent, nsIDOMNode* aOldChild) { return _to OnRemoveChild(aParent, aOldChild); }  \
  NS_IMETHOD    OnAppendChild(nsIDOMNode* aParent, nsIDOMNode* aNewChild) { return _to OnAppendChild(aParent, aNewChild); }  \


extern "C" NS_DOM nsresult NS_InitNodeObserverClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptNodeObserver(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMNodeObserver_h__

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

#ifndef nsIDOMToolkitCore_h__
#define nsIDOMToolkitCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMTOOLKITCORE_IID \
 { 0x1cab9340, 0xc122, 0x11d2, \
    {0x81, 0xb2, 0x00, 0x60, 0x08, 0x3a, 0x0b, 0xcf}} 

class nsIDOMToolkitCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMTOOLKITCORE_IID; return iid; }

  NS_IMETHOD    ShowDialog(const nsString& aUrl, nsIDOMWindow* aParent)=0;

  NS_IMETHOD    ShowWindow(const nsString& aUrl, nsIDOMWindow* aParent)=0;

  NS_IMETHOD    ShowModalDialog(const nsString& aUrl, nsIDOMWindow* aParent)=0;
};


#define NS_DECL_IDOMTOOLKITCORE   \
  NS_IMETHOD    ShowDialog(const nsString& aUrl, nsIDOMWindow* aParent);  \
  NS_IMETHOD    ShowWindow(const nsString& aUrl, nsIDOMWindow* aParent);  \
  NS_IMETHOD    ShowModalDialog(const nsString& aUrl, nsIDOMWindow* aParent);  \



#define NS_FORWARD_IDOMTOOLKITCORE(_to)  \
  NS_IMETHOD    ShowDialog(const nsString& aUrl, nsIDOMWindow* aParent) { return _to##ShowDialog(aUrl, aParent); }  \
  NS_IMETHOD    ShowWindow(const nsString& aUrl, nsIDOMWindow* aParent) { return _to##ShowWindow(aUrl, aParent); }  \
  NS_IMETHOD    ShowModalDialog(const nsString& aUrl, nsIDOMWindow* aParent) { return _to##ShowModalDialog(aUrl, aParent); }  \


extern "C" NS_DOM nsresult NS_InitToolkitCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptToolkitCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMToolkitCore_h__

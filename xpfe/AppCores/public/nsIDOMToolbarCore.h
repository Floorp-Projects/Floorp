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

#ifndef nsIDOMToolbarCore_h__
#define nsIDOMToolbarCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMTOOLBARCORE_IID \
 { 0xbf4ae23e, 0xba9b, 0x11d2, \
    {0x96, 0xc4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56}} 

class nsIDOMToolbarCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMTOOLBARCORE_IID; return iid; }

  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetStatus(const nsString& aMsg)=0;
};


#define NS_DECL_IDOMTOOLBARCORE   \
  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetStatus(const nsString& aMsg);  \



#define NS_FORWARD_IDOMTOOLBARCORE(_to)  \
  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin) { return _to##SetWindow(aWin); }  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin) { return _to##SetWebShellWindow(aWin); }  \
  NS_IMETHOD    SetStatus(const nsString& aMsg) { return _to##SetStatus(aMsg); }  \


extern nsresult NS_InitToolbarCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptToolbarCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMToolbarCore_h__

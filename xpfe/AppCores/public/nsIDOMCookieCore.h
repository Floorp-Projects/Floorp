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

#ifndef nsIDOMCookieCore_h__
#define nsIDOMCookieCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMCOOKIECORE_IID \
{ 0x7ef693b0, 0x1929, 0x11d3, { 0xab, 0xa8, 0x0, 0x80, 0xc7, 0x87, 0xad, 0x96 } }

class nsIDOMCookieCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMCOOKIECORE_IID; return iid; }

  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin)=0;

  NS_IMETHOD    ChangePanel(const nsString& aUrl)=0;

  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SaveCookie(const nsString& aResults)=0;

  NS_IMETHOD    CancelCookie()=0;

  NS_IMETHOD    GetCookieList(nsString& aReturn)=0;

  NS_IMETHOD    GetPermissionList(nsString& aReturn)=0;
};


#define NS_DECL_IDOMCOOKIECORE   \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin);  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl);  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SaveCookie(const nsString& aResults);  \
  NS_IMETHOD    CancelCookie();  \
  NS_IMETHOD    GetCookieList(nsString& aReturn);  \
  NS_IMETHOD    GetPermissionList(nsString& aReturn);  \



#define NS_FORWARD_IDOMCOOKIECORE(_to)  \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin) { return _to ShowWindow(aCurrentFrontWin); }  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl) { return _to ChangePanel(aUrl); }  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin) { return _to PanelLoaded(aWin); }  \
  NS_IMETHOD    SaveCookie(const nsString& aResults) { return _to SaveCookie(aResults); }  \
  NS_IMETHOD    CancelCookie() { return _to CancelCookie(); }  \
  NS_IMETHOD    GetCookieList(nsString& aReturn) { return _to GetCookieList(aReturn); }  \
  NS_IMETHOD    GetPermissionList(nsString& aReturn) { return _to GetPermissionList(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitCookieCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptCookieCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMCookieCore_h__

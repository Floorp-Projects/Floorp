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

#ifndef nsIDOMSignonCore_h__
#define nsIDOMSignonCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMSIGNONCORE_IID \
 { 0x6b654240, 0xe2d, 0x11d3, \
    { 0xab, 0x9f, 0x0, 0x80, 0xc7, 0x87, 0xad, 0x96 }}

class nsIDOMSignonCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSIGNONCORE_IID; return iid; }

  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin)=0;

  NS_IMETHOD    ChangePanel(const nsString& aUrl)=0;

  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SaveSignon(const nsString& aResults)=0;

  NS_IMETHOD    CancelSignon()=0;
};


#define NS_DECL_IDOMSIGNONCORE   \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin);  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl);  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SaveSignon(const nsString& aResults);  \
  NS_IMETHOD    CancelSignon();  \



#define NS_FORWARD_IDOMSIGNONCORE(_to)  \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin) { return _to ShowWindow(aCurrentFrontWin); }  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl) { return _to ChangePanel(aUrl); }  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin) { return _to PanelLoaded(aWin); }  \
  NS_IMETHOD    SaveSignon(const nsString& aResults) { return _to SaveSignon(aResults); }  \
  NS_IMETHOD    CancelSignon() { return _to CancelSignon(); }  \


extern "C" NS_DOM nsresult NS_InitSignonCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSignonCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSignonCore_h__

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

#ifndef nsIDOMWalletCore_h__
#define nsIDOMWalletCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMWALLETCORE_IID \
 { 0x9d6e2540, 0x108b, 0x11d3,\
    { 0xab, 0xa3, 0x0, 0x80, 0xc7, 0x87, 0xad, 0x96 }};

class nsIDOMWalletCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMWALLETCORE_IID; return iid; }

  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin, nsIDOMWindow* aForm)=0;

  NS_IMETHOD    ChangePanel(const nsString& aUrl)=0;

  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SaveWallet(const nsString& aResults)=0;

  NS_IMETHOD    CancelWallet()=0;

  NS_IMETHOD    GetPrefillList(nsString& aReturn)=0;
};


#define NS_DECL_IDOMWALLETCORE   \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin, nsIDOMWindow* aForm);  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl);  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SaveWallet(const nsString& aResults);  \
  NS_IMETHOD    CancelWallet();  \
  NS_IMETHOD    GetPrefillList(nsString& aReturn);  \



#define NS_FORWARD_IDOMWALLETCORE(_to)  \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin, nsIDOMWindow* aForm) { return _to ShowWindow(aCurrentFrontWin, aForm); }  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl) { return _to ChangePanel(aUrl); }  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin) { return _to PanelLoaded(aWin); }  \
  NS_IMETHOD    SaveWallet(const nsString& aResults) { return _to SaveWallet(aResults); }  \
  NS_IMETHOD    CancelWallet() { return _to CancelWallet(); }  \
  NS_IMETHOD    GetPrefillList(nsString& aReturn) { return _to GetPrefillList(aReturn); }  \


extern "C" NS_DOM nsresult NS_InitWalletCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptWalletCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMWalletCore_h__

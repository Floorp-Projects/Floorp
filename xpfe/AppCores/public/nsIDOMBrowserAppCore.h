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

#ifndef nsIDOMBrowserAppCore_h__
#define nsIDOMBrowserAppCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMBROWSERAPPCORE_IID \
 { 0xb0ffb697, 0xbab4, 0x11d2, \
    {0x96, 0xc4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56}} 

class nsIDOMBrowserAppCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMBROWSERAPPCORE_IID; return iid; }

  NS_IMETHOD    Back()=0;

  NS_IMETHOD    Forward()=0;

  NS_IMETHOD    Reload(PRUint32 aReloadType)=0;

  NS_IMETHOD    Stop()=0;

  NS_IMETHOD    LoadUrl(const nsString& aUrl)=0;

  NS_IMETHOD    LoadInitialPage()=0;

  NS_IMETHOD    BackButtonPopup()=0;

  NS_IMETHOD    ForwardButtonPopup()=0;

  NS_IMETHOD    GotoHistoryIndex(PRInt32 aIndex)=0;

  NS_IMETHOD    WalletPreview(nsIDOMWindow* aWin, nsIDOMWindow* aForm)=0;

  NS_IMETHOD    CookieViewer(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SignonViewer(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    WalletEditor(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    WalletChangePassword()=0;

  NS_IMETHOD    WalletQuickFillin(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    WalletRequestToCapture(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    WalletSamples()=0;

  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    NewWindow()=0;

  NS_IMETHOD    OpenWindow()=0;

  NS_IMETHOD    PrintPreview()=0;

  NS_IMETHOD    Copy()=0;

  NS_IMETHOD    Print()=0;

  NS_IMETHOD    Close()=0;

  NS_IMETHOD    Exit()=0;

  NS_IMETHOD    SelectAll()=0;

  NS_IMETHOD    Find()=0;

  NS_IMETHOD    FindNext()=0;
};


#define NS_DECL_IDOMBROWSERAPPCORE   \
  NS_IMETHOD    Back();  \
  NS_IMETHOD    Forward();  \
  NS_IMETHOD    Reload(PRUint32 aReloadType);  \
  NS_IMETHOD    Stop();  \
  NS_IMETHOD    LoadUrl(const nsString& aUrl);  \
  NS_IMETHOD    LoadInitialPage();  \
  NS_IMETHOD    BackButtonPopup();  \
  NS_IMETHOD    ForwardButtonPopup();  \
  NS_IMETHOD    GotoHistoryIndex(PRInt32 aIndex);  \
  NS_IMETHOD    WalletPreview(nsIDOMWindow* aWin, nsIDOMWindow* aForm);  \
  NS_IMETHOD    CookieViewer(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SignonViewer(nsIDOMWindow* aWin);  \
  NS_IMETHOD    WalletEditor(nsIDOMWindow* aWin);  \
  NS_IMETHOD    WalletChangePassword();  \
  NS_IMETHOD    WalletQuickFillin(nsIDOMWindow* aWin);  \
  NS_IMETHOD    WalletRequestToCapture(nsIDOMWindow* aWin);  \
  NS_IMETHOD    WalletSamples();  \
  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);  \
  NS_IMETHOD    NewWindow();  \
  NS_IMETHOD    OpenWindow();  \
  NS_IMETHOD    PrintPreview();  \
  NS_IMETHOD    Copy();  \
  NS_IMETHOD    Print();  \
  NS_IMETHOD    Close();  \
  NS_IMETHOD    Exit();  \
  NS_IMETHOD    SelectAll();  \
  NS_IMETHOD    Find();  \
  NS_IMETHOD    FindNext();  \



#define NS_FORWARD_IDOMBROWSERAPPCORE(_to)  \
  NS_IMETHOD    Back() { return _to Back(); }  \
  NS_IMETHOD    Forward() { return _to Forward(); }  \
  NS_IMETHOD    Reload(PRUint32 aReloadType) { return _to Reload(aReloadType); }  \
  NS_IMETHOD    Stop() { return _to Stop(); }  \
  NS_IMETHOD    LoadUrl(const nsString& aUrl) { return _to LoadUrl(aUrl); }  \
  NS_IMETHOD    LoadInitialPage() { return _to LoadInitialPage(); }  \
  NS_IMETHOD    BackButtonPopup() { return _to BackButtonPopup(); }  \
  NS_IMETHOD    ForwardButtonPopup() { return _to ForwardButtonPopup(); }  \
  NS_IMETHOD    GotoHistoryIndex(PRInt32 aIndex) { return _to GotoHistoryIndex(aIndex); }  \
  NS_IMETHOD    WalletPreview(nsIDOMWindow* aWin, nsIDOMWindow* aForm) { return _to WalletPreview(aWin, aForm); }  \
  NS_IMETHOD    CookieViewer(nsIDOMWindow* aWin) { return _to CookieViewer(aWin); }  \
  NS_IMETHOD    SignonViewer(nsIDOMWindow* aWin) { return _to SignonViewer(aWin); }  \
  NS_IMETHOD    WalletEditor(nsIDOMWindow* aWin) { return _to WalletEditor(aWin); }  \
  NS_IMETHOD    WalletChangePassword() { return _to WalletChangePassword(); }  \
  NS_IMETHOD    WalletQuickFillin(nsIDOMWindow* aWin) { return _to WalletQuickFillin(aWin); }  \
  NS_IMETHOD    WalletRequestToCapture(nsIDOMWindow* aWin) { return _to WalletRequestToCapture(aWin); }  \
  NS_IMETHOD    WalletSamples() { return _to WalletSamples(); }  \
  NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin) { return _to SetToolbarWindow(aWin); }  \
  NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin) { return _to SetContentWindow(aWin); }  \
  NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin) { return _to SetWebShellWindow(aWin); }  \
  NS_IMETHOD    NewWindow() { return _to NewWindow(); }  \
  NS_IMETHOD    OpenWindow() { return _to OpenWindow(); }  \
  NS_IMETHOD    PrintPreview() { return _to PrintPreview(); }  \
  NS_IMETHOD    Copy() { return _to Copy(); }  \
  NS_IMETHOD    Print() { return _to Print(); }  \
  NS_IMETHOD    Close() { return _to Close(); }  \
  NS_IMETHOD    Exit() { return _to Exit(); }  \
  NS_IMETHOD    SelectAll() { return _to SelectAll(); }  \
  NS_IMETHOD    Find() { return _to Find(); }  \
  NS_IMETHOD    FindNext() { return _to FindNext(); }  \


extern "C" NS_DOM nsresult NS_InitBrowserAppCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptBrowserAppCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMBrowserAppCore_h__

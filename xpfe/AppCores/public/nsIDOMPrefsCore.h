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

#ifndef nsIDOMPrefsCore_h__
#define nsIDOMPrefsCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMPREFSCORE_IID \
 { 0x55af8384, 0xe11e, 0x11d2, \
    {0x91, 0x5f, 0xa0, 0x53, 0xf0, 0x5f, 0xf7, 0xbc}} 

class nsIDOMPrefsCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMPREFSCORE_IID; return iid; }

  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin)=0;

  NS_IMETHOD    ChangePanel(const nsString& aUrl)=0;

  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin)=0;

  NS_IMETHOD    SavePrefs()=0;

  NS_IMETHOD    CancelPrefs()=0;
};


#define NS_DECL_IDOMPREFSCORE   \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin);  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl);  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin);  \
  NS_IMETHOD    SavePrefs();  \
  NS_IMETHOD    CancelPrefs();  \



#define NS_FORWARD_IDOMPREFSCORE(_to)  \
  NS_IMETHOD    ShowWindow(nsIDOMWindow* aCurrentFrontWin) { return _to##ShowWindow(aCurrentFrontWin); }  \
  NS_IMETHOD    ChangePanel(const nsString& aUrl) { return _to##ChangePanel(aUrl); }  \
  NS_IMETHOD    PanelLoaded(nsIDOMWindow* aWin) { return _to##PanelLoaded(aWin); }  \
  NS_IMETHOD    SavePrefs() { return _to##SavePrefs(); }  \
  NS_IMETHOD    CancelPrefs() { return _to##CancelPrefs(); }  \


extern "C" NS_DOM nsresult NS_InitPrefsCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptPrefsCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMPrefsCore_h__

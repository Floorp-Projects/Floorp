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

#ifndef nsIDOMMailCore_h__
#define nsIDOMMailCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMWindow;

#define NS_IDOMMAILCORE_IID \
 { 0x18c2f980, 0xb09f, 0x11d2, \
    {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMMailCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMMAILCORE_IID; return iid; }

  NS_IMETHOD    SendMail(const nsString& aAddrTo, const nsString& aSubject, const nsString& aMsg)=0;

  NS_IMETHOD    MailCompleteCallback(const nsString& aScript)=0;

  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin)=0;
};


#define NS_DECL_IDOMMAILCORE   \
  NS_IMETHOD    SendMail(const nsString& aAddrTo, const nsString& aSubject, const nsString& aMsg);  \
  NS_IMETHOD    MailCompleteCallback(const nsString& aScript);  \
  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin);  \



#define NS_FORWARD_IDOMMAILCORE(_to)  \
  NS_IMETHOD    SendMail(const nsString& aAddrTo, const nsString& aSubject, const nsString& aMsg) { return _to##SendMail(aAddrTo, aSubject, aMsg); }  \
  NS_IMETHOD    MailCompleteCallback(const nsString& aScript) { return _to##MailCompleteCallback(aScript); }  \
  NS_IMETHOD    SetWindow(nsIDOMWindow* aWin) { return _to##SetWindow(aWin); }  \


extern nsresult NS_InitMailCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptMailCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMMailCore_h__

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

#ifndef nsIDOMAppCoresManager_h__
#define nsIDOMAppCoresManager_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMBaseAppCore;

#define NS_IDOMAPPCORESMANAGER_IID \
 { 0x18c2f981, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMAppCoresManager : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMAPPCORESMANAGER_IID; return iid; }

  NS_IMETHOD    Startup()=0;

  NS_IMETHOD    Shutdown()=0;

  NS_IMETHOD    Add(nsIDOMBaseAppCore* aAppcore)=0;

  NS_IMETHOD    Remove(nsIDOMBaseAppCore* aAppcore)=0;

  NS_IMETHOD    Find(const nsString& aId, nsIDOMBaseAppCore** aReturn)=0;
};


#define NS_DECL_IDOMAPPCORESMANAGER   \
  NS_IMETHOD    Startup();  \
  NS_IMETHOD    Shutdown();  \
  NS_IMETHOD    Add(nsIDOMBaseAppCore* aAppcore);  \
  NS_IMETHOD    Remove(nsIDOMBaseAppCore* aAppcore);  \
  NS_IMETHOD    Find(const nsString& aId, nsIDOMBaseAppCore** aReturn);  \



#define NS_FORWARD_IDOMAPPCORESMANAGER(_to)  \
  NS_IMETHOD    Startup() { return _to##Startup(); }  \
  NS_IMETHOD    Shutdown() { return _to##Shutdown(); }  \
  NS_IMETHOD    Add(nsIDOMBaseAppCore* aAppcore) { return _to##Add(aAppcore); }  \
  NS_IMETHOD    Remove(nsIDOMBaseAppCore* aAppcore) { return _to##Remove(aAppcore); }  \
  NS_IMETHOD    Find(const nsString& aId, nsIDOMBaseAppCore** aReturn) { return _to##Find(aId, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitAppCoresManagerClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptAppCoresManager(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMAppCoresManager_h__

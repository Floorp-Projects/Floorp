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

#ifndef nsIDOMXPConnectFactory_h__
#define nsIDOMXPConnectFactory_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

#define NS_IDOMXPCONNECTFACTORY_IID \
 { 0x12221f90, 0xcd53, 0x11d2, \
  {0x92, 0xb6, 0x00, 0x10, 0x5a, 0x1b, 0x0d, 0x64}} 

class nsIDOMXPConnectFactory : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMXPCONNECTFACTORY_IID; return iid; }

  NS_IMETHOD    CreateInstance(const nsString& aProgID, nsISupports** aReturn)=0;
};


#define NS_DECL_IDOMXPCONNECTFACTORY   \
  NS_IMETHOD    CreateInstance(const nsString& aProgID, nsISupports** aReturn);  \



#define NS_FORWARD_IDOMXPCONNECTFACTORY(_to)  \
  NS_IMETHOD    CreateInstance(const nsString& aProgID, nsISupports** aReturn) { return _to##CreateInstance(aProgID, aReturn); }  \


extern "C" NS_APPSHELL nsresult NS_InitXPConnectFactoryClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_APPSHELL nsresult NS_NewScriptXPConnectFactory(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXPConnectFactory_h__

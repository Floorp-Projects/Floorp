/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXPCONNECTFACTORY_IID; return iid; }

  NS_IMETHOD    CreateInstance(const nsString& aContractID, nsISupports** aReturn)=0;
};


#define NS_DECL_IDOMXPCONNECTFACTORY   \
  NS_IMETHOD    CreateInstance(const nsString& aContractID, nsISupports** aReturn);  \



#define NS_FORWARD_IDOMXPCONNECTFACTORY(_to)  \
  NS_IMETHOD    CreateInstance(const nsString& aContractID, nsISupports** aReturn) { return _to##CreateInstance(aContractID, aReturn); }  \


extern "C" NS_APPSHELL nsresult NS_InitXPConnectFactoryClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_APPSHELL nsresult NS_NewScriptXPConnectFactory(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXPConnectFactory_h__

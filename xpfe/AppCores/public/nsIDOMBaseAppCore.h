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

#ifndef nsIDOMBaseAppCore_h__
#define nsIDOMBaseAppCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMBASEAPPCORE_IID \
 { 0xbe5c13bd, 0xba9f, 0x11d2, \
    {0x96, 0xc4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56}} 

class nsIDOMBaseAppCore : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMBASEAPPCORE_IID; return iid; }

  NS_IMETHOD    GetId(nsString& aId)=0;

  NS_IMETHOD    Init(const nsString& aId)=0;

  NS_IMETHOD    SetDocumentCharset(const nsString& aCharset)=0;
};


#define NS_DECL_IDOMBASEAPPCORE   \
  NS_IMETHOD    GetId(nsString& aId);  \
  NS_IMETHOD    Init(const nsString& aId);  \
  NS_IMETHOD    SetDocumentCharset(const nsString& aCharset);  \



#define NS_FORWARD_IDOMBASEAPPCORE(_to)  \
  NS_IMETHOD    GetId(nsString& aId) { return _to GetId(aId); } \
  NS_IMETHOD    Init(const nsString& aId) { return _to Init(aId); }  \
  NS_IMETHOD    SetDocumentCharset(const nsString& aCharset) { return _to SetDocumentCharset(aCharset); }  \


extern "C" NS_DOM nsresult NS_InitBaseAppCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptBaseAppCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMBaseAppCore_h__

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

#ifndef nsIDOMRDFCore_h__
#define nsIDOMRDFCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMNode;

#define NS_IDOMRDFCORE_IID \
 { 0xdf365f00, 0xcc53, 0x11d2, \
  { 0x8f, 0xdf, 0x0, 0x08, 0xc7, 0x0a, 0xdc, 0x7b } } 

class nsIDOMRDFCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMRDFCORE_IID; return iid; }

  NS_IMETHOD    DoSort(nsIDOMNode* aNode, const nsString& aSortResource)=0;
};


#define NS_DECL_IDOMRDFCORE   \
  NS_IMETHOD    DoSort(nsIDOMNode* aNode, const nsString& aSortResource);  \



#define NS_FORWARD_IDOMRDFCORE(_to)  \
  NS_IMETHOD    DoSort(nsIDOMNode* aNode, const nsString& aSortResource) { return _to##DoSort(aNode, aSortResource); }  \


extern "C" NS_DOM nsresult NS_InitRDFCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptRDFCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMRDFCore_h__

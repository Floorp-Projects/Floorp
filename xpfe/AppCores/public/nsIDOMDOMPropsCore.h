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

#ifndef nsIDOMDOMPropsCore_h__
#define nsIDOMDOMPropsCore_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMBaseAppCore.h"

class nsIDOMNode;
class nsIDOMWindow;

#define NS_IDOMDOMPROPSCORE_IID \
 { 0xd23ec5d6, 0xe91e, 0x11d2, \
    {0xaa, 0xd6, 0x0, 0x80, 0x5f, 0x8a, 0x49, 0x5}} 

class nsIDOMDOMPropsCore : public nsIDOMBaseAppCore {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMDOMPROPSCORE_IID; return iid; }

  NS_IMETHOD    GetNode(nsIDOMNode** aNode)=0;
  NS_IMETHOD    SetNode(nsIDOMNode* aNode)=0;

  NS_IMETHOD    ShowProperties(const nsString& aUrl, nsIDOMWindow* aParent, nsIDOMNode* aNode)=0;

  NS_IMETHOD    Commit()=0;

  NS_IMETHOD    Cancel()=0;
};


#define NS_DECL_IDOMDOMPROPSCORE   \
  NS_IMETHOD    GetNode(nsIDOMNode** aNode);  \
  NS_IMETHOD    SetNode(nsIDOMNode* aNode);  \
  NS_IMETHOD    ShowProperties(const nsString& aUrl, nsIDOMWindow* aParent, nsIDOMNode* aNode);  \
  NS_IMETHOD    Commit();  \
  NS_IMETHOD    Cancel();  \



#define NS_FORWARD_IDOMDOMPROPSCORE(_to)  \
  NS_IMETHOD    GetNode(nsIDOMNode** aNode) { return _to##GetNode(aNode); } \
  NS_IMETHOD    SetNode(nsIDOMNode* aNode) { return _to##SetNode(aNode); } \
  NS_IMETHOD    ShowProperties(const nsString& aUrl, nsIDOMWindow* aParent, nsIDOMNode* aNode) { return _to##ShowProperties(aUrl, aParent, aNode); }  \
  NS_IMETHOD    Commit() { return _to##Commit(); }  \
  NS_IMETHOD    Cancel() { return _to##Cancel(); }  \


extern "C" NS_DOM nsresult NS_InitDOMPropsCoreClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptDOMPropsCore(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMDOMPropsCore_h__

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

#ifndef nsIDOMXULNode_h__
#define nsIDOMXULNode_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMNode;

#define NS_IDOMXULNODE_IID \
 { 0x574ed81, 0xc088, 0x11d2, \
  { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } } 

class nsIDOMXULNode : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMXULNODE_IID; return iid; }

  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode)=0;

  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode)=0;

  NS_IMETHOD    DoCommand()=0;
};


#define NS_DECL_IDOMXULNODE   \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode);  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode);  \
  NS_IMETHOD    DoCommand();  \



#define NS_FORWARD_IDOMXULNODE(_to)  \
  NS_IMETHOD    AddBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode) { return _to##AddBroadcastListener(aAttr, aNode); }  \
  NS_IMETHOD    RemoveBroadcastListener(const nsString& aAttr, nsIDOMNode* aNode) { return _to##RemoveBroadcastListener(aAttr, aNode); }  \
  NS_IMETHOD    DoCommand() { return _to##DoCommand(); }  \


extern nsresult NS_InitXULNodeClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULNode(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULNode_h__

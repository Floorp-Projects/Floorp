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

#ifndef nsIDOMXULTreeElement_h__
#define nsIDOMXULTreeElement_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMXULElement.h"

class nsIRDFCompositeDataSource;

#define NS_IDOMXULTREEELEMENT_IID \
 { 0xa6cf90ec, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} } 

class nsIDOMXULTreeElement : public nsIDOMXULElement {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULTREEELEMENT_IID; return iid; }

  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase)=0;
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase)=0;
};


#define NS_DECL_IDOMXULTREEELEMENT   \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase);  \
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase);  \



#define NS_FORWARD_IDOMXULTREEELEMENT(_to)  \
  NS_IMETHOD    GetDatabase(nsIRDFCompositeDataSource** aDatabase) { return _to GetDatabase(aDatabase); } \
  NS_IMETHOD    SetDatabase(nsIRDFCompositeDataSource* aDatabase) { return _to SetDatabase(aDatabase); } \


extern "C" NS_DOM nsresult NS_InitXULTreeElementClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULTreeElement(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULTreeElement_h__

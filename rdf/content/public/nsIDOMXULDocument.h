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

#ifndef nsIDOMXULDocument_h__
#define nsIDOMXULDocument_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"

class nsIRDFService;
class nsIDOMElement;
class nsIDOMNodeList;

#define NS_IDOMXULDOCUMENT_IID \
 { 0x17ddd8c0, 0xc5f8, 0x11d2, \
        { 0xa6, 0xae, 0x0, 0x10, 0x4b, 0xde, 0x60, 0x48 } } 

class nsIDOMXULDocument : public nsIDOMDocument {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMXULDOCUMENT_IID; return iid; }

  NS_IMETHOD    GetRdf(nsIRDFService** aRdf)=0;

  NS_IMETHOD    GetElementById(const nsString& aId, nsIDOMElement** aReturn)=0;

  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn)=0;
};


#define NS_DECL_IDOMXULDOCUMENT   \
  NS_IMETHOD    GetRdf(nsIRDFService** aRdf);  \
  NS_IMETHOD    GetElementById(const nsString& aId, nsIDOMElement** aReturn);  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn);  \



#define NS_FORWARD_IDOMXULDOCUMENT(_to)  \
  NS_IMETHOD    GetRdf(nsIRDFService** aRdf) { return _to GetRdf(aRdf); } \
  NS_IMETHOD    GetElementById(const nsString& aId, nsIDOMElement** aReturn) { return _to GetElementById(aId, aReturn); }  \
  NS_IMETHOD    GetElementsByAttribute(const nsString& aName, const nsString& aValue, nsIDOMNodeList** aReturn) { return _to GetElementsByAttribute(aName, aValue, aReturn); }  \


extern "C" NS_DOM nsresult NS_InitXULDocumentClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptXULDocument(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMXULDocument_h__

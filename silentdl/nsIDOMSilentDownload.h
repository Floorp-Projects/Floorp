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

#ifndef nsIDOMSilentDownload_h__
#define nsIDOMSilentDownload_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMSilentDownloadTask;

#define NS_IDOMSILENTDOWNLOAD_IID \
 { 0x18c2f981, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMSilentDownload : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMSILENTDOWNLOAD_IID; return iid; }

  NS_IMETHOD    GetByteRange(PRInt32* aByteRange)=0;
  NS_IMETHOD    SetByteRange(PRInt32 aByteRange)=0;

  NS_IMETHOD    GetInterval(PRInt32* aInterval)=0;
  NS_IMETHOD    SetInterval(PRInt32 aInterval)=0;

  NS_IMETHOD    Startup()=0;

  NS_IMETHOD    Shutdown()=0;

  NS_IMETHOD    Add(nsIDOMSilentDownloadTask* aTask)=0;

  NS_IMETHOD    Remove(nsIDOMSilentDownloadTask* aTask)=0;

  NS_IMETHOD    Find(const nsString& aId, nsIDOMSilentDownloadTask** aReturn)=0;
};


#define NS_DECL_IDOMSILENTDOWNLOAD   \
  NS_IMETHOD    GetByteRange(PRInt32* aByteRange);  \
  NS_IMETHOD    SetByteRange(PRInt32 aByteRange);  \
  NS_IMETHOD    GetInterval(PRInt32* aInterval);  \
  NS_IMETHOD    SetInterval(PRInt32 aInterval);  \
  NS_IMETHOD    Startup();  \
  NS_IMETHOD    Shutdown();  \
  NS_IMETHOD    Add(nsIDOMSilentDownloadTask* aTask);  \
  NS_IMETHOD    Remove(nsIDOMSilentDownloadTask* aTask);  \
  NS_IMETHOD    Find(const nsString& aId, nsIDOMSilentDownloadTask** aReturn);  \



#define NS_FORWARD_IDOMSILENTDOWNLOAD(_to)  \
  NS_IMETHOD    GetByteRange(PRInt32* aByteRange) { return _to##GetByteRange(aByteRange); } \
  NS_IMETHOD    SetByteRange(PRInt32 aByteRange) { return _to##SetByteRange(aByteRange); } \
  NS_IMETHOD    GetInterval(PRInt32* aInterval) { return _to##GetInterval(aInterval); } \
  NS_IMETHOD    SetInterval(PRInt32 aInterval) { return _to##SetInterval(aInterval); } \
  NS_IMETHOD    Startup() { return _to##Startup(); }  \
  NS_IMETHOD    Shutdown() { return _to##Shutdown(); }  \
  NS_IMETHOD    Add(nsIDOMSilentDownloadTask* aTask) { return _to##Add(aTask); }  \
  NS_IMETHOD    Remove(nsIDOMSilentDownloadTask* aTask) { return _to##Remove(aTask); }  \
  NS_IMETHOD    Find(const nsString& aId, nsIDOMSilentDownloadTask** aReturn) { return _to##Find(aId, aReturn); }  \


extern nsresult NS_InitSilentDownloadClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSilentDownload(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSilentDownload_h__

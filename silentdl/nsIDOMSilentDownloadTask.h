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

#ifndef nsIDOMSilentDownloadTask_h__
#define nsIDOMSilentDownloadTask_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"


#define NS_IDOMSILENTDOWNLOADTASK_IID \
 { 0x18c2f980, 0xb09f, 0x11d2, \
    {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMSilentDownloadTask : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMSILENTDOWNLOADTASK_IID; return iid; }
  enum {
    SDL_NOT_INITED = -2,
    SDL_NOT_ADDED = -1,
    SDL_STARTED = 0,
    SDL_SUSPENDED = 1,
    SDL_COMPLETED = 2,
    SDL_DOWNLOADING_NOW = 3,
    SDL_ERROR = 4
  };

  NS_IMETHOD    GetId(nsString& aId)=0;

  NS_IMETHOD    GetUrl(nsString& aUrl)=0;

  NS_IMETHOD    GetScript(nsString& aScript)=0;

  NS_IMETHOD    GetState(PRInt32* aState)=0;
  NS_IMETHOD    SetState(PRInt32 aState)=0;

  NS_IMETHOD    GetErrorMsg(nsString& aErrorMsg)=0;
  NS_IMETHOD    SetErrorMsg(const nsString& aErrorMsg)=0;

  NS_IMETHOD    GetNextByte(PRInt32* aNextByte)=0;
  NS_IMETHOD    SetNextByte(PRInt32 aNextByte)=0;

  NS_IMETHOD    GetOutFile(nsString& aOutFile)=0;

  NS_IMETHOD    Init(const nsString& aId, const nsString& aUrl, const nsString& aScript)=0;

  NS_IMETHOD    Remove()=0;

  NS_IMETHOD    Suspend()=0;

  NS_IMETHOD    Resume()=0;

  NS_IMETHOD    DownloadNow()=0;

  NS_IMETHOD    DownloadSelf(PRInt32 aRange)=0;
};


#define NS_DECL_IDOMSILENTDOWNLOADTASK   \
  NS_IMETHOD    GetId(nsString& aId);  \
  NS_IMETHOD    GetUrl(nsString& aUrl);  \
  NS_IMETHOD    GetScript(nsString& aScript);  \
  NS_IMETHOD    GetState(PRInt32* aState);  \
  NS_IMETHOD    SetState(PRInt32 aState);  \
  NS_IMETHOD    GetErrorMsg(nsString& aErrorMsg);  \
  NS_IMETHOD    SetErrorMsg(const nsString& aErrorMsg);  \
  NS_IMETHOD    GetNextByte(PRInt32* aNextByte);  \
  NS_IMETHOD    SetNextByte(PRInt32 aNextByte);  \
  NS_IMETHOD    GetOutFile(nsString& aOutFile);  \
  NS_IMETHOD    Init(const nsString& aId, const nsString& aUrl, const nsString& aScript);  \
  NS_IMETHOD    Remove();  \
  NS_IMETHOD    Suspend();  \
  NS_IMETHOD    Resume();  \
  NS_IMETHOD    DownloadNow();  \
  NS_IMETHOD    DownloadSelf(PRInt32 aRange);  \



#define NS_FORWARD_IDOMSILENTDOWNLOADTASK(_to)  \
  NS_IMETHOD    GetId(nsString& aId) { return _to##GetId(aId); } \
  NS_IMETHOD    GetUrl(nsString& aUrl) { return _to##GetUrl(aUrl); } \
  NS_IMETHOD    GetScript(nsString& aScript) { return _to##GetScript(aScript); } \
  NS_IMETHOD    GetState(PRInt32* aState) { return _to##GetState(aState); } \
  NS_IMETHOD    SetState(PRInt32 aState) { return _to##SetState(aState); } \
  NS_IMETHOD    GetErrorMsg(nsString& aErrorMsg) { return _to##GetErrorMsg(aErrorMsg); } \
  NS_IMETHOD    SetErrorMsg(const nsString& aErrorMsg) { return _to##SetErrorMsg(aErrorMsg); } \
  NS_IMETHOD    GetNextByte(PRInt32* aNextByte) { return _to##GetNextByte(aNextByte); } \
  NS_IMETHOD    SetNextByte(PRInt32 aNextByte) { return _to##SetNextByte(aNextByte); } \
  NS_IMETHOD    GetOutFile(nsString& aOutFile) { return _to##GetOutFile(aOutFile); } \
  NS_IMETHOD    Init(const nsString& aId, const nsString& aUrl, const nsString& aScript) { return _to##Init(aId, aUrl, aScript); }  \
  NS_IMETHOD    Remove() { return _to##Remove(); }  \
  NS_IMETHOD    Suspend() { return _to##Suspend(); }  \
  NS_IMETHOD    Resume() { return _to##Resume(); }  \
  NS_IMETHOD    DownloadNow() { return _to##DownloadNow(); }  \
  NS_IMETHOD    DownloadSelf(PRInt32 aRange) { return _to##DownloadSelf(aRange); }  \


extern nsresult NS_InitSilentDownloadTaskClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptSilentDownloadTask(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMSilentDownloadTask_h__

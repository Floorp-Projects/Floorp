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

#ifndef _nsMsgTest_h__
#define _nsMsgTest_h__

#include "nsIMsgTest.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsIShellInstance.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsRect.h"
#include "nsApplicationManager.h"
#include "nsIContentViewer.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nspr.h"
#include "jsapi.h"
#include "nsIStreamListener.h"

class nsMsgTest : public nsIMsgTest
{
public:
  nsMsgTest();
  ~nsMsgTest();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();

  // nsIAppShell interfaces
  NS_IMETHOD Create(int* argc, char ** argv) ;
  NS_IMETHOD SetDispatchListener(nsDispatchListener* aDispatchListener) ;
  NS_IMETHOD Exit();
  virtual nsresult Run();
  virtual void* GetNativeData(PRUint32 aDataType) ;

  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent)  ;
  NS_IMETHOD GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer) ;

  NS_IMETHOD ReceiveCommand(nsString& aCommand, nsString& aReply);
  NS_IMETHOD StartCommandServer();
  NS_METHOD ParseCommandLine();

private:
  NS_METHOD RegisterFactories();

private:
  nsIShellInstance * mShellInstance ;
  nsString mServer ;
  nsString mFrom ;
  nsString mTo ;
  nsString mDomain ;
  nsString mMessage ;
  nsString mHeader;

};

#endif

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

#ifndef _nsAppTest_h__
#define _nsAppTest_h__

#include <stdio.h>
#include "nsIApplicationShell.h"
#include "nsIShellInstance.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsShellInstance.h"
#include "nsIAppShell.h"

#define NS_IAPPTEST_IID      \
 { 0x5040bd10, 0xdec5, 0x11d1, \
   {0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

/*
 * AppTest Class Declaration
 */

class nsAppTest : public nsIApplicationShell,
		          public nsIAppShell
{
public:
  nsAppTest();
  ~nsAppTest();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  virtual nsresult  Run();

  virtual void Create(int* argc, char ** argv) ;
  virtual void SetDispatchListener(nsDispatchListener* aDispatchListener) ;
  virtual void Exit();
  virtual void* GetNativeData(PRUint32 aDataType) ;
  NS_IMETHOD_(nsEventStatus) HandleEvent(nsGUIEvent *aEvent)  ;
  NS_IMETHOD GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer) ;

public:
  nsIShellInstance * mShellInstance;

};

/*
 * AppTestFactory Class Declaration
 */

class nsAppTestFactory : public nsIFactory {

public:
  nsAppTestFactory();
  ~nsAppTestFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports * aOuter,
                            const nsIID &aIID,
                            void ** aResult);

  NS_IMETHOD LockFactory(PRBool aLock);


};


#endif

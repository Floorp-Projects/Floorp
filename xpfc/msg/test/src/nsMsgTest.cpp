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

#include "jdefines.h"
#include "nsMsgTest.h"
#include "nsxpfcCIID.h"
#include "nsIAppShell.h"
#include "nsMsgTestCIID.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsMsgTestFactory.h"
#include "nsFont.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nspr.h"
#include "plgetopt.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "jsapi.h"

#ifdef NS_UNIX
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"
#endif


static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);

/*
 * External refs
 */

#ifdef NS_UNIX
extern XtAppContext app_context;
extern Widget topLevel;
#endif

/*
 * Local Globals
 */

nsIMsgTest * gShell = nsnull;
nsIID kIXPCOMApplicationShellCID = NS_MSGTEST_CID ; 

/*
 * Useage
 */

static nsresult Usage(void)
{
  return 1; 
} 


// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{

  nsresult res = nsRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsMsgTestFactory(kIXPCOMApplicationShellCID),
                                               PR_FALSE) ;

  return res;
}


/*
 * nsMsgTest Definition
 */

nsMsgTest::nsMsgTest()
{
  NS_INIT_REFCNT();
  mShellInstance  = nsnull ;
  gShell = this;

}

/*
 * nsMsgTest dtor
 */
nsMsgTest::~nsMsgTest()
{
}

/*
 * nsISupports stuff
 */

nsresult nsMsgTest::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIApplicationShell *)(this);                                        
  }
  else if(aIID.Equals(kIXPCOMApplicationShellCID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIMsgTest*)(this);                                        
  }
  else if(aIID.Equals(kIAppShellIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIAppShell*)(this);                                        
  }
  else if(aIID.Equals(kIStreamListenerIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }  
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsMsgTest)
NS_IMPL_RELEASE(nsMsgTest)

/*
 * Init
 */

nsresult nsMsgTest::Init()
{

  /*
   * Register class factrories needed for application
   */

  RegisterFactories() ;

  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance) ;

  return res ;
}


nsresult nsMsgTest::Create(int* argc, char ** argv)
{
  return NS_OK;
}

nsresult nsMsgTest::Exit()
{
  return NS_OK;
}

nsresult nsMsgTest::Run()
{
  mShellInstance->Run();
  return NS_OK;
}

nsresult nsMsgTest::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  return NS_OK;
}

void* nsMsgTest::GetNativeData(PRUint32 aDataType)
{
#ifdef XP_UNIX
  if (aDataType == NS_NATIVE_SHELL)
    return topLevel;

  return nsnull;
#else
  return (mShellInstance->GetApplicationWidget());
#endif

}

nsresult nsMsgTest::RegisterFactories()
{
#ifdef NS_WIN32
  #define XPFC_DLL  "xpfc10.dll"
#else
  #define XPFC_DLL "libxpfc10.so"
#endif

  // register graphics classes
  static NS_DEFINE_IID(kCXPFCCanvasCID, NS_XPFC_CANVAS_CID);

  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);

  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCXPFCObserverManagerCID,   NS_XPFC_OBSERVERMANAGER_CID);
  nsRepository::RegisterFactory(kCXPFCObserverManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult nsMsgTest::GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer)
{
  return NS_OK;
}


nsEventStatus nsMsgTest::HandleEvent(nsGUIEvent *aEvent)
{

    nsEventStatus result = nsEventStatus_eConsumeNoDefault;

    switch(aEvent->message) {

        case NS_CREATE:
        {
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_SIZE:
        {
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_DESTROY:
        {
          mShellInstance->ExitApplication() ;
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;
    }

    return nsEventStatus_eIgnore; 
}


nsresult nsMsgTest::StartCommandServer()
{
  return NS_OK;
}

nsresult nsMsgTest::ReceiveCommand(nsString& aCommand, nsString& aReply)
{
  return NS_OK;
}

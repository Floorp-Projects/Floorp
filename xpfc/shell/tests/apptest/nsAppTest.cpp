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
#include "nscore.h"
#include "nsAppTest.h"
#include "nsApplicationManager.h"

#include "nsString.h"
#include "nsFont.h"

// TODO: Put this in nsIShellInstance....

#ifdef NS_UNIX
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

extern XtAppContext app_context;
extern Widget topLevel;
#endif

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_IAPPTEST_IID ; 

// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{
  nsresult res = nsRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsAppTestFactory(),
                                               PR_FALSE) ;

  return res;
}

/*
 * nsAppTest Definition
 */

nsAppTest::nsAppTest()
{
  NS_INIT_REFCNT();
}

nsAppTest::~nsAppTest()
{
}

static NS_DEFINE_IID(kIAppTestIID, NS_IAPPTEST_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);


nsresult nsAppTest::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIApplicationShell*)(this);                                        
  }
  else if(aIID.Equals(kIXPCOMApplicationShellCID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsAppTest*)(this);                                        
  }
  else if(aIID.Equals(kIAppTestIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsAppTest*)(this);                                        
  }
  else if(aIID.Equals(kIAppShellIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIAppShell*)(this);                                        
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsAppTest)
NS_IMPL_RELEASE(nsAppTest)

nsresult nsAppTest::Init()
{

  
  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance) ;

  if (NS_OK != res)
    return res ;

  nsRect aRect(100,100,540, 380) ;

  nsIAppShell * appshell ;

  res = QueryInterface(kIAppShellIID,(void**)&appshell);

  if (NS_OK != res)
    return res ;


  mShellInstance->CreateApplicationWindow(appshell,aRect);
  mShellInstance->ShowApplicationWindow(PR_TRUE) ;

  return res ;

}

void* nsAppTest::GetNativeData(PRUint32 aDataType)
{
#ifdef XP_UNIX
  if (aDataType == NS_NATIVE_SHELL)
    return topLevel;

  return nsnull;
#else
  return (mShellInstance->GetApplicationWidget());
#endif

}

void nsAppTest::Create(int* argc, char ** argv)
{
  return;
}
void nsAppTest::Exit()
{
  return;
}
void nsAppTest::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  return ;
}

nsresult nsAppTest::Run()
{
  return (mShellInstance->Run());
}

/*
 * nsAppTestFactory Definition
 */

nsAppTestFactory::nsAppTestFactory()
{    
  NS_INIT_REFCNT();
}

nsAppTestFactory::~nsAppTestFactory()
{    
}

NS_DEFINE_IID(kIAppTestFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsAppTestFactory,kIAppTestFactoryIID);


nsresult nsAppTestFactory::CreateInstance(nsISupports * aOuter,
                                                   const nsIID &aIID,
                                                   void ** aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL ;  

  nsISupports * inst = (nsISupports *)(nsIApplicationShell*)new nsAppTest() ;

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    delete inst ;
  }

  return res;

}

nsresult nsAppTestFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}


nsEventStatus nsAppTest::HandleEvent(nsGUIEvent *aEvent)
{
    nsEventStatus result = nsEventStatus_eConsumeNoDefault;

    switch(aEvent->message) {

        case NS_CREATE:
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

        case NS_PAINT:
        {
          nsRect aRect(0,0,540, 380) ;
	      // paint the background
	      nsString aString("Hello World!\n");
	      nsIRenderingContext * rndctx = ((nsPaintEvent*)aEvent)->renderingContext;
          nsDrawingSurface ds;

          ds = rndctx->CreateDrawingSurface(&aRect);
          rndctx->SelectOffScreenDrawingSurface(ds);

          nsFont font("serif", NS_FONT_STYLE_NORMAL,
		          NS_FONT_VARIANT_SMALL_CAPS,
		          NS_FONT_WEIGHT_NORMAL,
		          0,
		          12);

          rndctx->SetFont(font);



	      rndctx->SetColor(NS_RGB(0, 0, 255));
          rndctx->FillRect(aRect);
	      
	      rndctx->SetColor(NS_RGB(255, 0, 0));
	      rndctx->DrawString(aString, 50, 50, 100);
	        
          rndctx->CopyOffScreenBits(aRect);
          rndctx->DestroyDrawingSurface(ds);
    	  return nsEventStatus_eConsumeNoDefault;
        }
        break;

    }

    return nsEventStatus_eIgnore; 
}




nsresult nsAppTest::GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer)
{
  return NS_OK;
}

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

#ifdef NS_WIN32
#include "windows.h"
#endif

nsEventStatus HandleEventApplication(nsGUIEvent *aEvent);

// All Applications must specify this *special* application CID
// to their own unique IID.
nsIID kIXPCOMApplicationShellCID = NS_IAPPTEST_IID ; 

// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{
  nsresult res = NSRepository::RegisterFactory(kIXPCOMApplicationShellCID,
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

NS_DEFINE_IID(kIAppTestIID, NS_IAPPTEST_IID);
NS_IMPL_ISUPPORTS(nsAppTest,kIAppTestIID);

nsresult nsAppTest::Init()
{

  
  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance) ;

  if (NS_OK != res)
    return res ;

  nsRect aRect(100,100,540, 380) ;

  mShellInstance->CreateApplicationWindow(aRect, HandleEventApplication);
  mShellInstance->ShowApplicationWindow(PR_TRUE) ;

  return res ;

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

  nsISupports * inst = new nsAppTest() ;

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



nsEventStatus PR_CALLBACK HandleEventApplication(nsGUIEvent *aEvent)
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
          //mShellInstance->ExitApplication() ;
#ifdef NS_WIN32
          PostQuitMessage(0);
#endif
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_PAINT:
        {

	  // paint the background
	  nsString aString("Hello World!\n");
	  nsIRenderingContext * rndctx = ((nsPaintEvent*)aEvent)->renderingContext;
	  //rndctx->SetColor(aEvent->widget->GetBackgroundColor());
	  rndctx->SetColor(NS_RGB(0, 0, 255));
	  rndctx->FillRect(*(((nsPaintEvent*)aEvent)->rect));
	  
	  nsFont font("Times", NS_FONT_STYLE_NORMAL,
		      NS_FONT_VARIANT_NORMAL,
		      NS_FONT_WEIGHT_BOLD,
		      0,
		      12);
	  rndctx->SetFont(font);

	  rndctx->SetColor(NS_RGB(255, 0, 0));
	  rndctx->DrawString(aString, 50, 50, 100);
	  
	  
	  return nsEventStatus_eConsumeNoDefault;


        }
        break;

    }

    return nsEventStatus_eIgnore; 
}




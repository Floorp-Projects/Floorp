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
#include <stdio.h>
#include "nscore.h"
#include "nsISupports.h"
#include "nsIShellInstance.h"
#include "nsShellInstance.h"

#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIShellInstanceIID, NS_ISHELLINSTANCE_IID);
static NS_DEFINE_CID(kIShellInstanceCID, NS_ISHELLINSTANCE_CID);
static NS_DEFINE_CID(kCShellInstanceIID, NS_ISHELLINSTANCE_CID);

class nsShellInstanceFactory : public nsIFactory {

public:
  nsShellInstanceFactory();
  ~nsShellInstanceFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports * aOuter,
                            const nsIID &aIID,
                            void ** aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

};

nsShellInstance::nsShellInstance()
{
}

nsShellInstance::~nsShellInstance()
{
}

NS_IMPL_ADDREF(nsShellInstance)
NS_IMPL_RELEASE(nsShellInstance)
NS_IMPL_QUERY_INTERFACE(nsShellInstance,NS_ISHELLINSTANCE_CID);

nsresult nsShellInstance::Init()
{
  nsresult res = NS_OK;

  RegisterFactories() ;

  return res;
}

nsresult nsShellInstance::Run()
{
#ifdef _WIN32
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return ((nsresult)msg.wParam);
#else
  return NS_OK;
#endif
}

void * nsShellInstance::GetNativeInstance()
{
  return mNativeInstance ;
}

void nsShellInstance::SetNativeInstance(void * aNativeInstance)
{
  mNativeInstance = aNativeInstance;
  return ;
}

nsIApplicationShell * nsShellInstance::GetApplicationShell()
{
  return mApplicationShell ;
}

void nsShellInstance::SetApplicationShell(nsIApplicationShell * aApplicationShell)
{
  mApplicationShell = aApplicationShell;
  return ;
}

// XXX We really need a standard way to enumerate 
//     a set of libraries and call their self
//     registration routines... when that code is 
//     XP of course.
nsresult nsShellInstance::RegisterFactories()
{
  #define WIDGET_DLL "raptorwidget.dll"
  #define GFXWIN_DLL "raptorgfxwin.dll"

  static NS_DEFINE_IID(kCWindowIID, NS_WINDOW_CID);
  static NS_DEFINE_IID(kCChildWindowIID, NS_CHILD_CID);
  static NS_DEFINE_IID(kCScrollbarIID, NS_VERTSCROLLBAR_CID);

  NSRepository::RegisterFactory(kCWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildWindowIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCScrollbarIID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  
  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsShellInstanceFactory::nsShellInstanceFactory()
{    
}

nsShellInstanceFactory::~nsShellInstanceFactory()
{    
}

nsresult nsShellInstanceFactory::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void*)(nsShellInstanceFactory*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsShellInstanceFactory*)this;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsShellInstanceFactory)
NS_IMPL_RELEASE(nsShellInstanceFactory)

nsresult nsShellInstanceFactory::CreateInstance(nsISupports * aOuter,
                                                   const nsIID &aIID,
                                                   void ** aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL ;  

  nsISupports * inst = new nsShellInstance() ;

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    delete inst ;
  }

  return res;

}

nsresult nsShellInstanceFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}

// return the proper factory to the caller
extern "C" NS_WEB nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsShellInstanceFactory();

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}


NS_WEB nsresult NS_NewShellInstance(nsIShellInstance** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsShellInstance* it = new nsShellInstance();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIShellInstanceIID, (void **) aInstancePtrResult);
}

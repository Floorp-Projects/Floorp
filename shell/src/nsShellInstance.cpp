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

#ifdef NS_WIN32
#include "windows.h"
#elif NS_UNIX
#include <Xm/Xm.h>
#endif

#include "nsISupports.h"
#include "nsIShellInstance.h"
#include "nsShellInstance.h"

#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

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
  mApplicationWindow = NULL;
}

nsShellInstance::~nsShellInstance()
{
}

NS_DEFINE_IID(kIShellInstanceIID, NS_ISHELLINSTANCE_IID);
NS_IMPL_ISUPPORTS(nsShellInstance,kIShellInstanceIID);

nsresult nsShellInstance::Init()
{
  nsresult res = NS_OK;

  RegisterFactories() ;

  return res;
}

nsresult nsShellInstance::Run()
{
#ifdef NS_WIN32
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
  }
  return ((nsresult)msg.wParam);
#elif NS_UNIX
  extern   XtAppContext app_context ;

  XtAppMainLoop(app_context) ;
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
  // hardcode names of dll's
#ifdef NS_WIN32
  #define GFXWIN_DLL "raptorgfxwin.dll"
  #define WIDGET_DLL "raptorwidget.dll"
#else
  #define GFXWIN_DLL "libgfxunix.so"
  #define WIDGET_DLL "libwidgetunix.so"
#endif

  // register graphics classes
  static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
  static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
  static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);

  NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);

  // register widget classes
  static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);
  static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);
  static NS_DEFINE_IID(kCButtonCID, NS_BUTTON_CID);
  static NS_DEFINE_IID(kCCheckButtonCID, NS_CHECKBUTTON_CID);
  static NS_DEFINE_IID(kCComboBoxCID, NS_COMBOBOX_CID);
  static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
  static NS_DEFINE_IID(kCListBoxCID, NS_LISTBOX_CID);
  static NS_DEFINE_IID(kCRadioButtonCID, NS_RADIOBUTTON_CID);
  static NS_DEFINE_IID(kCRadioGroupCID, NS_RADIOGROUP_CID);
  static NS_DEFINE_IID(kCHorzScrollbarCID, NS_HORZSCROLLBAR_CID);
  static NS_DEFINE_IID(kCVertScrollbarCID, NS_VERTSCROLLBAR_CID);
  static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
  static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);

  NSRepository::RegisterFactory(kCWindowCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCChildCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCCheckButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCComboBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCFileWidgetCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCListBoxCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRadioButtonCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCRadioGroupCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCHorzScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCVertScrollbarCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextAreaCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  NSRepository::RegisterFactory(kCTextFieldCID, WIDGET_DLL, PR_FALSE, PR_FALSE);
  

  return NS_OK;
}

nsIWidget * nsShellInstance::CreateApplicationWindow(const nsRect &aRect,
                                                     EVENT_CALLBACK aHandleEventFunction)
{

  nsRect windowRect ;

  if (aRect.IsEmpty()) {
    windowRect.SetRect(100,100,320,480);
  } else {
    windowRect.SetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  static NS_DEFINE_IID(kCWindowCID, NS_WINDOW_CID);

  NSRepository::CreateInstance(kCWindowCID, 
                               nsnull, 
                               kIWidgetIID, 
                               (void **)&(mApplicationWindow));

  mApplicationWindow->Create((nsIWidget*)NULL, 
                  aRect, 
                  aHandleEventFunction, 
                  nsnull, nsnull, (nsWidgetInitData *) GetNativeInstance());

  return (mApplicationWindow);
}


nsresult nsShellInstance::ShowApplicationWindow(PRBool show)
{
  mApplicationWindow->Show(show);

#ifdef NS_UNIX
  XtRealizeWidget((Widget)GetNativeInstance());
#endif

  return NS_OK;
}

nsresult nsShellInstance::ExitApplication()
{

#ifdef NS_WIN32
  PostQuitMessage(0);
#endif
  return NS_OK;
}

void * nsShellInstance::GetApplicationWindowNativeInstance()
{
  return (mApplicationWindow->GetNativeData(NS_NATIVE_WINDOW));
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



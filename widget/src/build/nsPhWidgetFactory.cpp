/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIButton.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsAppShell.h"
#include "nsLookAndFeel.h"
#include "nsDialog.h"
#include "nsMenuBar.h"
#include "nsMenu.h"
#include "nsMenuItem.h"

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_IID(kCAppShell,      NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCLookAndFeel,   NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCDialog,        NS_DIALOG_CID);
static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);
static NS_DEFINE_IID(kCMenuBar,       NS_MENUBAR_CID);
static NS_DEFINE_IID(kCMenu,          NS_MENU_CID);
static NS_DEFINE_IID(kCMenuItem,      NS_MENUITEM_CID);

class nsWidgetFactory : public nsIFactory
{   
public:   

    NS_DECL_ISUPPORTS

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsWidgetFactory(const nsCID &aClass);   
    ~nsWidgetFactory();   
private:
  nsCID mClassID;

};   



nsWidgetFactory::nsWidgetFactory(const nsCID &aClass)   
{   
 NS_INIT_REFCNT();
 mClassID = aClass;
}   

nsWidgetFactory::~nsWidgetFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "Reference count not zero in destructor");
}   

nsresult nsWidgetFactory::QueryInterface(const nsIID &aIID,   
                                         void **aResult)   
{   
  if (NULL == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports *)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory *)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();
  return NS_OK;
}   

NS_IMPL_ADDREF(nsWidgetFactory)
NS_IMPL_RELEASE(nsWidgetFactory)

nsresult nsWidgetFactory::CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
    if (aResult == NULL) {  
        return NS_ERROR_NULL_POINTER;  
    }  

    *aResult = NULL;  

    if (nsnull != aOuter)
      return NS_ERROR_NO_AGGREGATION;
  
    nsISupports *inst = nsnull;
    if (mClassID.Equals(kCWindow)) {
      inst = (nsISupports *)new nsWindow();
    }
    else if (mClassID.Equals(kCChild)) {
      inst = (nsISupports *)new ChildWindow();
    }
    else if (mClassID.Equals(kCAppShell)) {
        inst = (nsISupports*)new nsAppShell();
    }
    else if (mClassID.Equals(kCToolkit)) {
        inst = (nsISupports*)new nsToolkit();
    }
    else if (mClassID.Equals(kCLookAndFeel)) {
        inst = (nsISupports*)new nsLookAndFeel();
    }
    else if (mClassID.Equals(kCDialog)) {
        inst = (nsISupports*)(nsWindow *)new nsDialog();
    }
    else if (mClassID.Equals(kCMenuBar)) {
        inst = (nsISupports*)(nsIMenuBar *)new nsMenuBar();
    }
    else if (mClassID.Equals(kCMenu)) {
        inst = (nsISupports*)(nsIMenu *)new nsMenu();
    }
    else if (mClassID.Equals(kCMenuItem)) {
        inst = (nsISupports*)(nsIMenuItem *)new nsMenuItem();
    }
												
    if (inst == NULL) {  
        return NS_ERROR_OUT_OF_MEMORY;  
    }  

    nsresult res = inst->QueryInterface(aIID, aResult);

    if (res != NS_OK) {  
        // We didn't get the right interface, so clean up  
        delete inst;  
    }

    return res;

}  

nsresult nsWidgetFactory::LockFactory(PRBool aLock)  
{  
    // Not implemented in simplest case.  
    return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_WIDGET nsresult 
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (nsnull == aFactory) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = new nsWidgetFactory(aClass);

    if (nsnull == aFactory) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}



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
#include "nsWidgetsCID.h"

#include "nsToolkit.h"
#include "nsWindow.h"

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kIWidget,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);


static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);

class nsWidgetFactory : public nsIFactory
{   
public:   

    NS_DECL_ISUPPORTS

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsWidgetFactory();   
    ~nsWidgetFactory();   

};   



nsWidgetFactory::nsWidgetFactory()   
{   
}   

nsWidgetFactory::~nsWidgetFactory()   
{   
}   

nsresult nsWidgetFactory::QueryInterface(const nsIID &aIID,   
                                         void **aInstancePtr)   
{   
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void*)(nsWidgetFactory*)this;
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsWidgetFactory*)this;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
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
  
    if (nsnull != aOuter && !aIID.Equals(kISupportsIID)) {
        // aggregation with IID != nsISupports
        return NS_ERROR_ILLEGAL_VALUE;
    }

    nsIWidget *inst = nsnull;
    if (aIID.Equals(kCWindow)) {
        inst = (nsIWidget*)new nsWindow(aOuter);
    }
    else if (aIID.Equals(kIWidget)) {
        inst = (nsIWidget*)new nsWindow(aOuter);
    }
    else if (aIID.Equals(kCChild)) {
        inst = (nsIWidget*)new ChildWindow(aOuter);
    }
  
    if (inst == NULL) {  
        return NS_ERROR_OUT_OF_MEMORY;  
    }  


  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {
    delete inst ;
  }

  return res;

    return NS_OK;  
}  

nsresult nsWidgetFactory::LockFactory(PRBool aLock)  
{  
    // Not implemented in simplest case.  
    return NS_OK;
}  

// return the proper factory to the caller
extern "C" NS_WIDGET nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
    if (nsnull == aFactory) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFactory = new nsWidgetFactory();

    if (nsnull == aFactory) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}



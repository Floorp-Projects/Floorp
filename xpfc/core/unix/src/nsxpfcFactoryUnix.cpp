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

#include "nsxpfcFactory.h"
#include "nsXPFCMenuContainerUnix.h"

class nsxpfcFactoryUnix : public nsxpfcFactory
{   
  public:   

    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   


    nsxpfcFactoryUnix(const nsCID &aClass);   
    ~nsxpfcFactoryUnix();   

};   

nsxpfcFactoryUnix::nsxpfcFactoryUnix(const nsCID &aClass) : nsxpfcFactory(aClass)
{   
}   

nsxpfcFactoryUnix::~nsxpfcFactoryUnix()   
{   
}   

nsresult nsxpfcFactoryUnix::CreateInstance(nsISupports *aOuter,  
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCXPFCMenuBar)) {
    inst = (nsISupports *)(nsIXPFCMenuBar *)new nsXPFCMenuContainerUnix();
  } else if (mClassID.Equals(kCXPFCMenuContainer)) {
    inst = (nsISupports *)(nsIXPFCMenuContainer *)new nsXPFCMenuContainerUnix();
  }

  if (inst == NULL)
    return (nsxpfcFactory::CreateInstance(aOuter,aIID,aResult));

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK)
    delete inst;  

  return res;  
}  

// return the proper factory to the caller
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass, nsIFactory **aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsxpfcFactoryUnix(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}


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

#include "nsxpfc.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsxpfcCIID.h"
#include "nsxpfcFactory.h"


nsxpfcFactory::nsxpfcFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsxpfcFactory::~nsxpfcFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsxpfcFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt nsxpfcFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsxpfcFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsxpfcFactory::CreateInstance(nsISupports *aOuter,  
                                        const nsIID &aIID,  
                                        void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCShellInstance)) {
    inst = (nsISupports *)new nsShellInstance();
  } else if (mClassID.Equals(kCXPFCCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsXPFCCanvas(aOuter);
  } else if (mClassID.Equals(kCXPFCHTMLCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsXPFCHTMLCanvas(aOuter);
  } else if (mClassID.Equals(kCXPFolderCanvas)) {
    inst = (nsISupports *)(nsIXPFCCanvas *)new nsXPFolderCanvas(aOuter);
  } else if (mClassID.Equals(kCXPFCMenuContainer)) {
    inst = (nsISupports *)(nsIXPFCMenuContainer *)new nsXPFCMenuContainer();
  } else if (mClassID.Equals(kCXPFCToolbar)) {
    inst = (nsISupports *)(nsIXPFCToolbar *)new nsXPFCToolbar(aOuter);
  } else if (mClassID.Equals(kCXPFCDialog)) {
    inst = (nsISupports *)(nsIXPFCDialog *)new nsXPFCDialog(aOuter);
  } else if (mClassID.Equals(kCUserCID)) {
    inst = (nsISupports *)(nsIUser *)new nsUser(aOuter);
  } else if (mClassID.Equals(kCXPFCButton)) {
    inst = (nsISupports *)(nsIXPFCButton *)new nsXPFCButton(aOuter);
  } else if (mClassID.Equals(kCXPButton)) {
    inst = (nsISupports *)(nsIXPButton *)new nsXPButton(aOuter);
  } else if (mClassID.Equals(kCXPItem)) {
    inst = (nsISupports *)(nsIXPItem *)new nsXPItem(aOuter);
  } else if (mClassID.Equals(kCXPFCTextWidget)) {
    inst = (nsISupports *)(nsIXPFCTextWidget *)new nsXPFCTextWidget(aOuter);
  } else if (mClassID.Equals(kCXPFCTabWidget)) {
    inst = (nsISupports *)(nsIXPFCTabWidget *)new nsXPFCTabWidget(aOuter);
  } else if (mClassID.Equals(kCMenuManager)) {
    inst = (nsISupports *)(nsIMenuManager *)new nsMenuManager();
  } else if (mClassID.Equals(kCXPFCToolbarManager)) {
    inst = (nsISupports *)(nsIXPFCToolbarManager *)new nsXPFCToolbarManager();
  } else if (mClassID.Equals(kCStreamManager)) {
    inst = (nsISupports *)(nsIStreamManager *)new nsStreamManager();
  } else if (mClassID.Equals(kCStreamObject)) {
    inst = (nsISupports *)(nsIStreamObject *)new nsStreamObject();
  } else if (mClassID.Equals(kCXPFCMenuItem)) {
    inst = (nsISupports *)(nsIXPFCMenuItem *)new nsXPFCMenuItem();
  } else if (mClassID.Equals(kCXPFCDTD)) {
    inst = (nsISupports *)new nsXPFCXMLDTD();
  } else if (mClassID.Equals(kCXPFCContentSink)) {
    inst = (nsISupports *)(nsIXPFCXMLContentSink*)new nsXPFCXMLContentSink();
  } else if (mClassID.Equals(kCVector)) {
    inst = (nsISupports *)new nsVector();
  } else if (mClassID.Equals(kCVectorIterator)) {
    inst = (nsISupports *)new nsVectorIterator();
  } else if (mClassID.Equals(kCStack)) {
    inst = (nsISupports *)new nsStack();
  } else if (mClassID.Equals(kCLayout)) {
    inst = (nsISupports *)new nsLayout();
  } else if (mClassID.Equals(kCBoxLayout)) {
    inst = (nsISupports *)new nsBoxLayout();
  } else if (mClassID.Equals(kCListLayout)) {
    inst = (nsISupports *)new nsListLayout();
  } else if (mClassID.Equals(kCXPFCObserver)) {
    inst = (nsISupports *)new nsXPFCObserver();
  } else if (mClassID.Equals(kCXPFCObserverManager)) {
    inst = (nsISupports *)new nsXPFCObserverManager();
  } else if (mClassID.Equals(kCXPFCMethodInvokerCommand)) {
    inst = (nsISupports *)new nsXPFCMethodInvokerCommand();
  } else if (mClassID.Equals(kCXPFCNotificationStateCommand)) {
    inst = (nsISupports *)new nsXPFCNotificationStateCommand();
  } else if (mClassID.Equals(kCXPFCModelUpdateCommand)) {
    inst = (nsISupports *)new nsXPFCModelUpdateCommand();
  } else if (mClassID.Equals(kCXPFCActionCommand)) {
    inst = (nsISupports *)new nsXPFCActionCommand();
  } else if (mClassID.Equals(kCXPFCSubject)) {
    inst = (nsISupports *)new nsXPFCSubject();
  } else if (mClassID.Equals(kCXPFCCanvasManager)) {
    inst = (nsISupports *)(nsIXPFCCanvasManager *)new nsXPFCCanvasManager();
  } else if (mClassID.Equals(kCXPFCCommand)) {
    inst = (nsISupports *)new nsXPFCCommand();
  } else if (mClassID.Equals(kCXPFCCommandServerCID)) {
    inst = (nsISupports *)new nsCommandServer();
  } else if (mClassID.Equals(kCXPFCDataCollectionManager)) {
    inst = (nsISupports *)new nsXPFCDataCollectionManager();
  } else if (mClassID.Equals(kCSMTPServiceCID)) {
    inst = (nsISupports *)new nsSMTPService();
  }
#if 0
  else if (mClassID.Equals(kCMIMEServiceCID)) {
    inst = (nsISupports *)new nsMIMEService();
  } else if (mClassID.Equals(kCMessageCID)) {
    inst = (nsISupports *)new nsMessage();
  } else if (mClassID.Equals(kCMIMEMessageCID)) {
    inst = (nsISupports *)(nsIMIMEMessage*) new nsMIMEMessage();
  } else if (mClassID.Equals(kCMIMEBodyPartCID)) {
    inst = (nsISupports *)(nsIMIMEBodyPart*) new nsMIMEBodyPart();
  } else if (mClassID.Equals(kCMIMEBasicBodyPartCID)) {
    inst = (nsISupports *)(nsIMIMEBodyPart*) new nsMIMEBasicBodyPart();
  }
#endif

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

nsresult nsxpfcFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsdefs.h"
#include "nsWidgetsCID.h"

#include "nsFilePicker.h"
#include "nsFileWidget.h"
#include "nsFileSpecWithUIImpl.h"
#include "nsLookAndFeel.h"
#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsAppShell.h"
#include "nsIServiceManager.h"
#include "nsFontRetrieverService.h"
#include "nsSound.h"

#include "nsWindowsTimer.h"
#include "nsTimerManager.h"

// Drag & Drop, Clipboard
#include "nsClipboard.h"
#include "nsTransferable.h"
#include "nsXIFFormatConverter.h"
#include "nsDragService.h"

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);
static NS_DEFINE_IID(kCFileOpen,      NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kCFilePicker,    NS_FILEPICKER_CID);
static NS_DEFINE_IID(kCHorzScrollbar, NS_HORZSCROLLBAR_CID);
static NS_DEFINE_IID(kCVertScrollbar, NS_VERTSCROLLBAR_CID);
static NS_DEFINE_IID(kCAppShell,      NS_APPSHELL_CID);
static NS_DEFINE_IID(kCToolkit,       NS_TOOLKIT_CID);
static NS_DEFINE_IID(kCLookAndFeel,   NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kCFontRetrieverService, NS_FONTRETRIEVERSERVICE_CID);

static NS_DEFINE_IID(kCTimer, NS_TIMER_CID);
static NS_DEFINE_IID(kCTimerManager, NS_TIMERMANAGER_CID);

// Drag & Drop, Clipboard
static NS_DEFINE_IID(kCDataObj,       NS_DATAOBJ_CID);
static NS_DEFINE_IID(kCClipboard,     NS_CLIPBOARD_CID);
static NS_DEFINE_IID(kCTransferable,  NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCXIFFormatConverter,  NS_XIFFORMATCONVERTER_CID);
static NS_DEFINE_IID(kCDragService,   NS_DRAGSERVICE_CID);

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,    NS_IFACTORY_IID);


// Sound services (just Beep for now)
static NS_DEFINE_CID(kCSound,   NS_SOUND_CID);
static NS_DEFINE_CID(kCFileSpecWithUI,   NS_FILESPECWITHUI_CID);


class nsWidgetFactory : public nsIFactory
{   
public:   
    // nsISupports methods   
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

NS_IMPL_ADDREF(nsWidgetFactory)
NS_IMPL_RELEASE(nsWidgetFactory)

nsWidgetFactory::nsWidgetFactory(const nsCID &aClass)   
{   
  NS_INIT_REFCNT();
  mClassID = aClass;
}   


nsWidgetFactory::~nsWidgetFactory()   
{   
}   

nsresult nsWidgetFactory::QueryInterface(const nsIID &aIID,   
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

    NS_ADDREF_THIS(); // Increase reference count for caller   
    return NS_OK;   
}   





nsresult nsWidgetFactory::CreateInstance( nsISupports* aOuter,
                                          const nsIID &aIID,  
                                          void **aResult)  
{  
    if (aResult == NULL) {  
      return NS_ERROR_NULL_POINTER;  
    }  

    *aResult = NULL;  
    if (nsnull != aOuter) {
      return NS_ERROR_NO_AGGREGATION;
    }

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kCWindow)) {
        inst = (nsISupports*)(nsBaseWidget*)new nsWindow();
    }
    else if (mClassID.Equals(kCChild)) {
        inst = (nsISupports*)(nsBaseWidget*)new ChildWindow();
    }
    else if (mClassID.Equals(kCFileOpen)) {
        inst = (nsISupports*)new nsFileWidget();
    }
    else if (mClassID.Equals(kCFilePicker)) {
        inst = (nsISupports*)(nsBaseFilePicker*)new nsFilePicker();
    }
    else if (mClassID.Equals(kCHorzScrollbar)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_FALSE);
    }
    else if (mClassID.Equals(kCVertScrollbar)) {
        inst = (nsISupports*)(nsBaseWidget*)(nsWindow*)new nsScrollbar(PR_TRUE);
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
    else if (mClassID.Equals(kCSound)) {
    	  inst = (nsISupports*)(nsISound*)new nsSound();
    } 
    else if (mClassID.Equals(kCFileSpecWithUI)) {
    	inst = (nsISupports*) (nsIFileSpecWithUI *) new nsFileSpecWithUIImpl;
    }
    else if (mClassID.Equals(kCTransferable)) {
      inst = (nsISupports*)new nsTransferable();
    }
    else if (mClassID.Equals(kCXIFFormatConverter)) {
        inst = (nsISupports*)new nsXIFFormatConverter();
    }
    else if (mClassID.Equals(kCClipboard)) {
        inst = (nsISupports*)(nsBaseClipboard *)new nsClipboard();
    }
    else if (mClassID.Equals(kCDragService)) {
        inst = (nsISupports*)(nsIDragService *)new nsDragService();
    }
    else if (mClassID.Equals(kCFontRetrieverService)) {
        inst = (nsISupports*)(nsIFontRetrieverService *)new nsFontRetrieverService();
    }
    else if (mClassID.Equals(kCTimer)) {
        inst = (nsISupports*)(nsITimer*) new nsTimer();
    }
    else if (mClassID.Equals(kCTimerManager)) {
        inst = (nsISupports*)(nsITimerQueue*) new nsTimerManager();
    }
    if (inst == NULL) {  
        return NS_ERROR_OUT_OF_MEMORY;  
    }  

    NS_ADDREF(inst);  // Stabilize
    nsresult res = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

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
             const char *aContractID,
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





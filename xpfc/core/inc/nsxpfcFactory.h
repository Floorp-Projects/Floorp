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

#include "nsxpfc.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsxpfcCIID.h"

#include "nsShellInstance.h"
#include "nsIXPFCMenuBar.h"
#include "nsXPFCToolbar.h"
#include "nsXPFCDialog.h"
#include "nsXPFCButton.h"
#include "nsXPButton.h"
#include "nsXPItem.h"
#include "nsXPFCTabWidget.h"
#include "nsXPFCTextWidget.h"
#include "nsXPFCMenuContainer.h"
#include "nsXPFCMenuItem.h"
#include "nsXPFCXMLDTD.h"
#include "nsXPFCXMLContentSink.h"
#include "nsMenuManager.h"
#include "nsXPFCToolbarManager.h"
#include "nsStreamManager.h"
#include "nsStreamObject.h"
#include "nsVector.h"
#include "nsVectorIterator.h"
#include "nsStack.h"
#include "nsLayout.h"
#include "nsBoxLayout.h"
#include "nsListLayout.h"
#include "nsXPFCObserver.h"
#include "nsXPFCObserverManager.h"
#include "nsXPFCSubject.h"
#include "nsXPFCCommand.h"
#include "nsXPFCCanvas.h"
#include "nsXPFCCanvasManager.h"
#include "nsXPFCMethodInvokerCommand.h"
#include "nsXPFCNotificationStateCommand.h"
#include "nsXPFCModelUpdateCommand.h"
#include "nsXPFCActionCommand.h"
#include "nsCommandServer.h"
#include "nsXPFCHTMLCanvas.h"
#include "nsXPFolderCanvas.h"
#include "nsUser.h"

static NS_DEFINE_IID(kCShellInstance,             NS_XPFC_SHELL_INSTANCE_CID);
static NS_DEFINE_IID(kCXPFCMenuItem,                  NS_XPFCMENUITEM_CID);
static NS_DEFINE_IID(kCXPFCMenuBar,                   NS_XPFCMENUBAR_CID);
static NS_DEFINE_IID(kCXPFCMenuContainer,             NS_XPFCMENUCONTAINER_CID);
static NS_DEFINE_IID(kCMenuManager,               NS_MENU_MANAGER_CID);
static NS_DEFINE_IID(kCStreamManager,             NS_STREAM_MANAGER_CID);
static NS_DEFINE_IID(kIStreamManager,             NS_ISTREAM_MANAGER_IID);
static NS_DEFINE_IID(kCStreamObject,              NS_STREAM_OBJECT_CID);
static NS_DEFINE_IID(kIStreamObject,              NS_ISTREAM_OBJECT_IID);
static NS_DEFINE_IID(kCXPFCDTD,                   NS_IXPFCXML_DTD_IID);
static NS_DEFINE_IID(kCXPFCContentSink,           NS_XPFCXMLCONTENTSINK_IID);
static NS_DEFINE_IID(kCXPFCToolbar,               NS_XPFC_TOOLBAR_CID);
static NS_DEFINE_IID(kCXPFCToolbarManager,            NS_XPFCTOOLBAR_MANAGER_CID);
static NS_DEFINE_IID(kCXPFCDialog,                NS_XPFC_DIALOG_CID);
static NS_DEFINE_IID(kCXPFCButton,                NS_XPFC_BUTTON_CID);
static NS_DEFINE_IID(kCXPButton,                  NS_XP_BUTTON_CID);
static NS_DEFINE_IID(kCXPItem,                    NS_XP_ITEM_CID);
static NS_DEFINE_IID(kCXPFCTabWidget,             NS_XPFC_TABWIDGET_CID);
static NS_DEFINE_IID(kCXPFCTextWidget,            NS_XPFC_TEXTWIDGET_CID);
static NS_DEFINE_IID(kCVector,                    NS_VECTOR_CID);
static NS_DEFINE_IID(kCVectorIterator,            NS_VECTOR_ITERATOR_CID);
static NS_DEFINE_IID(kCLayout,                    NS_LAYOUT_CID);
static NS_DEFINE_IID(kILayout,                    NS_ILAYOUT_IID);
static NS_DEFINE_IID(kCBoxLayout,                 NS_BOXLAYOUT_CID);
static NS_DEFINE_IID(kCListLayout,                NS_LISTLAYOUT_CID);
static NS_DEFINE_IID(kCXPFCObserver,              NS_XPFC_OBSERVER_CID);
static NS_DEFINE_IID(kCXPFCObserverManager,       NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kCXPFCSubject,               NS_XPFC_SUBJECT_CID);
static NS_DEFINE_IID(kCXPFCCommand,               NS_XPFC_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCCanvas,                NS_XPFC_CANVAS_CID);
static NS_DEFINE_IID(kCXPFCHTMLCanvas,            NS_XPFC_HTML_CANVAS_CID);
static NS_DEFINE_IID(kCXPFolderCanvas,            NS_XP_FOLDER_CANVAS_CID);
static NS_DEFINE_IID(kCXPFCCanvasManager,         NS_XPFC_CANVASMANAGER_CID);
static NS_DEFINE_IID(kIXPFCCanvas,                NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kISupportsIID,               NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,                NS_IFACTORY_IID);
static NS_DEFINE_IID(kCStack,                     NS_STACK_CID);
static NS_DEFINE_IID(kCXPFCMethodInvokerCommand,  NS_XPFC_METHODINVOKER_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCNotificationStateCommand,  NS_XPFC_NOTIFICATIONSTATE_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCModelUpdateCommand,    NS_XPFC_MODELUPDATE_COMMAND_CID);
static NS_DEFINE_IID(kCXPFCCommandServerCID,      NS_XPFC_COMMAND_SERVER_CID);
static NS_DEFINE_IID(kCXPFCActionCommand,         NS_XPFC_ACTION_COMMAND_CID);
static NS_DEFINE_IID(kCUserCID,                   NS_USER_CID);

class nsxpfcFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                              void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsxpfcFactory(const nsCID &aClass);   
    ~nsxpfcFactory();   

  protected:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   


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
#include "nsMenuContainer.h"
#include "nsIXMLParserObject.h"
#include "nspr.h"
#include "plstr.h"
#include "nsxpfcCIID.h"
#include "nsXPFCActionCommand.h"
#include "nsWidgetsCID.h"
#include "nsIFileWidget.h"
#include "nsIWebViewerContainer.h"

static NS_DEFINE_IID(kIXMLParserObjectIID, NS_IXML_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kCIMenuContainerIID, NS_IMENUCONTAINER_IID);
static NS_DEFINE_IID(kCIMenuItemIID, NS_IMENUITEM_IID);
static NS_DEFINE_IID(kFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
static NS_DEFINE_IID(kXPFCCommandReceiverIID, NS_IXPFC_COMMANDRECEIVER_IID);

nsMenuContainer::nsMenuContainer() : nsMenuItem()
{
  NS_INIT_REFCNT();
  mChildMenus = nsnull;
  mShellInstance = nsnull;
  mWebViewerContainer = nsnull;
}

nsMenuContainer::~nsMenuContainer()
{
}

NS_DEFINE_IID(kIMenuContainerIID, NS_IMENUCONTAINER_IID);

NS_IMPL_ADDREF(nsMenuContainer)
NS_IMPL_RELEASE(nsMenuContainer)

nsresult nsMenuContainer::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kIMenuContainerIID);                         
  static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIMenuBarIID)) {                                      
    *aInstancePtr = (nsIMenuBar*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCCommandReceiverIID)) {                                          
      *aInstancePtr = (void*)(nsIXPFCCommandReceiver *) this;   
      AddRef();                                                            
      return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIXMLParserObjectIID)) {                                      
    *aInstancePtr = (nsIXMLParserObject*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (nsMenuItem::QueryInterface(aIID,aInstancePtr));

}

nsresult nsMenuContainer::Init()
{
  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                     nsnull, 
                                     kCVectorCID, 
                                     (void **)&mChildMenus);

  if (NS_OK != res)
    return res ;

  mChildMenus->Init();

  return NS_OK;
}

void* nsMenuContainer::GetNativeHandle()
{
  return (nsnull);
}

nsresult nsMenuContainer :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsMenuItem::SetParameter(aKey,aValue));
}

nsresult nsMenuContainer :: AddMenuItem(nsIMenuItem * aMenuItem)
{
  return NS_OK;
}

nsresult nsMenuContainer :: AddChild(nsIMenuItem * aItem)
{
  mChildMenus->Append(aItem);

  aItem->SetParent(this);

  return (NS_OK);
}


nsresult nsMenuContainer :: Update()
{
  return NS_OK;
}

nsresult nsMenuContainer :: SetShellContainer(nsIShellInstance * aShellInstance,
                                             nsIWebViewerContainer * aWebViewerContainer)
{
  mShellInstance = aShellInstance;
  mWebViewerContainer = aWebViewerContainer;

  /*
   * Make this the default receiver for the menubar on the webview container
   */

  mWebViewerContainer->GetMenuManager()->SetDefaultReceiver((nsIXPFCCommandReceiver *)this);

  return (NS_OK);
}

nsIMenuItem * nsMenuContainer :: MenuItemFromID(PRUint32 aID)
{
  nsresult res;
  nsIIterator * iterator = nsnull;
  nsIMenuItem * item = nsnull;
  nsIMenuItem * child = nsnull;
  nsIMenuContainer * container = nsnull;
  PRBool bFoundItem = PR_FALSE;

  res = mChildMenus->CreateIterator(&iterator);

  if (res != NS_OK)
    return nsnull;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    item = (nsIMenuItem *) iterator->CurrentItem();    

    if (item->GetMenuID() == aID)
    {
      bFoundItem = PR_TRUE;  
      break;
    }

    iterator->Next();
  }

  if (bFoundItem == PR_FALSE)
  {
    item = nsnull;

    iterator->Init();

    while(!(iterator->IsDone()))
    {
      child = (nsIMenuItem *) iterator->CurrentItem();    

      res = child->QueryInterface(kCIMenuContainerIID, (void**)&container);

      if (NS_OK == res)
      {

        item = container->MenuItemFromID(aID);

        NS_RELEASE(container);

        if (item != nsnull)
          break;
      }

      iterator->Next();
    }

  }

  NS_RELEASE(iterator);

  return item;
}

nsresult nsMenuContainer :: Action(nsIXPFCCommand * aCommand)
{
  
  /*
   * Check to see this is an ActionCommand
   */

  nsresult res;

  nsXPFCActionCommand * action_command = nsnull;
  static NS_DEFINE_IID(kXPFCActionCommandCID, NS_XPFC_ACTION_COMMAND_CID);                 

  res = aCommand->QueryInterface(kXPFCActionCommandCID,(void**)&action_command);

  if (NS_OK != res)
    return res;
  

  /*
   * Yeah, this is an action command. Do something
   */

  ProcessActionCommand(action_command->mAction);

  NS_RELEASE(action_command);

  return NS_OK;
}

nsresult nsMenuContainer::ProcessActionCommand(nsString& aAction)
{
  /*
   * Handle File Open...
   */

  if (aAction == "FileOpen")
  {
    PRBool selectedFileName = PR_FALSE;
    nsIFileWidget *fileWidget;
    nsString title("Open UI");
    nsString name;

    nsresult rv = nsRepository::CreateInstance(kFileWidgetCID,
                                               nsnull,
                                               kIFileWidgetIID,
                                               (void**)&fileWidget);
    if (NS_OK == rv) 
    {
      nsString titles[] = {"all files","calendar ui", "xpfc ui" };
      nsString filters[] = {"*.*", "*.cal", "*.ui"};

      fileWidget->SetFilterList(3, titles, filters);

      fileWidget->Create(mShellInstance->GetApplicationWidget(),
                         title,
                         eMode_load,
                         nsnull,
                         nsnull);

      PRUint32 result = fileWidget->Show();

      if (result) 
      {
        fileWidget->GetFile(name);
        selectedFileName = PR_TRUE;
      }

      NS_RELEASE(fileWidget);
    }

    if (selectedFileName == PR_TRUE)
    {
      if (mWebViewerContainer)
        mWebViewerContainer->LoadURL(name,nsnull);
    }

  /*
   * Handle Application Exit
   */

  } else if (aAction == "ApplicationExit") {

    mShellInstance->ExitApplication();

  } else if (aAction == "StartCommandServer") {

    nsIApplicationShell * shell = nsnull;

    mWebViewerContainer->GetApplicationShell(shell);
    
    shell->StartCommandServer();

  } else if (aAction == "LaunchCommandClient") {

    mShellInstance->LaunchApplication(nsString("trextest"));

  } else if (aAction == "ComposeEvent") {

    if (mWebViewerContainer)
      mWebViewerContainer->LoadURL("resource://res/ui/compose_event.ui",nsnull);

  } else {

    /*
     * It ain't builtin ... pass it off as a url to be processed
     */ 

    if (mWebViewerContainer)
      mWebViewerContainer->LoadURL(aAction,nsnull);

  }
  

  return NS_OK;
}
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

#include "nsMenuItem.h"
#include "nsIMenu.h"

#include "nsToolkit.h"
//#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>

//#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsIMenu.h"
#include "nsMenu.h"
#include "nsIMenuBar.h"
#include "nsIPopUpMenu.h"
#include "nsIWidget.h"

#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
//NS_IMPL_ISUPPORTS(nsMenuItem, kIMenuItemIID)

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuItemIID)) {                                         
    *aInstancePtr = (void*)(nsIMenuItem*)this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*)(nsIMenuListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;                     
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)


//-------------------------------------------------------------------------
//
// nsMenuItem constructor
//
//-------------------------------------------------------------------------
nsMenuItem::nsMenuItem() : nsIMenuItem()
{
  NS_INIT_REFCNT();
  mMenu     = nsnull;
  mTarget   = nsnull;
  mListener = nsnull;
  mIsSeparator = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  NS_IF_RELEASE(mMenu);
  NS_IF_RELEASE(mTarget);
  NS_IF_RELEASE(mListener);
}

//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  nsIWidget    * widget  = nsnull; // MenuBar's Parent
  nsIMenu      * menu    = nsnull;
  nsIMenuBar   * menuBar = nsnull;
  nsIPopUpMenu * popup   = nsnull;
  nsISupports  * parent  = aParent;

  // Bump the ref count on the parent, since it gets released unconditionally..
  NS_ADDREF(parent);
  while (1) {
    if (NS_OK == parent->QueryInterface(kIMenuIID,(void**)&menu)) {
      NS_RELEASE(parent);
      if (NS_OK != menu->GetParent(parent)) {
        NS_RELEASE(menu);
        return nsnull;
      }
      NS_RELEASE(menu);

    } else if (NS_OK == parent->QueryInterface(kIPopUpMenuIID,(void**)&popup)) {
      if (NS_OK != popup->GetParent(widget)) {
        widget =  nsnull;
      }
      NS_RELEASE(parent);
      NS_RELEASE(popup);
      return widget;

    } else if (NS_OK == parent->QueryInterface(kIMenuBarIID,(void**)&menuBar)) {
      if (NS_OK != menuBar->GetParent(widget)) {
        widget =  nsnull;
      }
      NS_RELEASE(parent);
      NS_RELEASE(menuBar);
      return widget;
    } else {
      NS_RELEASE(parent);
      return nsnull;
    }
  }
  return nsnull;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  mCommand = aCommand;
  mLabel   = aLabel;
  mMenu    = aParent;
  NS_ADDREF(mMenu);

  nsISupports * sups;
  if (NS_OK == aParent->QueryInterface(kISupportsIID,(void**)&sups)) {
    mTarget = GetMenuBarParent(sups);
    NS_RELEASE(sups);
  } else {
    mTarget = nsnull;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent, const nsString &aLabel, PRUint32 aCommand)
{
  mCommand = aCommand;
  mLabel   = aLabel;
  //mMenu    = aParent;

  if (NS_OK != aParent->GetParent(mTarget)) {
    mTarget = nsnull;
  }  
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu * aParent)
{
  mMenu    = aParent;
  mIsSeparator = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent)
{
  //mMenu    = aParent;
  mIsSeparator = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetLabel(nsString &aText)
{
   mLabel = aText;
  
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
  aCommand = mCommand;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
  aTarget = mTarget;
  NS_ADDREF(mTarget);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  aData = (void *)mMenu;
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (mListener == aMenuListener) {
    NS_IF_RELEASE(mListener);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
void nsMenuItem::SetCmdId(PRInt32 aId)
{
  mCmdId = aId;
}

//-------------------------------------------------------------------------
PRInt32 nsMenuItem::GetCmdId()
{
  return mCmdId;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  MENUITEMINFO menuInfo;
  memset(&menuInfo, 0, sizeof(menuInfo));
  menuInfo.cbSize     = sizeof(menuInfo);
  menuInfo.fMask      = MIIM_STATE;
  menuInfo.fState     = (aIsEnabled ? MFS_ENABLED:MFS_DISABLED);

  BOOL status = ::SetMenuItemInfo(((nsMenu *)mMenu)->GetNativeMenu(), mCmdId, FALSE, &menuInfo);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::GetDOMElement(nsIDOMElement ** aDOMElement)
{
  *aDOMElement = mDOMElement;
  return NS_OK;

}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetCommand(const nsString & aStrCmd)
{
  mCommandStr = aStrCmd;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;
 
  nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
  contentViewerContainer = do_QueryInterface(mWebShell);
  if (!contentViewerContainer) {
      NS_ERROR("Webshell doesn't support the content viewer container interface");
      return rv;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
      NS_ERROR("Unable to retrieve content viewer.");
      return rv;
  }

  nsCOMPtr<nsIDocumentViewer> docViewer;
  docViewer = do_QueryInterface(contentViewer);
  if (!docViewer) {
      NS_ERROR("Document viewer interface not supported by the content viewer.");
      return rv;
  }

  nsCOMPtr<nsIPresContext> presContext;
  if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
      NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
      return rv;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = NS_MOUSE_LEFT_CLICK;

  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(mDOMElement);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

  return rv;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = do_QueryInterface(aWebShell);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = do_QueryInterface(aDOMElement);
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  if (mListener) {
    mListener->MenuItemSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  if (mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menubarNode,
	void              * aWebShell)
{
  if (mListener) {
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  if (mListener) {
    mListener->MenuDeselected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

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

#include <gtk/gtk.h>

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"

#include "nsStringUtil.h"
#include "nsGtkEventHandler.h"

#include "nsIPopUpMenu.h"

#include "nsCOMPtr.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

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
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIMenuListenerIID)) {
    *aInstancePtr = (void*)(nsIMenuListener*)this;
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
  mMenuItem    = nsnull;
  mMenuParent  = nsnull;
  mPopUpParent = nsnull;
  mTarget      = nsnull;
  mXULCommandListener = nsnull;
  mIsSeparator = PR_FALSE;
  mWebShell    = nsnull;
  mDOMElement  = nsnull;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  NS_IF_RELEASE(mTarget);
}

//-------------------------------------------------------------------------
void nsMenuItem::Create(nsIWidget      *aMBParent, 
                        GtkWidget      *aParent, 
                        const nsString &aLabel, 
                        PRBool         aIsSeparator)
{
  mTarget  = aMBParent;
  mLabel   = aLabel;

  if (NULL == aParent || nsnull == aMBParent) {
    return;
  }

  mTarget = aMBParent;
  char * nameStr = mLabel.ToNewCString();
  if(aIsSeparator) {
    mIsSeparator = PR_TRUE;
    mMenuItem = gtk_menu_item_new();
  }else{
    mIsSeparator = PR_FALSE;
    mMenuItem = gtk_menu_item_new_with_label(nameStr);
  }
  gtk_widget_show(mMenuItem);
 
  gtk_signal_connect (GTK_OBJECT (mMenuItem), "activate",
                      GTK_SIGNAL_FUNC(menu_item_activate_handler),
		      this);
  delete[] nameStr;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu * aParent)
{
  mIsSeparator = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent)
{
  mIsSeparator = PR_TRUE;
  return NS_OK;
}


//-------------------------------------------------------------------------
GtkWidget *nsMenuItem::GetNativeParent()
{
  void * voidData;
  if (nsnull != mMenuParent) {
    mMenuParent->GetNativeData(&voidData);
  } else if (nsnull != mPopUpParent) {
    mPopUpParent->GetNativeData(voidData);
  } else {
    return nsnull;
  }
  return GTK_WIDGET(voidData);
}


//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  nsIWidget    * widget  = nsnull; // MenuBar's Parent
  nsIMenu      * menu    = nsnull;
  nsIMenuBar   * menuBar = nsnull;
  nsIPopUpMenu * popup   = nsnull;
  nsISupports  * parent  = aParent;
  
  while(1) {
    if (NS_OK == parent->QueryInterface(kIMenuIID,(void**)&menu)) {
      if (NS_OK != menu->GetParent(parent)) {
        NS_RELEASE(menu);
        return nsnull;
      }
      NS_RELEASE(menu);

    } else if (NS_OK == parent->QueryInterface(kIPopUpMenuIID,(void**)&popup)) {
      if (NS_OK != popup->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(popup);
      return widget;

    } else if (NS_OK == parent->QueryInterface(kIMenuBarIID,(void**)&menuBar)) {
      if (NS_OK != menuBar->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(menuBar);
      return widget;
    } else {
      return nsnull;
    }
  }
  return nsnull;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIMenu        * aParent, 
                             const nsString & aLabel, 
                             PRBool           aIsSeparator)
                            
{
  if (nsnull == aParent) {
    return NS_ERROR_FAILURE;
  }

  mMenuParent = aParent;

  nsIWidget   * widget  = nsnull; // MenuBar's Parent
  nsISupports * sups;
  if (NS_OK == aParent->QueryInterface(kISupportsIID,(void**)&sups)) {
    widget = GetMenuBarParent(sups);
    NS_RELEASE(sups);
  }

  Create(widget, GetNativeParent(), aLabel, aIsSeparator);
//  aParent->AddMenuItem(this);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu   *aParent, 
                             const nsString &aLabel,  
                             PRUint32        aCommand)
{
  mPopUpParent = aParent;
  //NS_ADDREF(mPopUpParent);

  nsIWidget * widget = nsnull;
  if (NS_OK != aParent->GetParent(widget)) {
    widget = nsnull;
  }

  Create(widget, GetNativeParent(), aLabel, PR_FALSE);
//  aParent->AddItem(this);

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
NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  gtk_widget_set_sensitive(GTK_WIDGET(mMenuItem), aIsEnabled);
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
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  aData = (void *)mMenuItem;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
  NS_ASSERTION(false, "we should never get here");
  NS_IF_RELEASE(mXULCommandListener);
  NS_IF_ADDREF(aMenuListener);
  mXULCommandListener = aMenuListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  if(!mIsSeparator) {
    g_print("nsMenuItem::MenuItemSelected\n");
    DoCommand();
  }else{
    g_print("nsMenuItem::MenuItemSelected is separator\n");
  }
  return nsEventStatus_eIgnore;
}
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  if(mXULCommandListener)
    return mXULCommandListener->MenuSelected(aMenuEvent);

  g_print("nsMenuItem::MenuSelected\n");
  return nsEventStatus_eIgnore;
}
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  g_print("nsMenuItem::MenuDeselected\n");
  return nsEventStatus_eIgnore;
}
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  g_print("nsMenuItem::MenuConstruct\n");
  return nsEventStatus_eIgnore;
}
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  g_print("nsMenuItem::MenuDestruct\n");
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Sets the JavaScript Command to be invoked when a "gui" event occurs on a source widget
* @param aStrCmd the JS command to be cached for later execution
* @return NS_OK 
*/
NS_METHOD nsMenuItem::SetCommand(const nsString & aStrCmd)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Executes the "cached" JavaScript Command 
* @return NS_OK if the command was executed properly, otherwise an error code
*/
NS_METHOD nsMenuItem::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;
 
  if(!mWebShell || !mDOMElement)
    return rv;
    
  nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
  contentViewerContainer = do_QueryInterface(mWebShell);
  if (!contentViewerContainer) {
      NS_ERROR("Webshell doesn't support the content viewer container interface");
      g_print("Webshell doesn't support the content viewer container interface");
      return rv;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
      NS_ERROR("Unable to retrieve content viewer.");
      g_print("Unable to retrieve content viewer.");
      return rv;
  }

  nsCOMPtr<nsIDocumentViewer> docViewer;
  docViewer = do_QueryInterface(contentViewer);
  if (!docViewer) {
      NS_ERROR("Document viewer interface not supported by the content viewer.");
      g_print("Document viewer interface not supported by the content viewer.");
      return rv;
  }

  nsCOMPtr<nsIPresContext> presContext;
  if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
      NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
      g_print("Unable to retrieve the doc viewer's presentation context.");
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
      g_print("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
  g_print("HandleDOMEvent called");
  return rv;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = aDOMElement;
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMElement(nsIDOMElement ** aDOMElement)
{
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}


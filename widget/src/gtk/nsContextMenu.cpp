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

#include "nsContextMenu.h"
#include "nsIComponentManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsString.h"
#include "nsGtkEventHandler.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);

//-------------------------------------------------------------------------
nsresult nsContextMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  if (aIID.Equals(nsIContextMenu::GetIID())) {
    *aInstancePtr = (void*)(nsIContextMenu*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

//-------------------------------------------------------------------------
NS_IMPL_ADDREF(nsContextMenu)
NS_IMPL_RELEASE(nsContextMenu)

//-------------------------------------------------------------------------
//
// nsContextMenu constructor
//
//-------------------------------------------------------------------------
nsContextMenu::nsContextMenu()
{
  NS_INIT_REFCNT();
  mNumMenuItems  = 0;
  mMenu          = nsnull;
  mParent        = nsnull;
  mListener      = nsnull;
  mConstructCalled = PR_FALSE;
  
  mDOMNode       = nsnull;
  mWebShell      = nsnull;
  mDOMElement    = nsnull;

  mAlignment      = "topleft";
  mAnchorAlignment = "none";
}

//-------------------------------------------------------------------------
//
// nsContextMenu destructor
//
//-------------------------------------------------------------------------
nsContextMenu::~nsContextMenu()
{

}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::Create(nsISupports *aParent)
{
  if(aParent)
  {
    nsIWidget *parent = nsnull;
    aParent->QueryInterface(nsIWidget::GetIID(), (void**) &parent);
    if(parent)
    {
      mParent = parent;
      NS_RELEASE(parent);
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::GetParent(nsISupports*& aParent)
{
  aParent = nsnull;
  if (nsnull != mParent) {
    return mParent->QueryInterface(kISupportsIID,
                                   (void**)&aParent);
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetMenu(nsIMenu * aMenu)
{
  mMenu = aMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::GetNativeData(void ** aData)
{
  *aData = (void *)mMenu;
  return NS_OK;
}

/* used for gtk_menu_popup */
void MenuPosFunc(GtkMenu *menu, gint *x, gint *y, gpointer data)
{
  nsContextMenu *cm = (nsContextMenu*)data;
  *x = cm->GetX();
  *y = cm->GetY();
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::Show(PRBool aShow)
{
  if (aShow == PR_TRUE)
  {
    GtkWidget *parent = GTK_WIDGET(mParent->GetNativeData(NS_NATIVE_WIDGET));
    gtk_menu_popup(GTK_MENU(mMenu),
                   parent, nsnull,
                   MenuPosFunc,
                   this, 1, GDK_CURRENT_TIME);
  }
  else
  {
    printf("we suck\n");
  }
}


//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetLocation(PRInt32 aX, PRInt32 aY,
                                     const nsString& anAlignment,
                                     const nsString& anAnchorAlignment)
{

  mAlignment       = anAlignment;
  mAnchorAlignment = anAnchorAlignment;

  mX = aX;
  mY = aY;

  return NS_OK;
}


// local methods
gint nsContextMenu::GetX(void)
{
  return mX;
}

gint nsContextMenu::GetY(void)
{
  return mY;
}
// end silly local methods

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetDOMNode(nsIDOMNode *aMenuNode)
{
  mDOMNode = aMenuNode;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetDOMElement(nsIDOMElement *aMenuElement)
{
  mDOMElement = aMenuElement;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsContextMenu::SetWebShell(nsIWebShell *aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}

/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

// menu item

#include "nsWidgetDefs.h"
#include "nsMenuItem.h"
#include "nsMenuBase.h"
#include "nsIMenu.h"
#include "nsIContextMenu.h"
#include "nsIMenuBar.h"
#include "nsWindow.h"

#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIWebShell.h"
#include "nsIDOMElement.h"

// XP-Com
NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if( nsnull == aInstancePtr)
     return NS_ERROR_NULL_POINTER;

  *aInstancePtr = nsnull;

  if( aIID.Equals(nsIMenuItem::GetIID()))
  {
     *aInstancePtr = (void*) ((nsIMenuItem*) this);
     NS_ADDREF_THIS();
     return NS_OK;
  }
  if( aIID.Equals(nsIMenuListener::GetIID()))
  {
     *aInstancePtr = (void*) ((nsIMenuListener*)this);
     NS_ADDREF_THIS();
     return NS_OK;
  }
  if( aIID.Equals(((nsISupports*)(nsIMenuItem*)this)->GetIID()))
  {
     *aInstancePtr = (void*) ((nsISupports*)((nsIMenuItem*)this));
     NS_ADDREF_THIS();
     return NS_OK;
  }

  return NS_NOINTERFACE;
}

// Constructor
nsMenuItem::nsMenuItem() : mMenuBase(nsnull), mIsSeparator(PR_FALSE),
                           mTarget(nsnull), mPMID(0), mMenuListener(nsnull),
                           mDOMElement(nsnull), mWebShell(nsnull)
{
   NS_INIT_REFCNT();
}

nsMenuItem::~nsMenuItem()
{
   NS_IF_RELEASE(mMenuListener);
}

// Helper function to find the widget at the root of the menu tree.
//
// This may have to traverse an arbitrary number of cascaded menus.

static nsIWidget *FindWidget( nsISupports *aParent)
{
   nsIWidget      *widget  = nsnull;
   nsIMenu        *menu    = nsnull;
   nsIMenuBar     *menuBar = nsnull;
   nsIContextMenu *popup   = nsnull;
   nsISupports    *parent  = aParent;

   // Bump the ref count on the parent, since it gets released unconditionally..
   NS_ADDREF(parent);
   for( ;;)
   {
      if( NS_OK == parent->QueryInterface( nsIMenu::GetIID(), (void**) &menu))
      {
         NS_RELEASE(parent);
         if( NS_OK != menu->GetParent(parent))
         {
            NS_RELEASE(menu);
            return nsnull;
         }
         NS_RELEASE(menu);
      }
      else if( NS_OK == parent->QueryInterface( nsIContextMenu::GetIID(),
                                                (void**) &popup))
      {
         nsISupports *pThing = 0;
         if( NS_OK != popup->GetParent( pThing))
         {
            widget = nsnull;
         }
         pThing->QueryInterface( nsIWidget::GetIID(), (void**) &widget);
         NS_RELEASE(parent);
         NS_RELEASE(popup);
         return widget;
      }
      else if( NS_OK == parent->QueryInterface( nsIMenuBar::GetIID(),
                                                (void**) &menuBar))
      {
         if( NS_OK != menuBar->GetParent(widget))
         {
            widget =  nsnull;
         }
         NS_RELEASE(parent);
         NS_RELEASE(menuBar);
         return widget;
      }
      else
      {
         NS_RELEASE(parent);
         return nsnull;
      }
   }
   return nsnull;
}

nsresult nsMenuItem::Create( nsISupports *aParent, const nsString &aLabel,
                             PRBool aIsSeparator)
{
   nsIContextMenu *pPopup = nsnull;
   nsIMenu        *pMenu = nsnull;

   void         *pvWnd = 0;

   if( NS_SUCCEEDED( aParent->QueryInterface( nsIMenu::GetIID(),
                                              (void**) &pMenu)))
   {
      pMenu->GetNativeData( &pvWnd);
      NS_RELEASE(pMenu);
   }
   else if( NS_SUCCEEDED( aParent->QueryInterface( nsIContextMenu::GetIID(),
                                                   (void**) &pPopup)))
   {
      pPopup->GetNativeData( &pvWnd);
      NS_RELEASE(pPopup);
   }

   mMenuBase = (nsMenuBase*) WinQueryWindowPtr( (HWND) pvWnd, QWL_USER);
   mLabel = aLabel;

   mTarget = FindWidget( aParent);
   NS_ASSERTION(mTarget,"Can't find window to target menuitem to");
   if( mTarget)
   {
      mIsSeparator = aIsSeparator;
   
      HWND hwnd = (HWND) mTarget->GetNativeData( NS_NATIVE_WINDOW);
      nsWindow *wnd = NS_HWNDToWindow( hwnd); // doesn't addref
      mPMID = wnd->GetNextCmdID();
      mTarget->Release(); // don't want to hold a ref to the window
   }

   return NS_OK;
}

nsresult nsMenuItem::GetLabel( nsString &aText)
{
   aText = mLabel;
   return NS_OK;
}

nsresult nsMenuItem::SetLabel( nsString &aText)
{
   mLabel = aText;
   return NS_OK;
}

// menuitem style
nsresult nsMenuItem::SetEnabled( PRBool aIsEnabled)
{
   MENUITEM mI;
   mMenuBase->GetItemID( mPMID, &mI);
   if( PR_TRUE == aIsEnabled)
      mI.afAttribute &= ~MIA_DISABLED;
   else
      mI.afAttribute |= MIA_DISABLED;
   mMenuBase->UpdateItem( &mI);
   return NS_OK;
}

nsresult nsMenuItem::GetEnabled( PRBool *aIsEnabled)
{
   if( nsnull == aIsEnabled)
      return NS_ERROR_NULL_POINTER;

   MENUITEM mI;
   mMenuBase->GetItemID( mPMID, &mI);

   *aIsEnabled = ((mI.afAttribute & MIA_DISABLED) ? PR_FALSE : PR_TRUE);
   return NS_OK;
}

nsresult nsMenuItem::SetChecked( PRBool aIsChecked)
{
   MENUITEM mI;
   mMenuBase->GetItemID( mPMID, &mI);
   if( PR_TRUE == aIsChecked)
      mI.afAttribute |= MIA_CHECKED;
   else
      mI.afAttribute &= ~MIA_CHECKED;
   mMenuBase->UpdateItem( &mI);
   return NS_OK;
}

nsresult nsMenuItem::GetChecked( PRBool *aIsChecked)
{
   if( nsnull == aIsChecked)
      return NS_ERROR_NULL_POINTER;

   MENUITEM mI;
   mMenuBase->GetItemID( mPMID, &mI);

   *aIsChecked = ((mI.afAttribute & MIA_CHECKED) ? PR_TRUE : PR_FALSE);
   return NS_OK;
}

nsresult nsMenuItem::GetCommand( PRUint32 &aCommand)
{
   return NS_ERROR_FAILURE;
}

nsresult nsMenuItem::GetTarget( nsIWidget *&aTarget)
{
   NS_IF_RELEASE(aTarget);
   aTarget = mTarget;
   NS_IF_ADDREF(aTarget);
   return NS_OK;
}

nsresult nsMenuItem::GetNativeData( void *&aData)
{
   return mMenuBase->GetNativeData( &aData);
}

NS_METHOD nsMenuItem::AddMenuListener( nsIMenuListener *aMenuListener)
{
   NS_IF_RELEASE(mMenuListener);
   mMenuListener = aMenuListener;
   NS_ADDREF(mMenuListener);
   return NS_OK;
}

NS_METHOD nsMenuItem::RemoveMenuListener( nsIMenuListener *aMenuListener)
{
   if( mMenuListener == aMenuListener)
      NS_IF_RELEASE(mMenuListener);
   return NS_OK;
}

nsresult nsMenuItem::IsSeparator( PRBool &aIsSep)
{
   aIsSep = mIsSeparator;
   return NS_OK;
}

nsresult nsMenuItem::SetCommand( const nsString &aStrCmd)
{
   mCmdString = aStrCmd;
   return NS_OK;
}

nsresult nsMenuItem::DoCommand()
{
   // code copied from windows
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
   event.message = NS_MENU_ACTION;
 
   nsCOMPtr<nsIContent> contentNode;
   contentNode = do_QueryInterface(mDOMElement);
   if (!contentNode) {
       NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
       return rv;
   }

   rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
 
   return rv;
}

nsresult nsMenuItem::SetDOMElement( nsIDOMElement *aDOMElement)
{
   mDOMElement = aDOMElement;
   return NS_OK;
}

nsresult nsMenuItem::GetDOMElement( nsIDOMElement **aDOMElement)
{
   if( !aDOMElement)
      return NS_ERROR_NULL_POINTER;

   NS_IF_RELEASE(*aDOMElement);
   *aDOMElement = mDOMElement;
   NS_IF_ADDREF(mDOMElement);
   return NS_OK;
}

nsresult nsMenuItem::SetWebShell( nsIWebShell *aWebShell)
{
   mWebShell = aWebShell;
   return NS_OK;
}

// nsIMenuListener interface
nsEventStatus nsMenuItem::MenuItemSelected( const nsMenuEvent &aMenuEvent)
{
   DoCommand();
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuConstruct( const nsMenuEvent &aMenuEvent,
                                         nsIWidget         *aParentWindow, 
                                         void              *menubarNode,
                                         void              *aWebShell)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuSelected( const nsMenuEvent &aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDestruct( const nsMenuEvent &aMenuEvent)
{
   return nsEventStatus_eIgnore;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"

#include "nsIPopUpMenu.h"
#include "nsQEventHandler.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIContent.h"
#include "nsIContentViewer.h"
#include "nsIDOMElement.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);
//NS_IMPL_ISUPPORTS1(nsMenuItem, nsIMenuItem)

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aInstancePtr = NULL;

    if (aIID.Equals(kIMenuItemIID)) 
    {
        *aInstancePtr = (void*)(nsIMenuItem*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) 
    {
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::nsMenuItem()\n"));
    NS_INIT_REFCNT();
    mMenuItem           = nsnull;
    mMenuParent         = nsnull;
    mPopUpParent        = nsnull;
    mTarget             = nsnull;
    mXULCommandListener = nsnull;
    mIsSeparator        = PR_FALSE;
    mWebShell           = nsnull;
    mDOMElement         = nsnull;
    mIsSubMenu          = nsnull;
    mEventHandler       = nsnull;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::~nsMenuItem()\n"));
    NS_IF_RELEASE(mMenuParent);
    NS_IF_RELEASE(mPopUpParent);
    NS_IF_RELEASE(mTarget);

    delete mMenuItem;
}

//-------------------------------------------------------------------------
void nsMenuItem::Create(nsIWidget      * aMBParent, 
                        QWidget        * aParent, 
                        const nsString & aLabel)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::Create()\n"));
    mTarget  = aMBParent;
    mLabel   = aLabel;

    if (NULL == aParent || nsnull == aMBParent) 
    {
        return;
    }

    char * nameStr = ToNewCString(mLabel);

    mMenuItem = new QString(nameStr);

    //mEventHandler = nsQEventHandler::Instance();
#if 0
    QObject::connect(mMenuItem, 
                     SIGNAL(activated()), 
                     (QObject *)mEventHandler, 
                     SLOT(MenuItemActivated()));
#endif

    delete[] nameStr;
}

//-------------------------------------------------------------------------
QWidget * nsMenuItem::GetNativeParent()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetNativeParent()\n"));
    void * voidData;
    if (nsnull != mMenuParent) 
    {
        mMenuParent->GetNativeData(&voidData);
    } 
    else if (nsnull != mPopUpParent) 
    {
        mPopUpParent->GetNativeData(voidData);
    } 
    else 
    {
        return nsnull;
    }
    return (QWidget *) voidData;
}


//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetMenuBarParent()\n"));
    nsIWidget    * widget  = nsnull; // MenuBar's Parent
    nsIMenu      * menu    = nsnull;
    nsIMenuBar   * menuBar = nsnull;
    nsIPopUpMenu * popup   = nsnull;
    nsISupports  * parent  = aParent;

    // Bump the ref count on the parent, since it gets released 
    // unconditionally..
    NS_ADDREF(parent);
    while (1) 
    {
        if (NS_OK == parent->QueryInterface(kIMenuIID,(void**)&menu)) 
        {
            NS_RELEASE(parent);
            if (NS_OK != menu->GetParent(parent)) 
            {
                NS_RELEASE(menu);
                return nsnull;
            }
            NS_RELEASE(menu);

        } 
        else if (NS_OK == parent->QueryInterface(kIPopUpMenuIID,
                                                 (void**)&popup)) 
        {
            if (NS_OK != popup->GetParent(widget)) 
            {
                widget =  nsnull;
            } 
            NS_RELEASE(parent);
            NS_RELEASE(popup);
            return widget;

        } 
        else if (NS_OK == parent->QueryInterface(kIMenuBarIID,
                                                 (void**)&menuBar)) 
        {
            if (NS_OK != menuBar->GetParent(widget)) 
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

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsISupports    * aParent, 
                             const nsString & aLabel, 
                             PRBool           aIsSeparator)
                            
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::Create()\n"));
    if (nsnull == aParent) 
    {
        return NS_ERROR_FAILURE;
    }

    //mMenuParent = aParent;
    mIsSeparator = aIsSeparator;
    NS_ADDREF(mMenuParent);

    nsIWidget   * widget  = GetMenuBarParent(aParent); // MenuBar's Parent

    Create(widget, GetNativeParent(), aLabel);
    //  aParent->AddMenuItem(this);

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetLabel()\n"));
    aText = mLabel;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetLabel(nsString &aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::SetLabel()\n"));
    mLabel = aText;
  
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::SetEnabled()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetEnabled()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::SetChecked()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetChecked()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetCheckboxType(PRBool aIsCheckbox)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::SetCheckboxType()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCheckboxType(PRBool *aIsCheckbox)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetCheckboxType()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetCommand()\n"));
    aCommand = mCommand;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetTarget()\n"));
    aTarget = mTarget;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::GetNativeData()\n"));
    aData = (void *)mMenuItem;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::AddMenuListener()\n"));
    NS_IF_RELEASE(mXULCommandListener);
    NS_IF_ADDREF(aMenuListener);
    mXULCommandListener = aMenuListener;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::nsMenuItem()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::IsSeparator()\n"));
    aIsSep = mIsSeparator;
    return NS_OK;
}

//-------------------------------------------------------------------------
/**
 * Sets the JavaScript Command to be invoked when a "gui" event
 * occurs on a source widget
 * @param aStrCmd the JS command to be cached for later execution
 * @return NS_OK 
 */
NS_METHOD nsMenuItem::SetCommand(const nsString &aStrCmd)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::SetCommand()\n"));
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
    {
        return rv;
    }
    
    nsCOMPtr<nsIContentViewer> contentViewer;
    NS_ENSURE_SUCCESS(mWebShell->GetContentViewer(getter_AddRefs(contentViewer)),
      NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocumentViewer> docViewer;
    docViewer = do_QueryInterface(contentViewer);
    if (!docViewer) 
    {
        NS_ERROR("Document viewer interface not supported by the content viewer.");
        //g_print("Document viewer interface not supported by the content viewer.");
        return rv;
    }

    nsCOMPtr<nsIPresContext> presContext;
    if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) 
    {
        NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
        //g_print("Unable to retrieve the doc viewer's presentation context.");
        return rv;
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    nsMouseEvent event;
    event.eventStructType = NS_MOUSE_EVENT;
    event.message = NS_MOUSE_LEFT_CLICK;

    nsCOMPtr<nsIContent> contentNode;
    contentNode = do_QueryInterface(mDOMElement);
    if (!contentNode) 
    {
        NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
        //g_print("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
        return rv;
    }

    rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    //g_print("HandleDOMEvent called");
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

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetShortcutChar(const nsString &aText)
{
  mKeyEquivalent = aText;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::GetShortcutChar(nsString &aText)
{
  aText = mKeyEquivalent;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetModifiers(PRUint8 aModifiers)
{
  mModifiers = aModifiers;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::GetModifiers(PRUint8 * aModifiers)
{
  *aModifiers = mModifiers; 
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::MenuItemSelected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
    if(mXULCommandListener)
    {
        return mXULCommandListener->MenuSelected(aMenuEvent);
    }

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::MenuSelected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::MenuDeselected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                    nsIWidget         * aParentWindow, 
                                    void              * menuNode,
                                    void              * aWebShell)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::MenuConstruct()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuItem::MenuDestruct()\n"));
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetDOMNode(nsIDOMNode * aDOMNode)
{
  return NS_OK;
}
  
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMNode(nsIDOMNode ** aDOMNode)
{
  return NS_OK;
} 



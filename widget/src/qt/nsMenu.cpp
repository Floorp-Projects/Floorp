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

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"

#include "nsString.h"
#include "nsStringUtil.h"
#include "nsIMenuListener.h"
#include "nsQEventHandler.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID, NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

nsresult nsMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aInstancePtr = NULL;

    if (aIID.Equals(kIMenuIID))
    {
        *aInstancePtr = (void*)(nsIMenu*) this;
        NS_ADDREF_THIS();
        return NS_OK;
    }                                                                      
    if (aIID.Equals(kISupportsIID)) 
    {
        *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsIMenuListener))) 
    {
        *aInstancePtr = (void*)(nsIMenuListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }                   

    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsMenu)
NS_IMPL_RELEASE(nsMenu)

//-------------------------------------------------------------------------
//
// nsMenu constructor
//
//-------------------------------------------------------------------------
nsMenu::nsMenu() : nsIMenu()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::nsMenu()\n"));
    NS_INIT_REFCNT();
    mNumMenuItems  = 0;
    mMenu          = nsnull;
    mMenuParent    = nsnull;
    mMenuBarParent = nsnull;
    mListener      = nsnull;
    mEventHandler  = nsnull;

    mDOMNode       = nsnull;
    mWebShell      = nsnull;
    mDOMElement    = nsnull;
    mAccessKey     =  NS_ConvertASCIItoUCS2("_").get();
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::~nsMenu()\n"));
    NS_IF_RELEASE(mMenuBarParent);
    NS_IF_RELEASE(mMenuParent);
    NS_IF_RELEASE(mListener);

    // Free out menu items.
    RemoveAll();

    delete mMenu;
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsISupports *aParent, const nsString &aLabel)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::Create()\n"));
    if (aParent)
    {
        nsIMenuBar * menubar = nsnull;
        aParent->QueryInterface(kIMenuBarIID, (void**) &menubar);
        if (menubar)
        {
            mMenuBarParent = menubar;
            NS_ADDREF(mMenuBarParent);
            NS_RELEASE(menubar);
        }
        else
        {
            nsIMenu * menu = nsnull;
            aParent->QueryInterface(kIMenuIID, (void**) &menu);
            if (menu)
            {
                mMenuParent = menu;
                NS_ADDREF(mMenuParent);
                NS_RELEASE(menu);
            }
        }
    }

    void * aData = nsnull;

    if (mMenuBarParent)
    {
        // parent is a menu bar.
        mMenuBarParent->GetNativeData(aData);
    }
    else if (mMenuParent)
    {
        // parent is a menu.
        mMenuParent->GetNativeData(&aData);
    }

    QWidget * parent = (QWidget *) aData;

    mLabel = aLabel;
    mMenu = new QPopupMenu(parent, QPopupMenu::tr("nsMenu"));

    //mEventHandler = nsQEventHandler::Instance(mMenu, this);

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetParent()\n"));
    aParent = nsnull;
    if (nsnull != mMenuParent) 
    {
        return mMenuParent->QueryInterface(kISupportsIID,(void**)&aParent);
    } 
    else if (nsnull != mMenuBarParent) 
    {
        return mMenuBarParent->QueryInterface(kISupportsIID,(void**)&aParent);
    }

    return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetLabel()\n"));
    aText = mLabel;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsString &aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetLabel()\n"));
    /* we Do GetLabel() when we are adding the menu...
     * but we might want to redo this.
     */
    mLabel = aText;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
    aText = mAccessKey;
    char *foo = mAccessKey.ToNewCString();
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetAccessKey returns \"%s\"\n",
                                       foo));
    delete [] foo;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
    mAccessKey = aText;
    char *foo = mAccessKey.ToNewCString();
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetAccessKey to \"%s\"\n",
                                       foo));
    delete [] foo;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::AddItem()\n"));
    if (aItem)
    {
        nsIMenuItem * menuitem = nsnull;
        aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);
        if (menuitem)
        {
            AddMenuItem(menuitem);
            NS_RELEASE(menuitem);
        }
        else
        {
            nsIMenu * menu = nsnull;
            aItem->QueryInterface(kIMenuIID, (void**) &menu);
            if (menu)
            {
                AddMenu(menu);
                NS_RELEASE(menu);
            }
        }
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::AddMenuItem()\n"));
    QString * string;
    void * voidData;
  
    aMenuItem->GetNativeData(voidData);
    string = (QString *) voidData;

    mMenu->insertItem(*string, mEventHandler, SLOT(MenuItemActivated()));

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::AddMenu()\n"));
    nsString Label;
    QPopupMenu * menu = nsnull;
    char * labelStr;
    void * voidData = NULL;

    aMenu->GetLabel(Label);

    labelStr = Label.ToNewCString();

    QString string = labelStr;

    aMenu->GetNativeData(&voidData);
    menu = (QPopupMenu *) voidData;

    mMenu->insertItem(string, menu);

    delete[] labelStr;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::AddSeparator()\n"));
    mMenu->insertSeparator();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetItemCount()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetItemAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::InsertItemAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aPos)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::InsertSeparator()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::RemoveItem()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::RemoveAll()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetNativeData(void * aData)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetNativeData(void ** aData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetNativeData()\n"));
    *aData = (void *) mMenu;
    return NS_OK;
}

//-------------------------------------------------------------------------
QWidget *nsMenu::GetNativeParent()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetNativeParent()\n"));
    void * voidData;

    if (nsnull != mMenuParent) 
    {
        mMenuParent->GetNativeData(&voidData);
    } 
    else if (nsnull != mMenuBarParent) 
    {
        mMenuBarParent->GetNativeData(voidData);
    } 
    else 
    {
        return nsnull;
    }

    return (QWidget *) voidData;
}

//-------------------------------------------------------------------------
// Set DOMNode
//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetDOMNode()\n"));
    mDOMNode = aMenuNode;
    return NS_OK;
}

//-------------------------------------------------------------------------
// Set DOMElement
//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetDOMElement()\n"));
    mDOMElement = aMenuElement;
    return NS_OK;
}
    
//-------------------------------------------------------------------------
// Set WebShell
//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetWebShell()\n"));
    mWebShell = aWebShell;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::AddMenuListener()\n"));
    mListener = aMenuListener;
    NS_ADDREF(mListener);
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::RemoveMenuListener()\n"));
    if (aMenuListener == mListener) 
    {
        NS_IF_RELEASE(mListener);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
// Set enabled state
//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::SetEnabled()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
// Get enabled state
//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetEnabled(PRBool* aIsEnabled)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::GetEnabled()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
// Query if this is the help menu
//-------------------------------------------------------------------------
NS_METHOD nsMenu::IsHelpMenu(PRBool* aIsHelpMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::IsHelpMenu()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::MenuItemSelected()\n"));
    if (nsnull != mListener) 
    {
        mListener->MenuSelected(aMenuEvent);
    }
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::MenuSelected()\n"));
    if (nsnull != mListener) 
    {
        mListener->MenuSelected(aMenuEvent);
    }
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::MenuDeselected()\n"));
    if (nsnull != mListener) 
    {
        mListener->MenuDeselected(aMenuEvent);
    }
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                    nsIWidget         * aParentWindow, 
                                    void              * menuNode,
                                    void              * aWebShell)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::MenuConstruct()\n"));
    if (nsnull != mListener) 
    {
        mListener->MenuDeselected(aMenuEvent);
    }
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenu::MenuDestruct()\n"));
    if (nsnull != mListener) 
    {
        mListener->MenuDeselected(aMenuEvent);
    }
    return nsEventStatus_eIgnore;
}

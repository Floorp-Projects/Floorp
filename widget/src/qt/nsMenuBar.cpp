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

#include "nsMenuBar.h"
#include "nsIMenu.h"
#include "nsIWidget.h"

#include "nsString.h"
#include "nsReadableUtils.h"

#include "nsQEventHandler.h"

static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
//NS_IMPL_ISUPPORTS1(nsMenuBar, nsIMenuBar)

nsresult nsMenuBar::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    *aInstancePtr = NULL;

    if (aIID.Equals(kIMenuBarIID)) 
    {
        *aInstancePtr = (void*) ((nsIMenuBar*) this);
        NS_ADDREF_THIS();
        return NS_OK;
    }                                                                      
    if (aIID.Equals(kISupportsIID)) 
    {                                      
        *aInstancePtr = (void*) ((nsISupports*)(nsIMenuBar*) this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIMenuListener))) 
    {                                      
        *aInstancePtr = (void*) ((nsIMenuListener*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsMenuBar)
NS_IMPL_RELEASE(nsMenuBar)

//-------------------------------------------------------------------------
//
// nsMenuBar constructor
//
//-------------------------------------------------------------------------
nsMenuBar::nsMenuBar() : nsIMenuBar(), nsIMenuListener()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::nsMenuBar()\n"));
    NS_INIT_REFCNT();
    mNumMenus       = 0;
    mMenuBar        = nsnull;
    mParent         = nsnull;
    mIsMenuBarAdded = PR_FALSE;
    mEventHandler   = nsnull;
}

//-------------------------------------------------------------------------
//
// nsMenuBar destructor
//
//-------------------------------------------------------------------------
nsMenuBar::~nsMenuBar()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::~nsMenuBar()\n"));
    NS_IF_RELEASE(mParent);
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Create(nsIWidget *aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::Create()\n"));
    SetParent(aParent);
    mMenuBar = new QMenuBar(NULL, QMenuBar::tr("nsMenuBar"));

    mParent->SetMenuBar(this);

    mMenuBar->show();

    //mEventHandler = nsQEventHandler::Instance();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetParent(nsIWidget *&aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::GetParent()\n"));
    aParent = mParent;
    NS_IF_ADDREF(aParent);
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetParent(nsIWidget *aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::SetParent()\n"));
    NS_IF_RELEASE(mParent);
    mParent = aParent;
    NS_IF_ADDREF(mParent);
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::AddMenu(nsIMenu * aMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::AddMenu()\n"));
    nsString Label;
    QPopupMenu * menu;
    char *labelStr;
    void *voidData;

    aMenu->GetLabel(Label);

    labelStr = ToNewCString(Label);

    QString string = labelStr;

    aMenu->GetNativeData(&voidData);
    menu = (QPopupMenu *) voidData;

    ((QMenuBar *)mMenuBar)->insertItem(string, menu);

    delete[] labelStr;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuCount(PRUint32 &aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::GetMenuCount()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::GetMenuAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::InsertMenuAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveMenu(const PRUint32 aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::RemoveMenu()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::RemoveAll()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::RemoveAll()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::GetNativeData(void *& aData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::GetNativeData()\n"));
    aData = (void *) mMenuBar;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::SetNativeData(void * aData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::SetNativeData()\n"));
    mMenuBar = (QMenuBar *) aData;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBar::Paint()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::Paint()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenuBar::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::MenuItemSelected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuSelected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::MenuSelected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::MenuDeselected()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                       nsIWidget         * aParentWindow, 
                                       void              * menuNode,
                                       void              * aWebShell)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::MenuConstruct()\n"));
    return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuBar::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsMenuBar::MenuDestruct()\n"));
    return nsEventStatus_eIgnore;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsMenuBar.h"
#include "nsIMenu.h"
#include "nsIWidget.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include "nsQEventHandler.h"

static NS_DEFINE_IID(kIMenuBarIID, NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
//NS_IMPL_ISUPPORTS(nsMenuBar, kMenuBarIID)

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
    if (aIID.Equals(kIMenuListenerIID)) 
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

    labelStr = Label.ToNewCString();

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

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
#include "nsPopUpMenu.h"
#include "nsIPopUpMenu.h"
#include "nsIMenu.h"
#include "nsIWidget.h"

#include "nsString.h"
#include "nsStringUtil.h"
#include "nsQEventHandler.h"

static NS_DEFINE_IID(kPopUpMenuIID, NS_IPOPUPMENU_IID);
NS_IMPL_ISUPPORTS(nsPopUpMenu, kPopUpMenuIID)

//-------------------------------------------------------------------------
//
// nsPopUpMenu constructor
//
//-------------------------------------------------------------------------
nsPopUpMenu::nsPopUpMenu() : nsIPopUpMenu()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::nsPopUpMenu()\n"));
    NS_INIT_REFCNT();
    mNumMenuItems = 0;
    mParent       = nsnull;
    mMenu         = nsnull;
    mEventHandler = nsnull;
}

//-------------------------------------------------------------------------
//
// nsPopUpMenu destructor
//
//-------------------------------------------------------------------------
nsPopUpMenu::~nsPopUpMenu()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::~nsPopUpMenu()\n"));
    NS_IF_RELEASE(mParent);

    if (mMenu)
    {
        delete mMenu;
    }
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::Create(nsIWidget *aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::Create()\n"));
    mParent = aParent;
    NS_ADDREF(mParent);

    QWidget * parent = (QWidget *) mParent->GetNativeData(NS_NATIVE_WINDOW);

    //mEventHandler = nsQEventHandler::Instance();

    mMenu = new QPopupMenu(parent, QPopupMenu::tr("nsPopUpMenu"));

    return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddItem(const nsString &aText)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::AddItem()\n"));
    char * labelStr = mLabel.ToNewCString();

    mMenu->insertItem(labelStr);

    delete[] labelStr;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddItem(nsIMenuItem * aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::AddItem()\n"));
    QString * string;
    void *voidData;
  
    aMenuItem->GetNativeData(voidData);
    string = (QString *) voidData;

    mMenu->insertItem(*string);

    // XXX add aMenuItem to internal data structure list
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddMenu(nsIMenu * aMenu)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::AddMenu()\n"));
    nsString Label;
    QPopupMenu * newmenu = nsnull;
    char *labelStr;
    void *voidData=NULL;
  
    aMenu->GetLabel(Label);

    labelStr = Label.ToNewCString();

    aMenu->GetNativeData(&voidData);
    newmenu = (QPopupMenu *) voidData;

    mMenu->insertItem(labelStr, newmenu);

    delete[] labelStr;

    // XXX add aMenu to internal data structor list
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::AddSeparator() 
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::AddSeparator()\n"));
    mMenu->insertSeparator();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetItemCount(PRUint32 &aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::GetItemCount()\n"));
    aCount = mMenu->count();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetItemAt(const PRUint32 aCount, 
                                 nsIMenuItem *& aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::GetItemAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertItemAt(const PRUint32 aCount, 
                                    nsIMenuItem *& aMenuItem)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::InsertItemAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertItemAt(const PRUint32 aCount, 
                                    const nsString & aMenuItemName)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::InsertItemAt()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::InsertSeparator(const PRUint32 aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::InsertSeparator()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::RemoveItem(const PRUint32 aCount)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::RemoveItem()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::RemoveAll()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::RemoveAll()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::ShowMenu(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu:ShowMenu()\n"));
    mX = aX;
    mY = aY;

    QPoint pos(mX, mY);

    mMenu->popup(pos);

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetNativeData(void *& aData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::GetNativeData()\n"));
    aData = (void *)mMenu;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsPopUpMenu::GetParent(nsIWidget *& aParent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsPopUpMenu::GetParent()\n"));
    aParent = mParent;
    return NS_OK;
}



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

#include "nsClipboard.h"

// XXXX #include "nsDataObj.h"
#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsITransferable.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsWidget.h"
#include <qapplication.h>

// XXXX #include "DDCOMM.h"

// interface definitions
static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::nsClipboard()\n"));
    //NS_INIT_REFCNT();
    mIgnoreEmptyNotification = PR_FALSE;
    mWindow         = nsnull;

    // Create a Native window for the shell container...
    //nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
    //mWindow->Show(PR_FALSE);
    //mWindow->Resize(1,1,PR_FALSE);
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::~nsClipboard()\n"));
    NS_IF_RELEASE(mWindow);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_NOINTERFACE;

    static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
    if (aIID.Equals(kIClipboard)) 
    {
        *aInstancePtr = (void*) ((nsIClipboard*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetNativeClipboardData()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::SetNativeClipboardData()\n"));
    mIgnoreEmptyNotification = PR_TRUE;

    // make sure we have a good transferable
    if (nsnull == mTransferable) 
    {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP 
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::GetNativeClipboardData()\n"));
    // make sure we have a good transferable
    if (nsnull == aTransferable) 
    {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsClipboard::ForceDataToClipboard()\n"));
    // make sure we have a good transferable
    if (nsnull == mTransferable) 
    {
        return NS_ERROR_FAILURE;
    }


    return NS_OK;
}


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

#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "plevent.h"
#include "nsWidget.h"
#include "nsQApplication.h"

//-------------------------------------------------------------------------
//
// constructor
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::nsToolkit()\n"));
    NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// destructor
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::~nsToolkit()\n"));
}

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsToolkit::Init(PRThread *aThread)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsToolkit::Init()\n"));
    return NS_OK;
}

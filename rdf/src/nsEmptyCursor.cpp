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

#include "nscore.h"
#include "nsIRDFCursor.h"

static NS_DEFINE_IID(kIRDFCursorIID, NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);

class EmptyCursorImpl : public nsIRDFCursor {
public:
    EmptyCursorImpl(void);
    virtual ~EmptyCursorImpl(void) {};

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);
    NS_IMETHOD QueryInterface(REFNSIID iid, void** result);

    // nsIRDFCursor
    NS_IMETHOD HasMoreElements(PRBool& result);
    NS_IMETHOD GetNext(nsIRDFNode*& next, PRBool& tv);
};

nsIRDFCursor* gEmptyCursor;
static EmptyCursorImpl gEmptyCursorImpl;

EmptyCursorImpl::EmptyCursorImpl(void)
{
    gEmptyCursor = this;
}

NS_IMETHODIMP_(nsrefcnt)
EmptyCursorImpl::AddRef(void)
{
    return 2;
}


NS_IMETHODIMP_(nsrefcnt)
EmptyCursorImpl::Release(void)
{
    return 1;
}

NS_IMETHODIMP
EmptyCursorImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


NS_IMETHODIMP
EmptyCursorImpl::HasMoreElements(PRBool& result)
{
    result = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
EmptyCursorImpl::GetNext(nsIRDFNode*& next, PRBool& tv)
{
    next = nsnull;
    return NS_ERROR_UNEXPECTED;
}



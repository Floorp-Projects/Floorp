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

/*

  An empty enumerator.

 */

#include "nsIEnumerator.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////

class EmptyEnumeratorImpl : public nsISimpleEnumerator
{
public:
    EmptyEnumeratorImpl(void) {};
    virtual ~EmptyEnumeratorImpl(void) {};

    // nsISupports interface
    NS_IMETHOD_(nsrefcnt) AddRef(void) {
        return 2;
    }

    NS_IMETHOD_(nsrefcnt) Release(void) {
        return 1;
    }

    NS_IMETHOD QueryInterface(REFNSIID iid, void** result) {
        if (! result)
            return NS_ERROR_NULL_POINTER;

        if (iid.Equals(nsISimpleEnumerator::GetIID()) ||
            iid.Equals(kISupportsIID)) {
            *result = (nsISimpleEnumerator*) this;
            NS_ADDREF(this);
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }

    // nsISimpleEnumerator
    NS_IMETHOD HasMoreElements(PRBool* aResult) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports** aResult) {
        return NS_ERROR_UNEXPECTED;
    }
};

extern "C" NS_COM nsresult
NS_NewEmptyEnumerator(nsISimpleEnumerator** aResult)
{
    static EmptyEnumeratorImpl gEmptyEnumerator;
    *aResult = &gEmptyEnumerator;
    return NS_OK;
}


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

static NS_DEFINE_IID(kIRDFArcsInCursorIID,    NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,   NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID, NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,          NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////

class EmptyAssertionCursorImpl : public nsIRDFAssertionCursor
{
public:
    EmptyAssertionCursorImpl(void) {};
    virtual ~EmptyAssertionCursorImpl(void) {};

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef(void) {
        return 2;
    }

    NS_IMETHOD_(nsrefcnt) Release(void) {
        return 1;
    }

    NS_IMETHOD QueryInterface(REFNSIID iid, void** result) {
        if (! result)
            return NS_ERROR_NULL_POINTER;

        if (iid.Equals(kIRDFAssertionCursorIID) ||
            iid.Equals(kIRDFCursorIID) ||
            iid.Equals(kISupportsIID)) {
            *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
            /* AddRef(); // not necessary */
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }

    // nsIRDFCursor
    NS_IMETHOD Advance(void) {
        return NS_ERROR_RDF_CURSOR_EMPTY;
    }

    // nsIRDFAssertionCursor
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetSubject(nsIRDFResource** aResource) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetObject(nsIRDFNode** aObject) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetTruthValue(PRBool* aTruthValue) {
        return NS_ERROR_UNEXPECTED;
    }
};

nsresult
NS_NewEmptyRDFAssertionCursor(nsIRDFAssertionCursor** result)
{
    static EmptyAssertionCursorImpl gEmptyAssertionCursor;
    *result = &gEmptyAssertionCursor;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

class EmptyArcsOutCursorImpl : public nsIRDFArcsOutCursor
{
public:
    EmptyArcsOutCursorImpl(void) {};
    virtual ~EmptyArcsOutCursorImpl(void) {};

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef(void) {
        return 2;
    }

    NS_IMETHOD_(nsrefcnt) Release(void) {
        return 1;
    }

    NS_IMETHOD QueryInterface(REFNSIID iid, void** result) {
        if (! result)
            return NS_ERROR_NULL_POINTER;

        if (iid.Equals(kIRDFArcsOutCursorIID) ||
            iid.Equals(kIRDFCursorIID) ||
            iid.Equals(kISupportsIID)) {
            *result = NS_STATIC_CAST(nsIRDFArcsOutCursor*, this);
            /* AddRef(); // not necessary */
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }

    // nsIRDFCursor
    NS_IMETHOD Advance(void) {
        return NS_ERROR_RDF_CURSOR_EMPTY;
    }

    // nsIRDFArcsOutCursor
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetSubject(nsIRDFResource** aResource) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetTruthValue(PRBool* aTruthValue) {
        return NS_ERROR_UNEXPECTED;
    }
};

nsresult
NS_NewEmptyRDFArcsOutCursor(nsIRDFArcsOutCursor** result)
{
    static EmptyArcsOutCursorImpl gEmptyArcsOutCursor;
    *result = &gEmptyArcsOutCursor;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

class EmptyArcsInCursorImpl : public nsIRDFArcsInCursor
{
public:
    EmptyArcsInCursorImpl(void) {};
    virtual ~EmptyArcsInCursorImpl(void) {};

    // nsISupports
    NS_IMETHOD_(nsrefcnt) AddRef(void) {
        return 2;
    }

    NS_IMETHOD_(nsrefcnt) Release(void) {
        return 1;
    }

    NS_IMETHOD QueryInterface(REFNSIID iid, void** result) {
        if (! result)
            return NS_ERROR_NULL_POINTER;

        if (iid.Equals(kIRDFArcsInCursorIID) ||
            iid.Equals(kIRDFCursorIID) ||
            iid.Equals(kISupportsIID)) {
            *result = NS_STATIC_CAST(nsIRDFArcsInCursor*, this);
            /* AddRef(); // not necessary */
            return NS_OK;
        }
        return NS_NOINTERFACE;
    }

    // nsIRDFCursor
    NS_IMETHOD Advance(void) {
        return NS_ERROR_RDF_CURSOR_EMPTY;
    }

    // nsIRDFArcsInCursor
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetObject(nsIRDFNode** aNode) {
        return NS_ERROR_UNEXPECTED;
    }

    NS_IMETHOD GetTruthValue(PRBool* aTruthValue) {
        return NS_ERROR_UNEXPECTED;
    }
};

nsresult
NS_NewEmptyRDFArcsInCursor(nsIRDFArcsInCursor** result)
{
    static EmptyArcsInCursorImpl gEmptyArcsInCursor;
    *result = &gEmptyArcsInCursor;
    return NS_OK;
}

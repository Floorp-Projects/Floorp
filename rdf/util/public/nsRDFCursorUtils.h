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

#ifndef nsRDFCursorUtils_h__
#define nsRDFCursorUtils_h__

#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsSupportsArrayEnumerator.h"
#include "nsIEnumerator.h"

#define NS_INHERIT_QUERYINTERFACE1(FromClass, NewInterface)         \
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) { \
        if (!aInstancePtr) return NS_ERROR_NULL_POINTER;            \
        if (aIID.Equals(NewInterface::IID())) {                     \
            *aInstancePtr = NS_STATIC_CAST(NewInterface*, this);    \
            AddRef();                                               \
            return NS_OK;                                           \
        }                                                           \
        return FromClass::QueryInterface(aIID, aInstancePtr);       \
    }                                                               \

#define NS_INHERIT_QUERYINTERFACE(FromClass)                        \
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) { \
        return FromClass::QueryInterface(aIID, aInstancePtr);       \
    }                                                               \

#define NS_INHERIT_ADDREF(FromClass)                                \
    NS_IMETHOD_(nsrefcnt) AddRef(void) {                            \
        return FromClass::AddRef();                                 \
    }                                                               \

#define NS_INHERIT_RELEASE(FromClass)                               \
    NS_IMETHOD_(nsrefcnt) Release(void) {                           \
        return FromClass::Release();                                \
    }                                                               \

#define NS_INHERIT_ISUPPORTS(FromClass)                             \
    NS_INHERIT_QUERYINTERFACE(FromClass)                            \
    NS_INHERIT_ADDREF(FromClass)                                    \
    NS_INHERIT_RELEASE(FromClass)                                   \

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFArrayCursor : public nsSupportsArrayEnumerator, 
                                public nsIRDFCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsSupportsArrayEnumerator, nsIRDFCursor)
    NS_INHERIT_ADDREF(nsSupportsArrayEnumerator)
    NS_INHERIT_RELEASE(nsSupportsArrayEnumerator)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsRDFArrayCursor methods:
    nsRDFArrayCursor(nsIRDFDataSource* aDataSource,
                     nsISupportsArray* valueArray);
    virtual ~nsRDFArrayCursor(void);

protected:
    nsIRDFDataSource* mDataSource;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFArrayAssertionCursor : public nsRDFArrayCursor, 
                                         public nsIRDFAssertionCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFArrayCursor, nsIRDFAssertionCursor)
    NS_INHERIT_ADDREF(nsRDFArrayCursor)
    NS_INHERIT_RELEASE(nsRDFArrayCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFArrayCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFArrayCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFArrayCursor::GetValue(aValue);
    }

    // nsIRDFAssertionCursor methods:
    NS_IMETHOD GetSubject(nsIRDFResource* *aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource* *aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode* *aObject);
    NS_IMETHOD GetTruthValue(PRBool *aTruthValue);

    // nsRDFArrayAssertionCursor methods:
    nsRDFArrayAssertionCursor(nsIRDFDataSource* aDataSource,
                              nsIRDFResource* subject,
                              nsIRDFResource* predicate,
                              nsISupportsArray* objectsArray,
                              PRBool truthValue = PR_TRUE);
    virtual ~nsRDFArrayAssertionCursor();
    
protected:
    nsIRDFResource* mSubject;
    nsIRDFResource* mPredicate;
    PRBool mTruthValue;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFSingletonAssertionCursor : public nsIRDFAssertionCursor
{
public:
    NS_DECL_ISUPPORTS

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFAssertionCursor methods:
    NS_IMETHOD GetSubject(nsIRDFResource* *aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource* *aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode* *aObject);
    NS_IMETHOD GetTruthValue(PRBool *aTruthValue);

    // nsRDFSingletonAssertionCursor methods:

    // node == subject if inverse == false
    // node == target if inverse == true
    // value computed when accessed from datasource
    nsRDFSingletonAssertionCursor(nsIRDFDataSource* aDataSource,
                                  nsIRDFNode* node,
                                  nsIRDFResource* predicate,
                                  PRBool inverse = PR_FALSE,
                                  PRBool truthValue = PR_TRUE);
    virtual ~nsRDFSingletonAssertionCursor();

protected:
    nsIRDFDataSource* mDataSource;
    nsIRDFNode* mNode;
    nsIRDFResource* mPredicate;
    nsIRDFNode* mValue;
    PRBool mInverse;
    PRBool mTruthValue;
    PRBool mConsumed;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFArrayArcsCursor : public nsRDFArrayCursor
{
public:
    NS_INHERIT_ISUPPORTS(nsRDFArrayCursor)

    // nsRDFArrayArcsCursor methods:
    nsRDFArrayArcsCursor(nsIRDFDataSource* aDataSource,
                         nsIRDFNode* node,
                         nsIRDFResource* predicate,
                         nsISupportsArray* arcs);
    virtual ~nsRDFArrayArcsCursor();

protected:
    nsresult GetPredicate(nsIRDFResource** aPredicate) {
        *aPredicate = mPredicate;
        NS_ADDREF(mPredicate);
        return NS_OK;
    }

    nsresult GetNode(nsIRDFNode* *result) {
        *result = mNode;
        NS_ADDREF(mNode);
        return NS_OK;
    }
    
    nsIRDFNode* mNode;
    nsIRDFResource* mPredicate;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFArrayArcsOutCursor : public nsRDFArrayArcsCursor,
                                       public nsIRDFArcsOutCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFArrayArcsCursor, nsIRDFArcsOutCursor)
    NS_INHERIT_ADDREF(nsRDFArrayArcsCursor)
    NS_INHERIT_RELEASE(nsRDFArrayArcsCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFArrayArcsCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFArrayArcsCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFArrayArcsCursor::GetValue(aValue);
    }

    // nsIRDFArcsOutCursor methods:
    NS_IMETHOD GetSubject(nsIRDFResource** aSubject) {
        return GetNode((nsIRDFNode**)aSubject);
    }
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return nsRDFArrayArcsCursor::GetPredicate(aPredicate);
    }

    // nsRDFArrayArcsOutCursor methods:
    nsRDFArrayArcsOutCursor(nsIRDFDataSource* aDataSource,
                            nsIRDFResource* subject,
                            nsIRDFResource* predicate,
                            nsISupportsArray* arcs)
        : nsRDFArrayArcsCursor(aDataSource, subject,
                               predicate, arcs) {}
    virtual ~nsRDFArrayArcsOutCursor() {}
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFArrayArcsInCursor : public nsRDFArrayArcsCursor,
                                      public nsIRDFArcsInCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFArrayArcsCursor, nsIRDFArcsInCursor)
    NS_INHERIT_ADDREF(nsRDFArrayArcsCursor)
    NS_INHERIT_RELEASE(nsRDFArrayArcsCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFArrayArcsCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFArrayArcsCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFArrayArcsCursor::GetValue(aValue);
    }

    // nsIRDFArcsInCursor methods:
    NS_IMETHOD GetObject(nsIRDFNode** aObject) {
        return GetNode(aObject);
    }
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return nsRDFArrayArcsCursor::GetPredicate(aPredicate);
    }

    // nsRDFArrayArcsInCursor methods:
    nsRDFArrayArcsInCursor(nsIRDFDataSource* aDataSource,
                            nsIRDFNode* object,
                            nsIRDFResource* predicate,
                            nsISupportsArray* arcs)
        : nsRDFArrayArcsCursor(aDataSource, object,
                               predicate, arcs) {}
    virtual ~nsRDFArrayArcsInCursor() {}
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFEnumeratorCursor : public nsIRDFCursor
{
public:
    NS_DECL_ISUPPORTS

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsRDFEnumeratorCursor methods:
    nsRDFEnumeratorCursor(nsIRDFDataSource* aDataSource,
                          nsIEnumerator* valueEnumerator);
    virtual ~nsRDFEnumeratorCursor(void);

protected:
    nsIRDFDataSource* mDataSource;
    nsIEnumerator* mEnum;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFEnumeratorAssertionCursor : public nsRDFEnumeratorCursor, 
                                              public nsIRDFAssertionCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFEnumeratorCursor, nsIRDFAssertionCursor)
    NS_INHERIT_ADDREF(nsRDFEnumeratorCursor)
    NS_INHERIT_RELEASE(nsRDFEnumeratorCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFEnumeratorCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFEnumeratorCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFEnumeratorCursor::GetValue(aValue);
    }

    // nsIRDFAssertionCursor methods:
    NS_IMETHOD GetSubject(nsIRDFResource* *aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource* *aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode* *aObject);
    NS_IMETHOD GetTruthValue(PRBool *aTruthValue);

    // nsRDFEnumeratorAssertionCursor methods:
    nsRDFEnumeratorAssertionCursor(nsIRDFDataSource* aDataSource,
                                   nsIRDFResource* subject,
                                   nsIRDFResource* predicate,
                                   nsIEnumerator* objectsEnumerator,
                                   PRBool truthValue = PR_TRUE);
    virtual ~nsRDFEnumeratorAssertionCursor();
    
protected:
    nsIRDFResource* mSubject;
    nsIRDFResource* mPredicate;
    PRBool mTruthValue;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFEnumeratorArcsCursor : public nsRDFEnumeratorCursor
{
public:
    NS_INHERIT_ISUPPORTS(nsRDFEnumeratorCursor)

    // nsRDFEnumeratorArcsOutCursor methods:
    nsRDFEnumeratorArcsCursor(nsIRDFDataSource* aDataSource,
                              nsIRDFNode* node,
                              nsIRDFResource* predicate,
                              nsIEnumerator* arcs);
    virtual ~nsRDFEnumeratorArcsCursor();

protected:
    nsresult GetPredicate(nsIRDFResource** aPredicate) {
        *aPredicate = mPredicate;
        NS_ADDREF(mPredicate);
        return NS_OK;
    }

    nsresult GetNode(nsIRDFNode* *result) {
        *result = mNode;
        NS_ADDREF(mNode);
        return NS_OK;
    }
    
    nsIRDFNode* mNode;
    nsIRDFResource* mPredicate;
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFEnumeratorArcsOutCursor : public nsRDFEnumeratorArcsCursor,
                                            public nsIRDFArcsOutCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFEnumeratorArcsCursor, nsIRDFArcsOutCursor)
    NS_INHERIT_ADDREF(nsRDFEnumeratorArcsCursor)
    NS_INHERIT_RELEASE(nsRDFEnumeratorArcsCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFEnumeratorArcsCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFEnumeratorArcsCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFEnumeratorArcsCursor::GetValue(aValue);
    }

    // nsIRDFArcsOutCursor methods:
    NS_IMETHOD GetSubject(nsIRDFResource** aSubject) {
        return GetNode((nsIRDFNode**)aSubject);
    }
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return nsRDFEnumeratorArcsCursor::GetPredicate(aPredicate);
    }

    // nsRDFEnumeratorArcsOutCursor methods:
    nsRDFEnumeratorArcsOutCursor(nsIRDFDataSource* aDataSource,
                                 nsIRDFResource* subject,
                                 nsIRDFResource* predicate,
                                 nsIEnumerator* arcs)
        : nsRDFEnumeratorArcsCursor(aDataSource, subject,
                                    predicate, arcs) {}
    virtual ~nsRDFEnumeratorArcsOutCursor() {}
};

////////////////////////////////////////////////////////////////////////////////

class NS_RDF nsRDFEnumeratorArcsInCursor : public nsRDFEnumeratorArcsCursor,
                                           public nsIRDFArcsInCursor
{
public:
    NS_INHERIT_QUERYINTERFACE1(nsRDFEnumeratorArcsCursor, nsIRDFArcsInCursor)
    NS_INHERIT_ADDREF(nsRDFEnumeratorArcsCursor)
    NS_INHERIT_RELEASE(nsRDFEnumeratorArcsCursor)

    // nsIRDFCursor methods:
    NS_IMETHOD Advance(void) { 
        return nsRDFEnumeratorArcsCursor::Advance();
    }
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) {
        return nsRDFEnumeratorArcsCursor::GetDataSource(aDataSource);
    }
    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return nsRDFEnumeratorArcsCursor::GetValue(aValue);
    }

    // nsIRDFArcsInCursor methods:
    NS_IMETHOD GetObject(nsIRDFNode** aObject) {
        return GetNode(aObject);
    }
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {
        return nsRDFEnumeratorArcsCursor::GetPredicate(aPredicate);
    }

    // nsRDFEnumeratorArcsInCursor methods:
    nsRDFEnumeratorArcsInCursor(nsIRDFDataSource* aDataSource,
                                nsIRDFNode* object,
                                nsIRDFResource* predicate,
                                nsIEnumerator* arcs)
        : nsRDFEnumeratorArcsCursor(aDataSource, object,
                                    predicate, arcs) {}
    virtual ~nsRDFEnumeratorArcsInCursor() {}
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsRDFCursorUtils_h__ */

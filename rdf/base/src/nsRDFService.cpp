/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

/*

  This file provides the implementation for the RDF service manager.

  TO DO
  -----

  1) Implement the CreateDataBase() methods.

  2) Cache date and int literals.

 */

#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsIAtom.h"
#include "nsIComponentManager.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
#include "pldhash.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"
#include "rdf.h"
#include "nsCRT.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFXMLDataSourceCID,    NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFDefaultResourceCID,  NS_RDFDEFAULTRESOURCE_CID);

static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFDateIID,         NS_IRDFDATE_IID);
static NS_DEFINE_IID(kIRDFIntIID,         NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog = nsnull;
#endif

class BlobImpl;

////////////////////////////////////////////////////////////////////////
// RDFServiceImpl
//
//   This is the RDF service.
//
class RDFServiceImpl : public nsIRDFService,
                       public nsSupportsWeakReference
{
protected:
    PLHashTable* mNamedDataSources;
    PLDHashTable mResources;
    PLDHashTable mLiterals;
    PLDHashTable mInts;
    PLDHashTable mDates;
    PLDHashTable mBlobs;

    nsCAutoString mLastURIPrefix;
    nsCOMPtr<nsIFactory> mLastFactory;
    nsCOMPtr<nsIFactory> mDefaultResourceFactory;

    RDFServiceImpl();
    nsresult Init();
    virtual ~RDFServiceImpl();

public:

    static nsresult GetRDFService(nsIRDFService** result);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFService
    NS_DECL_NSIRDFSERVICE

    // Implementation methods
    nsresult RegisterLiteral(nsIRDFLiteral* aLiteral);
    nsresult UnregisterLiteral(nsIRDFLiteral* aLiteral);
    nsresult RegisterInt(nsIRDFInt* aInt);
    nsresult UnregisterInt(nsIRDFInt* aInt);
    nsresult RegisterDate(nsIRDFDate* aDate);
    nsresult UnregisterDate(nsIRDFDate* aDate);
    nsresult RegisterBlob(BlobImpl* aBlob);
    nsresult UnregisterBlob(BlobImpl* aBlob);

    nsresult GetDataSource(const char *aURI, PRBool aBlock, nsIRDFDataSource **aDataSource );
};

static RDFServiceImpl* gRDFService; // The one-and-only RDF service


// These functions are copied from nsprpub/lib/ds/plhash.c, with one
// change to free the key in DataSourceFreeEntry.
// XXX sigh, why were DefaultAllocTable et. al. declared static, anyway?

static void * PR_CALLBACK
DataSourceAllocTable(void *pool, PRSize size)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    return PR_MALLOC(size);
}

static void PR_CALLBACK
DataSourceFreeTable(void *pool, void *item)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    PR_Free(item);
}

static PLHashEntry * PR_CALLBACK
DataSourceAllocEntry(void *pool, const void *key)
{
#if defined(XP_MAC)
#pragma unused (pool,key)
#endif

    return PR_NEW(PLHashEntry);
}

static void PR_CALLBACK
DataSourceFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    if (flag == HT_FREE_ENTRY) {
        PL_strfree((char*) he->key);
        PR_Free(he);
    }
}

static PLHashAllocOps dataSourceHashAllocOps = {
    DataSourceAllocTable, DataSourceFreeTable,
    DataSourceAllocEntry, DataSourceFreeEntry
};

//----------------------------------------------------------------------
//
// For the mResources hashtable.
//

struct ResourceHashEntry : public PLDHashEntryHdr {
    const char *mKey;
    nsIRDFResource *mResource;

    static const void * PR_CALLBACK
    GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
    {
        ResourceHashEntry *entry = NS_STATIC_CAST(ResourceHashEntry *, hdr);
        return entry->mKey;
    }

    static PLDHashNumber PR_CALLBACK
    HashKey(PLDHashTable *table, const void *key)
    {
        return nsCRT::HashCode(NS_STATIC_CAST(const char *, key));
    }

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
               const void *key)
    {
        const ResourceHashEntry *entry =
            NS_STATIC_CAST(const ResourceHashEntry *, hdr);

        return 0 == nsCRT::strcmp(NS_STATIC_CAST(const char *, key),
                                  entry->mKey);
    }
};

static PLDHashTableOps gResourceTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    ResourceHashEntry::GetKey,
    ResourceHashEntry::HashKey,
    ResourceHashEntry::MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

// ----------------------------------------------------------------------
//
// For the mLiterals hashtable.
//

struct LiteralHashEntry : public PLDHashEntryHdr {
    nsIRDFLiteral *mLiteral;
    const PRUnichar *mKey;

    static const void * PR_CALLBACK
    GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
    {
        LiteralHashEntry *entry = NS_STATIC_CAST(LiteralHashEntry *, hdr);
        return entry->mKey;
    }

    static PLDHashNumber PR_CALLBACK
    HashKey(PLDHashTable *table, const void *key)
    {
        return nsCRT::HashCode(NS_STATIC_CAST(const PRUnichar *, key));
    }

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
               const void *key)
    {
        const LiteralHashEntry *entry =
            NS_STATIC_CAST(const LiteralHashEntry *, hdr);

        return 0 == nsCRT::strcmp(NS_STATIC_CAST(const PRUnichar *, key),
                                  entry->mKey);
    }
};

static PLDHashTableOps gLiteralTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    LiteralHashEntry::GetKey,
    LiteralHashEntry::HashKey,
    LiteralHashEntry::MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

// ----------------------------------------------------------------------
//
// For the mInts hashtable.
//

struct IntHashEntry : public PLDHashEntryHdr {
    nsIRDFInt *mInt;
    PRInt32    mKey;

    static const void * PR_CALLBACK
    GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
    {
        IntHashEntry *entry = NS_STATIC_CAST(IntHashEntry *, hdr);
        return &entry->mKey;
    }

    static PLDHashNumber PR_CALLBACK
    HashKey(PLDHashTable *table, const void *key)
    {
        return PLDHashNumber(*NS_STATIC_CAST(const PRInt32 *, key));
    }

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
               const void *key)
    {
        const IntHashEntry *entry =
            NS_STATIC_CAST(const IntHashEntry *, hdr);

        return *NS_STATIC_CAST(const PRInt32 *, key) == entry->mKey;
    }
};

static PLDHashTableOps gIntTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    IntHashEntry::GetKey,
    IntHashEntry::HashKey,
    IntHashEntry::MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

// ----------------------------------------------------------------------
//
// For the mDates hashtable.
//

struct DateHashEntry : public PLDHashEntryHdr {
    nsIRDFDate *mDate;
    PRTime      mKey;

    static const void * PR_CALLBACK
    GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
    {
        DateHashEntry *entry = NS_STATIC_CAST(DateHashEntry *, hdr);
        return &entry->mKey;
    }

    static PLDHashNumber PR_CALLBACK
    HashKey(PLDHashTable *table, const void *key)
    {
        // xor the low 32 bits with the high 32 bits.
        PRTime t = *NS_STATIC_CAST(const PRTime *, key);
        PRInt64 h64, l64;
        LL_USHR(h64, t, 32);
        l64 = LL_INIT(0, 0xffffffff);
        LL_AND(l64, l64, t);
        PRInt32 h32, l32;
        LL_L2I(h32, h64);
        LL_L2I(l32, l64);
        return PLDHashNumber(l32 ^ h32);
    }

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
               const void *key)
    {
        const DateHashEntry *entry =
            NS_STATIC_CAST(const DateHashEntry *, hdr);

        return LL_EQ(*NS_STATIC_CAST(const PRTime *, key), entry->mKey);
    }
};

static PLDHashTableOps gDateTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    DateHashEntry::GetKey,
    DateHashEntry::HashKey,
    DateHashEntry::MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

class BlobImpl : public nsIRDFBlob
{
public:
    struct Data {
        PRInt32  mLength;
        PRUint8 *mBytes;
    };

    BlobImpl(const PRUint8 *aBytes, PRInt32 aLength)
    {
        mData.mLength = aLength;
        mData.mBytes = new PRUint8[aLength];
        memcpy(mData.mBytes, aBytes, aLength);
        NS_ADDREF(gRDFService);
        gRDFService->RegisterBlob(this);
    }

    virtual ~BlobImpl()
    {
        gRDFService->UnregisterBlob(this);
        // Use NS_RELEASE2() here, because we want to decrease the
        // refcount, but not null out the gRDFService pointer (which is
        // what a vanilla NS_RELEASE() would do).
        nsrefcnt refcnt;
        NS_RELEASE2(gRDFService, refcnt);
        delete[] mData.mBytes;
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFNODE
    NS_DECL_NSIRDFBLOB

    Data mData;
};

NS_IMPL_ISUPPORTS2(BlobImpl, nsIRDFNode, nsIRDFBlob)

NS_IMETHODIMP
BlobImpl::EqualsNode(nsIRDFNode *aNode, PRBool *aEquals)
{
    nsCOMPtr<nsIRDFBlob> blob = do_QueryInterface(aNode);
    if (blob) {
        PRInt32 length;
        blob->GetLength(&length);

        if (length == mData.mLength) {
            const PRUint8 *bytes;
            blob->GetValue(&bytes);

            if (0 == memcmp(bytes, mData.mBytes, length)) {
                *aEquals = PR_TRUE;
                return NS_OK;
            }
        }
    }

    *aEquals = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
BlobImpl::GetValue(const PRUint8 **aResult)
{
    *aResult = mData.mBytes;
    return NS_OK;
}

NS_IMETHODIMP
BlobImpl::GetLength(PRInt32 *aResult)
{
    *aResult = mData.mLength;
    return NS_OK;
}

// ----------------------------------------------------------------------
//
// For the mBlobs hashtable.
//

struct BlobHashEntry : public PLDHashEntryHdr {
    BlobImpl *mBlob;

    static const void * PR_CALLBACK
    GetKey(PLDHashTable *table, PLDHashEntryHdr *hdr)
    {
        BlobHashEntry *entry = NS_STATIC_CAST(BlobHashEntry *, hdr);
        return &entry->mBlob->mData;
    }

    static PLDHashNumber PR_CALLBACK
    HashKey(PLDHashTable *table, const void *key)
    {
        const BlobImpl::Data *data =
            NS_STATIC_CAST(const BlobImpl::Data *, key);

        const PRUint8 *p = data->mBytes, *limit = p + data->mLength;
        PLDHashNumber h = 0;
        for ( ; p < limit; ++p)
            h = (h >> 28) ^ (h << 4) ^ *p;
        return h;
    }

    static PRBool PR_CALLBACK
    MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
               const void *key)
    {
        const BlobHashEntry *entry =
            NS_STATIC_CAST(const BlobHashEntry *, hdr);

        const BlobImpl::Data *left = &entry->mBlob->mData;

        const BlobImpl::Data *right =
            NS_STATIC_CAST(const BlobImpl::Data *, key);

        return (left->mLength == right->mLength)
            && 0 == memcmp(left->mBytes, right->mBytes, right->mLength);
    }
};

static PLDHashTableOps gBlobTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    BlobHashEntry::GetKey,
    BlobHashEntry::HashKey,
    BlobHashEntry::MatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull
};

////////////////////////////////////////////////////////////////////////
// LiteralImpl
//
//   Currently, all literals are implemented exactly the same way;
//   i.e., there is are no resource factories to allow you to generate
//   customer resources. I doubt that makes sense, anyway.
//
class LiteralImpl : public nsIRDFLiteral {
public:
    static nsresult
    Create(const PRUnichar* aValue, nsIRDFLiteral** aResult);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_DECL_NSIRDFNODE

    // nsIRDFLiteral
    NS_DECL_NSIRDFLITERAL

protected:
    LiteralImpl(const PRUnichar* s);
    virtual ~LiteralImpl();

    const PRUnichar* GetValue() const {
        size_t objectSize = ((sizeof(LiteralImpl) + sizeof(PRUnichar) - 1) / sizeof(PRUnichar)) * sizeof(PRUnichar);
        return NS_REINTERPRET_CAST(const PRUnichar*, NS_REINTERPRET_CAST(const unsigned char*, this) + objectSize);
    }
};


nsresult
LiteralImpl::Create(const PRUnichar* aValue, nsIRDFLiteral** aResult)
{
    // Goofy math to get alignment right. Copied from nsSharedString.h.
    size_t objectSize = ((sizeof(LiteralImpl) + sizeof(PRUnichar) - 1) / sizeof(PRUnichar)) * sizeof(PRUnichar);
    size_t stringLen = nsCharTraits<PRUnichar>::length(aValue);
    size_t stringSize = (stringLen + 1) * sizeof(PRUnichar);

    void* objectPtr = operator new(objectSize + stringSize);
    if (! objectPtr)
        return NS_ERROR_NULL_POINTER;

    PRUnichar* buf = NS_REINTERPRET_CAST(PRUnichar*, NS_STATIC_CAST(unsigned char*, objectPtr) + objectSize);
    nsCharTraits<PRUnichar>::copy(buf, aValue, stringLen + 1);

    NS_ADDREF(*aResult = new (objectPtr) LiteralImpl(buf));
    return NS_OK;
}


LiteralImpl::LiteralImpl(const PRUnichar* s)
{
    gRDFService->RegisterLiteral(this);
    NS_ADDREF(gRDFService);
}

LiteralImpl::~LiteralImpl()
{
    gRDFService->UnregisterLiteral(this);

    // Use NS_RELEASE2() here, because we want to decrease the
    // refcount, but not null out the gRDFService pointer (which is
    // what a vanilla NS_RELEASE() would do).
    nsrefcnt refcnt;
    NS_RELEASE2(gRDFService, refcnt);
}

NS_IMPL_THREADSAFE_ADDREF(LiteralImpl)
NS_IMPL_THREADSAFE_RELEASE(LiteralImpl)

nsresult
LiteralImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFLiteralIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFLiteral*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
LiteralImpl::EqualsNode(nsIRDFNode* aNode, PRBool* aResult)
{
    nsresult rv;
    nsIRDFLiteral* literal;
    rv = aNode->QueryInterface(kIRDFLiteralIID, (void**) &literal);
    if (NS_SUCCEEDED(rv)) {
        *aResult = (NS_STATIC_CAST(nsIRDFLiteral*, this) == literal);
        NS_RELEASE(literal);
        return NS_OK;
    }
    else if (rv == NS_NOINTERFACE) {
        *aResult = PR_FALSE;
        return NS_OK;
    }
    else {
        return rv;
    }
}

NS_IMETHODIMP
LiteralImpl::GetValue(PRUnichar* *value)
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    const PRUnichar *temp = GetValue();
    *value = temp? nsCRT::strdup(temp) : 0;
    return NS_OK;
}


NS_IMETHODIMP
LiteralImpl::GetValueConst(const PRUnichar** aValue)
{
    *aValue = GetValue();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// DateImpl
//

class DateImpl : public nsIRDFDate {
public:
    DateImpl(const PRTime s);
    virtual ~DateImpl();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_DECL_NSIRDFNODE

    // nsIRDFDate
    NS_IMETHOD GetValue(PRTime *value);

private:
    nsresult EqualsDate(nsIRDFDate* date, PRBool* result);
    PRTime mValue;
};


DateImpl::DateImpl(const PRTime s)
    : mValue(s)
{
    gRDFService->RegisterDate(this);
    NS_ADDREF(gRDFService);
}

DateImpl::~DateImpl()
{
    gRDFService->UnregisterDate(this);

    // Use NS_RELEASE2() here, because we want to decrease the
    // refcount, but not null out the gRDFService pointer (which is
    // what a vanilla NS_RELEASE() would do).
    nsrefcnt refcnt;
    NS_RELEASE2(gRDFService, refcnt);
}

NS_IMPL_ADDREF(DateImpl)
NS_IMPL_RELEASE(DateImpl)

nsresult
DateImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFDateIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFDate*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
DateImpl::EqualsNode(nsIRDFNode* node, PRBool* result)
{
    nsresult rv;
    nsIRDFDate* date;
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFDateIID, (void**) &date))) {
        rv = EqualsDate(date, result);
        NS_RELEASE(date);
    }
    else {
        *result = PR_FALSE;
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
DateImpl::GetValue(PRTime *value)
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue;
    return NS_OK;
}


nsresult
DateImpl::EqualsDate(nsIRDFDate* date, PRBool* result)
{
    NS_ASSERTION(date && result, "null ptr");
    if (!date || !result)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    PRTime p;
    if (NS_FAILED(rv = date->GetValue(&p)))
        return rv;

    *result = LL_EQ(p, mValue);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// IntImpl
//

class IntImpl : public nsIRDFInt {
public:
    IntImpl(PRInt32 s);
    virtual ~IntImpl();

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_DECL_NSIRDFNODE

    // nsIRDFInt
    NS_IMETHOD GetValue(PRInt32 *value);

private:
    nsresult EqualsInt(nsIRDFInt* value, PRBool* result);
    PRInt32 mValue;
};


IntImpl::IntImpl(PRInt32 s)
    : mValue(s)
{
    gRDFService->RegisterInt(this);
    NS_ADDREF(gRDFService);
}

IntImpl::~IntImpl()
{
    gRDFService->UnregisterInt(this);

    // Use NS_RELEASE2() here, because we want to decrease the
    // refcount, but not null out the gRDFService pointer (which is
    // what a vanilla NS_RELEASE() would do).
    nsrefcnt refcnt;
    NS_RELEASE2(gRDFService, refcnt);
}

NS_IMPL_ADDREF(IntImpl)
NS_IMPL_RELEASE(IntImpl)

nsresult
IntImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFIntIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFInt*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
IntImpl::EqualsNode(nsIRDFNode* node, PRBool* result)
{
    nsresult rv;
    nsIRDFInt* intValue;
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFIntIID, (void**) &intValue))) {
        rv = EqualsInt(intValue, result);
        NS_RELEASE(intValue);
    }
    else {
        *result = PR_FALSE;
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
IntImpl::GetValue(PRInt32 *value)
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue;
    return NS_OK;
}


nsresult
IntImpl::EqualsInt(nsIRDFInt* intValue, PRBool* result)
{
    NS_ASSERTION(intValue && result, "null ptr");
    if (!intValue || !result)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    PRInt32 p;
    if (NS_FAILED(rv = intValue->GetValue(&p)))
        return rv;

    *result = (p == mValue);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// RDFServiceImpl

RDFServiceImpl::RDFServiceImpl()
    :  mNamedDataSources(nsnull)
{
    mResources.ops = nsnull;
    mLiterals.ops = nsnull;
    mInts.ops = nsnull;
    mDates.ops = nsnull;
    mBlobs.ops = nsnull;
}

nsresult
RDFServiceImpl::Init()
{
    nsresult rv;

    mNamedDataSources = PL_NewHashTable(23,
                                        PL_HashString,
                                        PL_CompareStrings,
                                        PL_CompareValues,
                                        &dataSourceHashAllocOps, nsnull);

    if (! mNamedDataSources)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!PL_DHashTableInit(&mResources, &gResourceTableOps, nsnull,
                           sizeof(ResourceHashEntry), PL_DHASH_MIN_SIZE)) {
        mResources.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&mLiterals, &gLiteralTableOps, nsnull,
                           sizeof(LiteralHashEntry), PL_DHASH_MIN_SIZE)) {
        mLiterals.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&mInts, &gIntTableOps, nsnull,
                           sizeof(IntHashEntry), PL_DHASH_MIN_SIZE)) {
        mInts.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&mDates, &gDateTableOps, nsnull,
                           sizeof(DateHashEntry), PL_DHASH_MIN_SIZE)) {
        mDates.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!PL_DHashTableInit(&mBlobs, &gBlobTableOps, nsnull,
                           sizeof(BlobHashEntry), PL_DHASH_MIN_SIZE)) {
        mBlobs.ops = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mDefaultResourceFactory = do_GetClassObject(kRDFDefaultResourceCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get default resource factory");
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFService");
#endif

    return NS_OK;
}


RDFServiceImpl::~RDFServiceImpl()
{
    if (mNamedDataSources) {
        PL_HashTableDestroy(mNamedDataSources);
        mNamedDataSources = nsnull;
    }
    if (mResources.ops)
        PL_DHashTableFinish(&mResources);
    if (mLiterals.ops)
        PL_DHashTableFinish(&mLiterals);
    if (mInts.ops)
        PL_DHashTableFinish(&mInts);
    if (mDates.ops)
        PL_DHashTableFinish(&mDates);
    if (mBlobs.ops)
        PL_DHashTableFinish(&mBlobs);
    gRDFService = nsnull;
}


nsresult
RDFServiceImpl::GetRDFService(nsIRDFService** mgr)
{
    if (! gRDFService) {
        RDFServiceImpl* serv = new RDFServiceImpl();
        if (! serv)
            return NS_ERROR_OUT_OF_MEMORY;

        nsresult rv;
        rv = serv->Init();
        if (NS_FAILED(rv)) {
            delete serv;
            return rv;
        }

        gRDFService = serv;
    }

    NS_ADDREF(gRDFService);
    *mgr = gRDFService;
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(RDFServiceImpl, nsIRDFService, nsISupportsWeakReference)

// Per RFC2396.
static const PRUint8
kLegalSchemeChars[] = {
          //        ASCII    Bits     Ordered  Hex
          //                 01234567 76543210
    0x00, // 00-07
    0x00, // 08-0F
    0x00, // 10-17
    0x00, // 18-1F
    0x00, // 20-27   !"#$%&' 00000000 00000000
    0x28, // 28-2F  ()*+,-./ 00010100 00101000 0x28
    0xff, // 30-37  01234567 11111111 11111111 0xFF
    0x03, // 38-3F  89:;<=>? 11000000 00000011 0x03
    0xfe, // 40-47  @ABCDEFG 01111111 11111110 0xFE
    0xff, // 48-4F  HIJKLMNO 11111111 11111111 0xFF
    0xff, // 50-57  PQRSTUVW 11111111 11111111 0xFF
    0x87, // 58-5F  XYZ[\]^_ 11100001 10000111 0x87
    0xfe, // 60-67  `abcdefg 01111111 11111110 0xFE
    0xff, // 68-6F  hijklmno 11111111 11111111 0xFF
    0xff, // 70-77  pqrstuvw 11111111 11111111 0xFF
    0x07, // 78-7F  xyz{|}~  11100000 00000111 0x07
    0x00, 0x00, 0x00, 0x00, // >= 80
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static inline PRBool
IsLegalSchemeCharacter(const char aChar)
{
    PRUint8 mask = kLegalSchemeChars[aChar >> 3];
    PRUint8 bit = PR_BIT(aChar & 0x7);
    return PRBool((mask & bit) != 0);
}


NS_IMETHODIMP
RDFServiceImpl::GetResource(const nsACString& aURI, nsIRDFResource** aResource)
{
    // Sanity checks
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    NS_PRECONDITION(!aURI.IsEmpty(), "URI is empty");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;
    if (aURI.IsEmpty())
        return NS_ERROR_INVALID_ARG;

    const nsAFlatCString& flatURI = PromiseFlatCString(aURI);
    PR_LOG(gLog, PR_LOG_DEBUG, ("rdfserv get-resource %s", flatURI.get()));

    // First, check the cache to see if we've already created and
    // registered this thing.
    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mResources, flatURI.get(), PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        ResourceHashEntry *entry = NS_STATIC_CAST(ResourceHashEntry *, hdr);
        NS_ADDREF(*aResource = entry->mResource);
        return NS_OK;
    }

    // Nope. So go to the repository to create it.

    // Compute the scheme of the URI. Scan forward until we either:
    //
    // 1. Reach the end of the string
    // 2. Encounter a non-alpha character
    // 3. Encouter a colon.
    //
    // If we encounter a colon _before_ encountering a non-alpha
    // character, then assume it's the scheme.
    //
    // XXX Although it's really not correct, we'll allow underscore
    // characters ('_'), too.
    nsACString::const_iterator p, end;
    aURI.BeginReading(p);
    aURI.EndReading(end);
    while (p != end && IsLegalSchemeCharacter(*p))
        ++p;

    nsresult rv;
    nsCOMPtr<nsIFactory> factory;

    nsACString::const_iterator begin;
    aURI.BeginReading(begin);
    if (*p == ':') {
        // There _was_ a scheme. First see if it's the same scheme
        // that we just tried to use...
        if (mLastFactory && mLastURIPrefix.Equals(Substring(begin, p)))
            factory = mLastFactory;
        else {
            // Try to find a factory using the component manager.
            nsACString::const_iterator begin;
            aURI.BeginReading(begin);
            nsCAutoString contractID;
            contractID = NS_LITERAL_CSTRING(NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX) +
                         Substring(begin, p);

            factory = do_GetClassObject(contractID.get());
            if (factory) {
                // Store the factory in our one-element cache.
                if (p != begin) {
                    mLastFactory = factory;
                    mLastURIPrefix = Substring(begin, p);
                }
            }
        }
    }

    if (! factory) {
        // fall through to using the "default" resource factory if either:
        //
        // 1. The URI didn't have a scheme, or
        // 2. There was no resource factory registered for the scheme.
        factory = mDefaultResourceFactory;

        // Store the factory in our one-element cache.
        if (p != begin) {
            mLastFactory = factory;
            mLastURIPrefix = Substring(begin, p);
        }
    }

    nsIRDFResource *result;
    rv = factory->CreateInstance(nsnull, NS_GET_IID(nsIRDFResource), (void**) &result);
    if (NS_FAILED(rv)) return rv;

    // Now initialize it with it's URI. At this point, the resource
    // implementation should register itself with the RDF service.
    rv = result->Init(flatURI.get());
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to initialize resource");
        NS_RELEASE(result);
        return rv;
    }

    *aResource = result; // already refcounted from repository
    return rv;
}

NS_IMETHODIMP
RDFServiceImpl::GetUnicodeResource(const nsAString& aURI, nsIRDFResource** aResource)
{
    return GetResource(NS_ConvertUCS2toUTF8(aURI), aResource);
}


NS_IMETHODIMP
RDFServiceImpl::GetAnonymousResource(nsIRDFResource** aResult)
{
static PRUint32 gCounter = 0;
static char gChars[] = "0123456789abcdef"
                       "ghijklmnopqrstuv"
                       "wxyzABCDEFGHIJKL"
                       "MNOPQRSTUVWXYZ.+";

static PRInt32 kMask  = 0x003f;
static PRInt32 kShift = 6;

    if (! gCounter) {
        // Start it at a semi-unique value, just to minimize the
        // chance that we get into a situation where
        //
        // 1. An anonymous resource gets serialized out in a graph
        // 2. Reboot
        // 3. The same anonymous resource gets requested, and refers
        //    to something completely different.
        // 4. The serialization is read back in.
        LL_L2UI(gCounter, PR_Now());
    }

    nsresult rv;
    nsCAutoString s;

    do {
        // Ugh, this is a really sloppy way to do this; I copied the
        // implementation from the days when it lived outside the RDF
        // service. Now that it's a member we can be more cleverer.

        s.Truncate();
        s.Append("rdf:#$");

        PRUint32 id = ++gCounter;
        while (id) {
            char ch = gChars[(id & kMask)];
            s.Append(ch);
            id >>= kShift;
        }

        nsIRDFResource* resource;
        rv = GetResource(s, &resource);
        if (NS_FAILED(rv)) return rv;

        // XXX an ugly but effective way to make sure that this
        // resource is really unique in the world.
        resource->AddRef();
        nsrefcnt refcnt = resource->Release();

        if (refcnt == 1) {
            *aResult = resource;
            break;
        }

        NS_RELEASE(resource);
    } while (1);

    return NS_OK;
}


NS_IMETHODIMP
RDFServiceImpl::GetLiteral(const PRUnichar* aValue, nsIRDFLiteral** aLiteral)
{
    NS_PRECONDITION(aValue != nsnull, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aLiteral != nsnull, "null ptr");
    if (! aLiteral)
        return NS_ERROR_NULL_POINTER;

    // See if we have one already cached
    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mLiterals, aValue, PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        LiteralHashEntry *entry = NS_STATIC_CAST(LiteralHashEntry *, hdr);
        NS_ADDREF(*aLiteral = entry->mLiteral);
        return NS_OK;
    }

    // Nope. Create a new one
    return LiteralImpl::Create(aValue, aLiteral);
}

NS_IMETHODIMP
RDFServiceImpl::GetDateLiteral(PRTime aTime, nsIRDFDate** aResult)
{
    // See if we have one already cached
    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mDates, &aTime, PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        DateHashEntry *entry = NS_STATIC_CAST(DateHashEntry *, hdr);
        NS_ADDREF(*aResult = entry->mDate);
        return NS_OK;
    }

    DateImpl* result = new DateImpl(aTime);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = result);
    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::GetIntLiteral(PRInt32 aInt, nsIRDFInt** aResult)
{
    // See if we have one already cached
    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mInts, &aInt, PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        IntHashEntry *entry = NS_STATIC_CAST(IntHashEntry *, hdr);
        NS_ADDREF(*aResult = entry->mInt);
        return NS_OK;
    }

    IntImpl* result = new IntImpl(aInt);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = result);
    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::GetBlobLiteral(const PRUint8 *aBytes, PRInt32 aLength,
                               nsIRDFBlob **aResult)
{
    BlobImpl::Data key = { aLength, NS_CONST_CAST(PRUint8 *, aBytes) };

    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mBlobs, &key, PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        BlobHashEntry *entry = NS_STATIC_CAST(BlobHashEntry *, hdr);
        NS_ADDREF(*aResult = entry->mBlob);
        return NS_OK;
    }

    BlobImpl *result = new BlobImpl(aBytes, aLength);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = result);
    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::IsAnonymousResource(nsIRDFResource* aResource, PRBool* _result)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    rv = aResource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    if ((uri[0] == 'r') &&
        (uri[1] == 'd') &&
        (uri[2] == 'f') &&
        (uri[3] == ':') &&
        (uri[4] == '#') &&
        (uri[5] == '$')) {
        *_result = PR_TRUE;
    }
    else {
        *_result = PR_FALSE;
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::RegisterResource(nsIRDFResource* aResource, PRBool aReplace)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    rv = aResource->GetValueConst(&uri);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get URI from resource");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(uri != nsnull, "resource has no URI");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mResources, uri, PL_DHASH_LOOKUP);

    if (PL_DHASH_ENTRY_IS_BUSY(hdr)) {
        if (!aReplace) {
            NS_WARNING("resource already registered, and replace not specified");
            return NS_ERROR_FAILURE;    // already registered
        }

        // N.B., we do _not_ release the original resource because we
        // only ever held a weak reference to it. We simply replace
        // it.

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   replace-resource [%p] <-- [%p] %s",
                NS_STATIC_CAST(ResourceHashEntry *, hdr)->mResource,
                aResource, (const char*) uri));
    }
    else {
        hdr = PL_DHashTableOperate(&mResources, uri, PL_DHASH_ADD);
        if (! hdr)
            return NS_ERROR_OUT_OF_MEMORY;

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   register-resource [%p] %s",
                aResource, (const char*) uri));
    }

    // N.B., we only hold a weak reference to the resource: that way,
    // the resource can be destroyed when the last refcount goes
    // away. The single addref that the CreateResource() call made
    // will be owned by the callee.
    ResourceHashEntry *entry = NS_STATIC_CAST(ResourceHashEntry *, hdr);
    entry->mResource = aResource;
    entry->mKey = uri;

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::UnregisterResource(nsIRDFResource* aResource)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    rv = aResource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(uri != nsnull, "resource has no URI");
    if (! uri)
        return NS_ERROR_UNEXPECTED;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-resource [%p] %s",
            aResource, (const char*) uri));

#ifdef DEBUG
    if (PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mResources, uri, PL_DHASH_LOOKUP)))
        NS_WARNING("resource was never registered");
#endif

    PL_DHashTableOperate(&mResources, uri, PL_DHASH_REMOVE);
    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::RegisterDataSource(nsIRDFDataSource* aDataSource, PRBool aReplace)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsXPIDLCString uri;
    rv = aDataSource->GetURI(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;

    PLHashEntry** hep =
        PL_HashTableRawLookup(mNamedDataSources, (*mNamedDataSources->keyHash)(uri), uri);

    if (*hep) {
        if (! aReplace)
            return NS_ERROR_FAILURE; // already registered

        // N.B., we only hold a weak reference to the datasource, so
        // just replace the old with the new and don't touch any
        // refcounts.
        PR_LOG(gLog, PR_LOG_NOTICE,
               ("rdfserv    replace-datasource [%p] <-- [%p] %s",
                (*hep)->value, aDataSource, (const char*) uri));

        (*hep)->value = aDataSource;
    }
    else {
        const char* key = PL_strdup(uri);
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mNamedDataSources, key, aDataSource);

        PR_LOG(gLog, PR_LOG_NOTICE,
               ("rdfserv   register-datasource [%p] %s",
                aDataSource, (const char*) uri));

        // N.B., we only hold a weak reference to the datasource, so don't
        // addref.
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::UnregisterDataSource(nsIRDFDataSource* aDataSource)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsXPIDLCString uri;
    rv = aDataSource->GetURI(getter_Copies(uri));
    if (NS_FAILED(rv)) return rv;

    //NS_ASSERTION(uri != nsnull, "datasource has no URI");
    if (! uri)
        return NS_ERROR_UNEXPECTED;

    PLHashEntry** hep =
        PL_HashTableRawLookup(mNamedDataSources, (*mNamedDataSources->keyHash)(uri), uri);

    // It may well be that this datasource was never registered. If
    // so, don't unregister it.
    if (! *hep || ((*hep)->value != aDataSource))
        return NS_OK;

    // N.B., we only held a weak reference to the datasource, so we
    // don't release here.
    PL_HashTableRawRemove(mNamedDataSources, hep, *hep);

    PR_LOG(gLog, PR_LOG_NOTICE,
           ("rdfserv unregister-datasource [%p] %s",
            aDataSource, (const char*) uri));

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::GetDataSource(const char* aURI, nsIRDFDataSource** aDataSource)
{
    // Use the other GetDataSource and ask for a non-blocking Refresh.
    // If you wanted it loaded synchronously, then you should've tried to do it
    // yourself, or used GetDataSourceBlocking.
    return GetDataSource( aURI, PR_FALSE, aDataSource );
}

NS_IMETHODIMP
RDFServiceImpl::GetDataSourceBlocking(const char* aURI, nsIRDFDataSource** aDataSource)
{
    // Use GetDataSource and ask for a blocking Refresh.
    return GetDataSource( aURI, PR_TRUE, aDataSource );
}

nsresult
RDFServiceImpl::GetDataSource(const char* aURI, PRBool aBlock, nsIRDFDataSource** aDataSource)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Attempt to canonify the URI before we look for it in the
    // cache. We won't bother doing this on `rdf:' URIs to avoid
    // useless (and expensive) protocol handler lookups.
    nsCAutoString spec(aURI);

    if (!StringBeginsWith(spec, NS_LITERAL_CSTRING("rdf:"))) {
        nsCOMPtr<nsIURI> uri;
        NS_NewURI(getter_AddRefs(uri), spec);
        if (uri)
            uri->GetSpec(spec);
    }

    // First, check the cache to see if we already have this
    // datasource loaded and initialized.
    {
        nsIRDFDataSource* cached =
            NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, spec.get()));

        if (cached) {
            NS_ADDREF(cached);
            *aDataSource = cached;
            return NS_OK;
        }
    }

    // Nope. So go to the repository to try to create it.
    nsCOMPtr<nsIRDFDataSource> ds;
    if (StringBeginsWith(spec, NS_LITERAL_CSTRING("rdf:"))) {
        // It's a built-in data source. Convert it to a contract ID.
        nsCAutoString contractID(
                NS_LITERAL_CSTRING(NS_RDF_DATASOURCE_CONTRACTID_PREFIX) +
                Substring(spec, 4, spec.Length() - 4));

        // Strip params to get ``base'' contractID for data source.
        PRInt32 p = contractID.FindChar(PRUnichar('&'));
        if (p >= 0)
            contractID.Truncate(p);

        ds = do_GetService(contractID.get(), &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds);
        if (remote) {
            rv = remote->Init(spec.get());
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        // Try to load this as an RDF/XML data source
        ds = do_CreateInstance(kRDFXMLDataSourceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(ds));
        NS_ASSERTION(remote, "not a remote RDF/XML data source!");
        if (! remote) return NS_ERROR_UNEXPECTED;

        rv = remote->Init(spec.get());
        if (NS_FAILED(rv)) return rv;

        rv = remote->Refresh(aBlock);
        if (NS_FAILED(rv)) return rv;
    }

    *aDataSource = ds;
    NS_ADDREF(*aDataSource);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
RDFServiceImpl::RegisterLiteral(nsIRDFLiteral* aLiteral)
{
    const PRUnichar* value;
    aLiteral->GetValueConst(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mLiterals,
                                                             value,
                                                             PL_DHASH_LOOKUP)),
                 "literal already registered");

    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mLiterals, value, PL_DHASH_ADD);

    if (! hdr)
        return NS_ERROR_OUT_OF_MEMORY;

    LiteralHashEntry *entry = NS_STATIC_CAST(LiteralHashEntry *, hdr);

    // N.B., we only hold a weak reference to the literal: that
    // way, the literal can be destroyed when the last refcount
    // goes away. The single addref that the CreateLiteral() call
    // made will be owned by the callee.
    entry->mLiteral = aLiteral;
    entry->mKey = value;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv   register-literal [%p] %s",
            aLiteral, (const PRUnichar*) value));

    return NS_OK;
}


nsresult
RDFServiceImpl::UnregisterLiteral(nsIRDFLiteral* aLiteral)
{
    const PRUnichar* value;
    aLiteral->GetValueConst(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(PL_DHashTableOperate(&mLiterals,
                                                             value,
                                                             PL_DHASH_LOOKUP)),
                 "literal was never registered");

    PL_DHashTableOperate(&mLiterals, value, PL_DHASH_REMOVE);

    // N.B. that we _don't_ release the literal: we only held a weak
    // reference to it in the hashtable.
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-literal [%p] %s",
            aLiteral, (const PRUnichar*) value));

    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
RDFServiceImpl::RegisterInt(nsIRDFInt* aInt)
{
    PRInt32 value;
    aInt->GetValue(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mInts,
                                                             &value,
                                                             PL_DHASH_LOOKUP)),
                 "int already registered");

    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mInts, &value, PL_DHASH_ADD);

    if (! hdr)
        return NS_ERROR_OUT_OF_MEMORY;

    IntHashEntry *entry = NS_STATIC_CAST(IntHashEntry *, hdr);

    // N.B., we only hold a weak reference to the literal: that
    // way, the literal can be destroyed when the last refcount
    // goes away. The single addref that the CreateInt() call
    // made will be owned by the callee.
    entry->mInt = aInt;
    entry->mKey = value;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv   register-int [%p] %d",
            aInt, value));

    return NS_OK;
}


nsresult
RDFServiceImpl::UnregisterInt(nsIRDFInt* aInt)
{
    PRInt32 value;
    aInt->GetValue(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(PL_DHashTableOperate(&mInts,
                                                             &value,
                                                             PL_DHASH_LOOKUP)),
                 "int was never registered");

    PL_DHashTableOperate(&mInts, &value, PL_DHASH_REMOVE);

    // N.B. that we _don't_ release the literal: we only held a weak
    // reference to it in the hashtable.
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-int [%p] %d",
            aInt, value));

    return NS_OK;
}

//----------------------------------------------------------------------

nsresult
RDFServiceImpl::RegisterDate(nsIRDFDate* aDate)
{
    PRTime value;
    aDate->GetValue(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mDates,
                                                             &value,
                                                             PL_DHASH_LOOKUP)),
                 "date already registered");

    PLDHashEntryHdr *hdr =
        PL_DHashTableOperate(&mDates, &value, PL_DHASH_ADD);

    if (! hdr)
        return NS_ERROR_OUT_OF_MEMORY;

    DateHashEntry *entry = NS_STATIC_CAST(DateHashEntry *, hdr);

    // N.B., we only hold a weak reference to the literal: that
    // way, the literal can be destroyed when the last refcount
    // goes away. The single addref that the CreateDate() call
    // made will be owned by the callee.
    entry->mDate = aDate;
    entry->mKey = value;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv   register-date [%p] %ld",
            aDate, value));

    return NS_OK;
}


nsresult
RDFServiceImpl::UnregisterDate(nsIRDFDate* aDate)
{
    PRTime value;
    aDate->GetValue(&value);

    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(PL_DHashTableOperate(&mDates,
                                                             &value,
                                                             PL_DHASH_LOOKUP)),
                 "date was never registered");

    PL_DHashTableOperate(&mDates, &value, PL_DHASH_REMOVE);

    // N.B. that we _don't_ release the literal: we only held a weak
    // reference to it in the hashtable.
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-date [%p] %ld",
            aDate, value));

    return NS_OK;
}

nsresult
RDFServiceImpl::RegisterBlob(BlobImpl *aBlob)
{
    NS_ASSERTION(PL_DHASH_ENTRY_IS_FREE(PL_DHashTableOperate(&mBlobs,
                                                             &aBlob->mData,
                                                             PL_DHASH_LOOKUP)),
                 "blob already registered");

    PLDHashEntryHdr *hdr = 
        PL_DHashTableOperate(&mBlobs, &aBlob->mData, PL_DHASH_ADD);

    if (! hdr)
        return NS_ERROR_OUT_OF_MEMORY;

    BlobHashEntry *entry = NS_STATIC_CAST(BlobHashEntry *, hdr);

    // N.B., we only hold a weak reference to the literal: that
    // way, the literal can be destroyed when the last refcount
    // goes away. The single addref that the CreateInt() call
    // made will be owned by the callee.
    entry->mBlob = aBlob;

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv   register-blob [%p] %s",
            aBlob, aBlob->mData.mBytes));

    return NS_OK;
}

nsresult
RDFServiceImpl::UnregisterBlob(BlobImpl *aBlob)
{
    NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(PL_DHashTableOperate(&mBlobs,
                                                             &aBlob->mData,
                                                             PL_DHASH_LOOKUP)),
                 "blob was never registered");

    PL_DHashTableOperate(&mBlobs, &aBlob->mData, PL_DHASH_REMOVE);
 
     // N.B. that we _don't_ release the literal: we only held a weak
     // reference to it in the hashtable.
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-blob [%p] %s",
            aBlob, aBlob->mData.mBytes));

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFService(nsIRDFService** mgr)
{
    return RDFServiceImpl::GetRDFService(mgr);
}

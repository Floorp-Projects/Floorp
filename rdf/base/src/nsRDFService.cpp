/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"
#include "rdf.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFXMLDataSourceCID,    NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFDefaultResourceCID,  NS_RDFDEFAULTRESOURCE_CID);

static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFDateIID,         NS_IRDFDATE_IID);
static NS_DEFINE_IID(kIRDFIntIID,         NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog = nsnull;
#endif

// Defining these will re-use the string pointer that is owned by the
// resource (or literal) as the key for the hashtable. It avoids two
// strdup()'s for each new resource that is created.
#define REUSE_RESOURCE_URI_AS_KEY
#define REUSE_LITERAL_VALUE_AS_KEY

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
    PLHashTable* mResources;
    PLHashTable* mLiterals;

    char mLastURIPrefix[16];
    PRInt32 mLastPrefixlen;
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
    nsresult RegisterLiteral(nsIRDFLiteral* aLiteral, PRBool aReplace = PR_FALSE);
    nsresult UnregisterLiteral(nsIRDFLiteral* aLiteral);
};

static RDFServiceImpl* gRDFService; // The one-and-only RDF service


// These functions are copied from nsprpub/lib/ds/plhash.c, with one
// change to free the key in DataSourceFreeEntry.

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
    NS_INIT_REFCNT();
    gRDFService->RegisterLiteral(this);
    NS_ADDREF(gRDFService);
}

LiteralImpl::~LiteralImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: LiteralImpl\n", gInstanceCount);
#endif

    gRDFService->UnregisterLiteral(this);

    // Use NS_RELEASE2() here, because we want to decrease the
    // refcount, but not null out the gRDFService pointer (which is
    // what a vanilla NS_RELEASE() would do).
    nsrefcnt refcnt;
    NS_RELEASE2(gRDFService, refcnt);
}

NS_IMPL_THREADSAFE_ADDREF(LiteralImpl);
NS_IMPL_THREADSAFE_RELEASE(LiteralImpl);

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
    NS_INIT_REFCNT();
}

DateImpl::~DateImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: DateImpl\n", gInstanceCount);
#endif
}

NS_IMPL_ADDREF(DateImpl);
NS_IMPL_RELEASE(DateImpl);

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
    NS_INIT_REFCNT();
}

IntImpl::~IntImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: IntImpl\n", gInstanceCount);
#endif
}

NS_IMPL_ADDREF(IntImpl);
NS_IMPL_RELEASE(IntImpl);

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

static PLHashNumber PR_CALLBACK
rdf_HashWideString(const void* key)
{
    PLHashNumber result = 0;
    for (PRUnichar* s = (PRUnichar*) key; *s != nsnull; ++s)
        result = (result >> 28) ^ (result << 4) ^ *s;
    return result;
}

static PRIntn PR_CALLBACK
rdf_CompareWideStrings(const void* v1, const void* v2)
{
    return 0 == nsCRT::strcmp(NS_STATIC_CAST(const PRUnichar*, v1), NS_STATIC_CAST(const PRUnichar*, v2));
}


RDFServiceImpl::RDFServiceImpl()
    :  mNamedDataSources(nsnull), mResources(nsnull), mLiterals(nsnull), mLastPrefixlen(0)
{
    NS_INIT_REFCNT();
}



nsresult
RDFServiceImpl::Init()
{
    nsresult rv;

    mResources = PL_NewHashTable(1023,              // nbuckets
                                 PL_HashString,     // hash fn
                                 PL_CompareStrings, // key compare fn
                                 PL_CompareValues,  // value compare fn
                                 nsnull, nsnull);   // alloc ops & priv

    if (! mResources) return NS_ERROR_OUT_OF_MEMORY;

    mLiterals = PL_NewHashTable(1023,
                                rdf_HashWideString,
                                rdf_CompareWideStrings,
                                PL_CompareValues,
                                nsnull, nsnull);

    if (! mLiterals) return NS_ERROR_OUT_OF_MEMORY;

    mNamedDataSources = PL_NewHashTable(23,
                                        PL_HashString,
                                        PL_CompareStrings,
                                        PL_CompareValues,
                                        &dataSourceHashAllocOps, nsnull);

    if (! mNamedDataSources) return NS_ERROR_OUT_OF_MEMORY;

    rv = nsComponentManager::FindFactory(kRDFDefaultResourceCID, getter_AddRefs(mDefaultResourceFactory));
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
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: RDFServiceImpl\n", gInstanceCount);
#endif

    if (mNamedDataSources) {
        PL_HashTableDestroy(mNamedDataSources);
        mNamedDataSources = nsnull;
    }
    if (mResources) {
        PL_HashTableDestroy(mResources);
        mResources = nsnull;
    }
    if (mLiterals) {
        PL_HashTableDestroy(mLiterals);
        mLiterals = nsnull;
    }
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
RDFServiceImpl::GetResource(const char* aURI, nsIRDFResource** aResource)
{
    // Sanity checks
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    PR_LOG(gLog, PR_LOG_DEBUG, ("rdfserv get-resource %s", aURI));

    // First, check the cache to see if we've already created and
    // registered this thing.
    nsIRDFResource* result =
        NS_STATIC_CAST(nsIRDFResource*, PL_HashTableLookup(mResources, aURI));

    if (result) {
        // Addref for the callee.
        NS_ADDREF(result);
        *aResource = result;
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
    const char* p = aURI;
    while (IsLegalSchemeCharacter(*p))
        ++p;

    nsresult rv;
    nsCOMPtr<nsIFactory> factory;
    PRUint32 prefixlen = 0;

    if (*p == ':') {
        // There _was_ a scheme. First see if it's the same scheme
        // that we just tried to use...
        prefixlen = (p - aURI);

        if ((mLastFactory) && ((PRInt32)prefixlen == mLastPrefixlen) &&
            (aURI[0] == mLastURIPrefix[0]) &&
            (0 == PL_strncmp(aURI, mLastURIPrefix, prefixlen))) {
            factory = mLastFactory;
        }
        else {
            // Try to find a factory using the component manager.
            static const char kRDFResourceFactoryContractIDPrefix[]
                = NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX;

            PRInt32 pos = p - aURI;
            PRInt32 len = pos + sizeof(kRDFResourceFactoryContractIDPrefix) - 1;

            // Safely convert to a C-string for the XPCOM APIs
            char buf[128];
            char* contractID = buf;
            if (len >= PRInt32(sizeof buf))
                contractID = (char *)nsMemory::Alloc(len + 1);

            if (contractID == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;

            PL_strcpy(contractID, kRDFResourceFactoryContractIDPrefix);
            PL_strncpy(contractID + sizeof(kRDFResourceFactoryContractIDPrefix) - 1, aURI, pos);
            contractID[len] = '\0';

            nsCID cid;
            rv = nsComponentManager::ContractIDToClassID(contractID, &cid);

            if (contractID != buf)
                nsCRT::free(contractID);

            if (NS_SUCCEEDED(rv)) {
                rv = nsComponentManager::FindFactory(cid, getter_AddRefs(factory));
                NS_ASSERTION(NS_SUCCEEDED(rv), "factory registered, but couldn't load");
                if (NS_FAILED(rv)) return rv;

                // Store the factory in our one-element cache.
                if ((prefixlen > 0) && (prefixlen < sizeof(mLastURIPrefix))) {
                    mLastFactory = factory;
                    PL_strncpyz(mLastURIPrefix, aURI, prefixlen + 1);
                    mLastPrefixlen = prefixlen;
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
        if ((prefixlen > 0) && (prefixlen < sizeof(mLastURIPrefix))) {
            mLastFactory = factory;
            PL_strncpyz(mLastURIPrefix, aURI, prefixlen + 1);
            mLastPrefixlen = prefixlen;
        }
    }

    rv = factory->CreateInstance(nsnull, NS_GET_IID(nsIRDFResource), (void**) &result);
    if (NS_FAILED(rv)) return rv;

    // Now initialize it with it's URI. At this point, the resource
    // implementation should register itself with the RDF service.
    rv = result->Init(aURI);
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to initialize resource");
        NS_RELEASE(result);
        return rv;
    }

    *aResource = result; // already refcounted from repository
    return rv;
}

NS_IMETHODIMP
RDFServiceImpl::GetUnicodeResource(const PRUnichar* aURI, nsIRDFResource** aResource)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    return GetResource(NS_ConvertUCS2toUTF8(aURI).get(), aResource);
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
        rv = GetResource(s.get(), &resource);
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

    // See if we have on already cached
    nsIRDFLiteral* literal =
        NS_STATIC_CAST(nsIRDFLiteral*, PL_HashTableLookup(mLiterals, aValue));

    if (literal) {
        NS_ADDREF(literal);
        *aLiteral = literal;
        return NS_OK;
    }

    // Nope. Create a new one
    return LiteralImpl::Create(aValue, aLiteral);
}

NS_IMETHODIMP
RDFServiceImpl::GetDateLiteral(PRTime time, nsIRDFDate** literal)
{
    // XXX how do we cache these? should they live in their own hashtable?
    DateImpl* result = new DateImpl(time);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *literal = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::GetIntLiteral(PRInt32 value, nsIRDFInt** literal)
{
    // XXX how do we cache these? should they live in their own hashtable?
    IntImpl* result = new IntImpl(value);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *literal = result;
    NS_ADDREF(result);
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

#ifdef REUSE_RESOURCE_URI_AS_KEY
    const char* uri;
    rv = aResource->GetValueConst(&uri);
#else
    nsXPIDLCString uri;
    rv = aResource->GetValue(getter_Copies(uri));
#endif
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get URI from resource");
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(uri != nsnull, "resource has no URI");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    PLHashNumber keyhash = (*mResources->keyHash)(uri);
    PLHashEntry** hep = PL_HashTableRawLookup(mResources, keyhash, uri);

    if (*hep) {
        if (!aReplace) {
            NS_WARNING("resource already registered, and replace not specified");
            return NS_ERROR_FAILURE;    // already registered
        }

        // N.B., we do _not_ release the original resource because we
        // only ever held a weak reference to it. We simply replace
        // it.

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   replace-resource [%p] <-- [%p] %s",
                (*hep)->value, aResource, (const char*) uri));

#ifdef REUSE_RESOURCE_URI_AS_KEY
        (*hep)->key   = uri;
#endif
        (*hep)->value = aResource;
    }
    else {
#ifdef REUSE_RESOURCE_URI_AS_KEY
        PL_HashTableRawAdd(mResources, hep, keyhash, uri, aResource);
#else
        const char* key = PL_strdup(uri);
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableRawAdd(mResources, hep, keyhash, key, aResource);
#endif

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   register-resource [%p] %s",
                aResource, (const char*) uri));

        // N.B., we only hold a weak reference to the resource: that
        // way, the resource can be destroyed when the last refcount
        // goes away. The single addref that the CreateResource() call
        // made will be owned by the callee.
    }

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::UnregisterResource(nsIRDFResource* aResource)
{
    NS_PRECONDITION(aResource != nsnull, "null ptr");
    if (! aResource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef REUSE_RESOURCE_URI_AS_KEY
    const char* uri;
    rv = aResource->GetValueConst(&uri);
#else
    nsXPIDLCString uri;
    rv = aResource->GetValue(getter_Copies(uri));
#endif
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(uri != nsnull, "resource has no URI");
    if (! uri)
        return NS_ERROR_UNEXPECTED;

    PLHashEntry** hep = PL_HashTableRawLookup(mResources, (*mResources->keyHash)(uri), uri);
    if (!hep || !*hep)
        return NS_OK;

#ifndef REUSE_RESOURCE_URI_AS_KEY
    PL_strfree((char*) (*hep)->key);
#endif

    // N.B. that we _don't_ release the resource: we only held a weak
    // reference to it in the hashtable.

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-resource [%p] %s",
            aResource, (const char*) uri));

    PL_HashTableRawRemove(mResources, hep, *hep);
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
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("rdfserv    replace-datasource [%p] <-- [%p] %s",
                (*hep)->value, aDataSource, (const char*) uri));

        (*hep)->value = aDataSource;
    }
    else {
        const char* key = PL_strdup(uri);
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mNamedDataSources, key, aDataSource);

        PR_LOG(gLog, PR_LOG_ALWAYS,
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

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfserv unregister-datasource [%p] %s",
            aDataSource, (const char*) uri));

    return NS_OK;
}

NS_IMETHODIMP
RDFServiceImpl::GetDataSource(const char* aURI, nsIRDFDataSource** aDataSource)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // First, check the cache to see if we already have this
    // datasource loaded and initialized.
    {
        nsIRDFDataSource* cached =
            NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, aURI));

        if (cached) {
            NS_ADDREF(cached);
            *aDataSource = cached;
            return NS_OK;
        }
    }

    // Nope. So go to the repository to try to create it.
    nsCOMPtr<nsIRDFDataSource> ds;
	nsAutoString rdfName; rdfName.AssignWithConversion(aURI);
    static const char kRDFPrefix[] = "rdf:";
    PRInt32 pos = rdfName.Find(kRDFPrefix);
    if (pos == 0) {
        // It's a built-in data source
        nsAutoString dataSourceName;
        rdfName.Right(dataSourceName, rdfName.Length() - (pos + sizeof(kRDFPrefix) - 1));

        // Safely convert it to a C-string for the XPCOM APIs
        nsCAutoString contractID(
                NS_LITERAL_CSTRING(NS_RDF_DATASOURCE_CONTRACTID_PREFIX) +
                NS_LossyConvertUCS2toASCII(dataSourceName));

        /* strip params to get ``base'' contractID for data source */
        PRInt32 p = contractID.FindChar(PRUnichar('&'));
        if (p != kNotFound)
            contractID.Truncate(p);

        nsCOMPtr<nsISupports> isupports;
        rv = nsServiceManager::GetService(contractID.get(), kISupportsIID,
                                          getter_AddRefs(isupports), nsnull);

        if (NS_FAILED(rv)) return rv;

        ds = do_QueryInterface(isupports, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds);
        if (remote) {
            rv = remote->Init(aURI);
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        // Try to load this as an RDF/XML data source
        ds = do_CreateInstance(kRDFXMLDataSourceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        // Start the datasource load asynchronously. If you wanted it
        // loaded synchronously, then you should've tried to do it
        // yourself.
        nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(ds));
        NS_ASSERTION(remote, "not a remote RDF/XML data source!");
        if (! remote) return NS_ERROR_UNEXPECTED;

        rv = remote->Init(aURI);
        if (NS_FAILED(rv)) return rv;

        rv = remote->Refresh(PR_FALSE);
        if (NS_FAILED(rv)) return rv;
    }

    *aDataSource = ds;
    NS_ADDREF(*aDataSource);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
RDFServiceImpl::RegisterLiteral(nsIRDFLiteral* aLiteral, PRBool aReplace)
{
    NS_PRECONDITION(aLiteral != nsnull, "null ptr");
    if (! aLiteral)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

#ifdef REUSE_LITERAL_VALUE_AS_KEY
    const PRUnichar* value;
    rv = aLiteral->GetValueConst(&value);
#else
    nsXPIDLString value;
    rv = aLiteral->GetValue(getter_Copies(value));
#endif
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get literal's value");
    if (NS_FAILED(rv)) return rv;

    PLHashEntry** hep = PL_HashTableRawLookup(mLiterals, (*mLiterals->keyHash)(value), value);

    if (*hep) {
        if (! aReplace) {
            NS_WARNING("literal already registered and replace not specified");
            return NS_ERROR_FAILURE;
        }

        // N.B., we do _not_ release the original literal because we
        // only ever held a weak reference to it. We simply replace
        // it.

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   replace-literal [%p] <-- [%p] %s",
                (*hep)->value, aLiteral, (const PRUnichar*) value));

#ifdef REUSE_LITERAL_VALUE_AS_KEY
        (*hep)->key   = value;
#endif
        (*hep)->value = aLiteral;
    }
    else {
#ifdef REUSE_LITERAL_VALUE_AS_KEY
        PL_HashTableAdd(mLiterals, value, aLiteral);
#else
        const PRUnichar* key = value.get() ? nsCRT::strdup(value.get()) : 0;
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mLiterals, key, aLiteral);
#endif

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("rdfserv   register-literal [%p] %s",
                aLiteral, (const PRUnichar*) value));

        // N.B., we only hold a weak reference to the literal: that
        // way, the literal can be destroyed when the last refcount
        // goes away. The single addref that the CreateLiteral() call
        // made will be owned by the callee.
    }

    return NS_OK;
}


nsresult
RDFServiceImpl::UnregisterLiteral(nsIRDFLiteral* aLiteral)
{
    NS_PRECONDITION(aLiteral != nsnull, "null ptr");

    nsresult rv;

#ifdef REUSE_LITERAL_VALUE_AS_KEY
    const PRUnichar* value;
    rv = aLiteral->GetValueConst(&value);
#else
    nsXPIDLString value;
    rv = aLiteral->GetValue(getter_Copies(value));
#endif
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get literal's value");
    if (NS_FAILED(rv)) return rv;

    PLHashEntry** hep = PL_HashTableRawLookup(mLiterals, (*mLiterals->keyHash)(value), value);
    if (!hep || !*hep)
        return NS_OK;

#ifndef REUSE_LITERAL_VALUE_AS_KEY
    nsMemory::Free((void*) (*hep)->key);
#endif

    // N.B. that we _don't_ release the literal: we only held a weak
    // reference to it in the hashtable.

    PR_LOG(gLog, PR_LOG_DEBUG,
           ("rdfserv unregister-literal [%p] %s",
            aLiteral, (const PRUnichar*) value));

    PL_HashTableRawRemove(mLiterals, hep, *hep);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFService(nsIRDFService** mgr)
{
    return RDFServiceImpl::GetRDFService(mgr);
}

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

  This file provides the implementation for the RDF service manager.

  TO DO
  -----

  1) Implement the CreateDataBase() methods.

  2) Cache date and int literals.

 */

#include "nsCOMPtr.h"
#include "nsIAllocator.h"
#include "nsIAtom.h"
#include "nsIComponentManager.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "prprf.h"
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
// ServiceImpl
//
//   This is the RDF service.
//
class ServiceImpl : public nsIRDFService
{
protected:
    PLHashTable* mNamedDataSources;
    PLHashTable* mResources;
    PLHashTable* mLiterals;

    ServiceImpl(void);
    virtual ~ServiceImpl(void);

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

static ServiceImpl* gRDFService; // The one-and-only RDF service


////////////////////////////////////////////////////////////////////////
// LiteralImpl
//
//   Currently, all literals are implemented exactly the same way;
//   i.e., there is are no resource factories to allow you to generate
//   customer resources. I doubt that makes sense, anyway.
//
class LiteralImpl : public nsIRDFLiteral {
public:
    LiteralImpl(const PRUnichar* s);
    virtual ~LiteralImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_DECL_NSIRDFNODE

    // nsIRDFLiteral
    NS_DECL_NSIRDFLITERAL

private:
    nsAutoString mValue;
};


LiteralImpl::LiteralImpl(const PRUnichar* s)
    : mValue(s)
{
    NS_INIT_REFCNT();
    gRDFService->RegisterLiteral(this);
}

LiteralImpl::~LiteralImpl(void)
{
    gRDFService->UnregisterLiteral(this);
}

NS_IMPL_ADDREF(LiteralImpl);
NS_IMPL_RELEASE(LiteralImpl);

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

    *value = nsXPIDLString::Copy(mValue.GetUnicode());
    return NS_OK;
}


NS_IMETHODIMP
LiteralImpl::GetValueConst(const PRUnichar** aValue)
{
    *aValue = mValue.GetUnicode();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// DateImpl
//

class DateImpl : public nsIRDFDate {
public:
    DateImpl(const PRTime s);
    virtual ~DateImpl(void);

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

DateImpl::~DateImpl(void)
{
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
    virtual ~IntImpl(void);

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

IntImpl::~IntImpl(void)
{
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
// ServiceImpl

static PLHashNumber
rdf_HashWideString(const void* key)
{
    PLHashNumber result = 0;
    for (PRUnichar* s = (PRUnichar*) key; *s != nsnull; ++s)
        result = (result >> 28) ^ (result << 4) ^ *s;
    return result;
}

static PRIntn
rdf_CompareWideStrings(const void* v1, const void* v2)
{
    return 0 == nsCRT::strcmp(NS_STATIC_CAST(const PRUnichar*, v1), NS_STATIC_CAST(const PRUnichar*, v2));
}


ServiceImpl::ServiceImpl(void)
    :  mNamedDataSources(nsnull), mResources(nsnull), mLiterals(nsnull)
{
    NS_INIT_REFCNT();
    mResources = PL_NewHashTable(1023,              // nbuckets
                                 PL_HashString,     // hash fn
                                 PL_CompareStrings, // key compare fn
                                 PL_CompareValues,  // value compare fn
                                 nsnull, nsnull);   // alloc ops & priv

    mLiterals = PL_NewHashTable(1023,
                                rdf_HashWideString,
                                rdf_CompareWideStrings,
                                PL_CompareValues,
                                nsnull, nsnull);

    mNamedDataSources = PL_NewHashTable(23,
                                        PL_HashString,
                                        PL_CompareStrings,
                                        PL_CompareValues,
                                        nsnull, nsnull);

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsRDFService");
#endif
}


ServiceImpl::~ServiceImpl(void)
{
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
ServiceImpl::GetRDFService(nsIRDFService** mgr)
{
    if (! gRDFService) {
        ServiceImpl* serv = new ServiceImpl();
        if (! serv)
            return NS_ERROR_OUT_OF_MEMORY;
        gRDFService = serv;
    }

    NS_ADDREF(gRDFService);
    *mgr = gRDFService;
    return NS_OK;
}


NS_IMPL_ADDREF(ServiceImpl);
NS_IMPL_RELEASE(ServiceImpl);
NS_IMPL_QUERY_INTERFACE(ServiceImpl, kIRDFServiceIID);


NS_IMETHODIMP
ServiceImpl::GetResource(const char* aURI, nsIRDFResource** aResource)
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
    while ((*p) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p == '_')) && (*p != ':'))
        ++p;

    nsresult rv;
    nsCOMPtr<nsIFactory> factory;

    if (*p == ':') {
        // There _was_ a scheme. Try to find a factory using the
        // component manager.
        static const char kRDFResourceFactoryProgIDPrefix[]
            = NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX;

        PRInt32 pos = p - aURI;
        PRInt32 len = pos + sizeof(kRDFResourceFactoryProgIDPrefix) - 1;

        // Safely convert to a C-string for the XPCOM APIs
        char buf[128];
        char* progID = buf;
        if (len >= PRInt32(sizeof buf))
            progID = new char[len + 1];

        if (progID == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_strcpy(progID, kRDFResourceFactoryProgIDPrefix);
        PL_strncpy(progID + sizeof(kRDFResourceFactoryProgIDPrefix) - 1, aURI, pos);
        progID[len] = '\0';

        nsCID cid;
        rv = nsComponentManager::ProgIDToCLSID(progID, &cid);

        if (progID != buf)
            delete[] progID;

        if (NS_SUCCEEDED(rv)) {
            rv = nsComponentManager::FindFactory(cid, getter_AddRefs(factory));
            NS_ASSERTION(NS_SUCCEEDED(rv), "factory registered, but couldn't load");
            if (NS_FAILED(rv)) return rv;
        }
    }

    if (! factory) {
        // fall through to using the "default" resource factory if either:
        //
        // 1. The URI didn't have a scheme, or
        // 2. There was no resource factory registered for the scheme.
        rv = nsComponentManager::FindFactory(kRDFDefaultResourceCID, getter_AddRefs(factory));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get default resource factory");
        if (NS_FAILED(rv)) return rv;
    }


    rv = factory->CreateInstance(nsnull, nsIRDFResource::GetIID(), (void**) &result);
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
ServiceImpl::GetUnicodeResource(const PRUnichar* aURI, nsIRDFResource** aResource)
{
    char buf[128];
    char* uri = buf;

    PRUint32 len = nsCRT::strlen(aURI);
    if (len >= sizeof(buf)) {
        uri = new char[len + 1];
        if (! uri) return NS_ERROR_OUT_OF_MEMORY;
    }

    /* CopyChars2To1(uri, 0, aURI, 0, len); // XXX not exported */
    const PRUnichar* src = aURI;
    char* dst = uri;
    while (*src) {
        *dst = (*src < 256) ? char(*src) : '.';
        ++src;
        ++dst;
    }
    *dst = '\0';

    nsresult rv = GetResource(uri, aResource);

    if (uri != buf)
        delete[] uri;

    return rv;
}


NS_IMETHODIMP
ServiceImpl::GetLiteral(const PRUnichar* aValue, nsIRDFLiteral** aLiteral)
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
    literal = new LiteralImpl(aValue);
    if (! literal)
        return NS_ERROR_OUT_OF_MEMORY;

    *aLiteral = literal;
    NS_ADDREF(literal);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::GetDateLiteral(PRTime time, nsIRDFDate** literal)
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
ServiceImpl::GetIntLiteral(PRInt32 value, nsIRDFInt** literal)
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
ServiceImpl::RegisterResource(nsIRDFResource* aResource, PRBool replace)
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

    PLHashEntry** hep = PL_HashTableRawLookup(mResources, (*mResources->keyHash)(uri), uri);

    if (*hep) {
        if (!replace) {
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
        PL_HashTableAdd(mResources, uri, aResource);
#else
        const char* key = PL_strdup(uri);
        if (! key)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mResources, key, aResource);
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
ServiceImpl::UnregisterResource(nsIRDFResource* aResource)
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
    NS_ASSERTION(*hep != nsnull, "resource wasn't registered");
    if (! *hep)
        return NS_ERROR_FAILURE;

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
ServiceImpl::RegisterDataSource(nsIRDFDataSource* aDataSource, PRBool replace)
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
        if (! replace)
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
ServiceImpl::UnregisterDataSource(nsIRDFDataSource* aDataSource)
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

    NS_ASSERTION(*hep != nsnull, "datasource was never registered");
    if (! *hep)
        return NS_ERROR_ILLEGAL_VALUE;

    PL_strfree((char*) (*hep)->key);

    // N.B., we only held a weak reference to the datasource, so we
    // don't release here.
    PL_HashTableRawRemove(mNamedDataSources, hep, *hep);

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("rdfserv unregister-datasource [%p] %s",
            aDataSource, (const char*) uri));

    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::GetDataSource(const char* uri, nsIRDFDataSource** aDataSource)
{
    nsresult rv;

    // First, check the cache to see if we already have this
    // datasource loaded and initialized.
    {
        nsIRDFDataSource* cached =
            NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

        if (cached) {
            nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(cached);
            if (remote) {
                rv = remote->Refresh(PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }

            NS_ADDREF(cached);
            *aDataSource = cached;
            return NS_OK;
        }
    }

    // Nope. So go to the repository to try to create it.
    nsCOMPtr<nsIRDFDataSource> ds;
	nsAutoString rdfName(uri);
    static const char kRDFPrefix[] = "rdf:";
    PRInt32 pos = rdfName.Find(kRDFPrefix);
    if (pos == 0) {
        // It's a built-in data source
        nsAutoString dataSourceName;
        rdfName.Right(dataSourceName, rdfName.Length() - (pos + sizeof(kRDFPrefix) - 1));

        nsAutoString progIDStr(NS_RDF_DATASOURCE_PROGID_PREFIX);
        progIDStr.Append(dataSourceName);

        // Safely convert it to a C-string for the XPCOM APIs
        char buf[64];
        char* progID = buf, *p;
        if (progIDStr.Length() >= PRInt32(sizeof buf))
            progID = new char[progIDStr.Length() + 1];

        if (progID == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        progIDStr.ToCString(progID, progIDStr.Length() + 1);

        /* strip params to get ``base'' progID for data source */
        p = PL_strchr(progID, ';');
        if (p)
            *p = '\0';

        nsCOMPtr<nsISupports> isupports;
        rv = nsServiceManager::GetService(progID, kISupportsIID, getter_AddRefs(isupports), nsnull);

        if (progID != buf)
            delete[] progID;

        if (NS_FAILED(rv)) return rv;

        ds = do_QueryInterface(isupports, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds);
        if (remote) {
            rv = remote->Init(uri);
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        // Try to load this as an RDF/XML data source
        rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                nsnull,
                                                nsIRDFDataSource::GetIID(),
                                                (void**) &ds);

        if (NS_FAILED(rv)) return rv;

        // Start the datasource load asynchronously. If you wanted it
        // loaded synchronously, then you should've tried to do it
        // yourself.
        nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(ds));
        NS_ASSERTION(remote, "not a remote RDF/XML data source!");
        if (! remote) return NS_ERROR_UNEXPECTED;

        rv = remote->Init(uri);
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
ServiceImpl::RegisterLiteral(nsIRDFLiteral* aLiteral, PRBool aReplace)
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
        const PRUnichar* key = nsXPIDLString::Copy(value);
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
ServiceImpl::UnregisterLiteral(nsIRDFLiteral* aLiteral)
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
    NS_ASSERTION(*hep != nsnull, "literal wasn't registered");
    if (! *hep)
        return NS_ERROR_FAILURE;

#ifndef REUSE_LITERAL_VALUE_AS_KEY
    nsAllocator::Free((void*) (*hep)->key);
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
    return ServiceImpl::GetRDFService(mgr);
}

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

#include "nsIAtom.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFResourceFactory.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prlog.h"
#include "nsIRDFXMLDataSource.h"

#if 0
#ifdef XP_MAC
#define HACK_DONT_USE_LIBREG 1
#endif
#endif

#if HACK_DONT_USE_LIBREG
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
static NS_DEFINE_CID(kRDFBookmarkDataSourceCID,  NS_RDFBOOKMARKDATASOURCE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContentSinkCID,         NS_RDFCONTENTSINK_CID);
static NS_DEFINE_CID(kRDFHTMLBuilderCID,         NS_RDFHTMLBUILDER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,  NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFTreeBuilderCID,         NS_RDFTREEBUILDER_CID);
static NS_DEFINE_CID(kRDFMenuBuilderCID,         NS_RDFMENUBUILDER_CID);
static NS_DEFINE_CID(kRDFToolbarBuilderCID,      NS_RDFTOOLBARBUILDER_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,       NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXULBuilderCID,          NS_RDFXULBUILDER_CID);
static NS_DEFINE_CID(kXULContentSinkCID,         NS_XULCONTENTSINK_CID);
static NS_DEFINE_CID(kXULDataSourceCID,			 NS_XULDATASOURCE_CID);
static NS_DEFINE_CID(kXULDocumentCID,            NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kRDFDefaultResourceCID,     NS_RDFDEFAULTRESOURCE_CID);
#endif

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFDateIID,         NS_IRDFDATE_IID);
static NS_DEFINE_IID(kIRDFIntIID,         NS_IRDFINT_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

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
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource);
    NS_IMETHOD FindResource(const char* uri, nsIRDFResource** resource, PRBool *found);
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource);
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal);
    NS_IMETHOD GetDateLiteral(const PRTime value, nsIRDFDate** date) ;
    NS_IMETHOD GetIntLiteral(const int32 value, nsIRDFInt** intLiteral);
    NS_IMETHOD RegisterResource(nsIRDFResource* aResource, PRBool replace = PR_FALSE);
    NS_IMETHOD UnregisterResource(nsIRDFResource* aResource);
    NS_IMETHOD RegisterDataSource(nsIRDFDataSource* dataSource, PRBool replace = PR_FALSE);
    NS_IMETHOD UnregisterDataSource(nsIRDFDataSource* dataSource);
    NS_IMETHOD GetDataSource(const char* uri, nsIRDFDataSource** dataSource);
    NS_IMETHOD CreateDatabase(const char** uris, nsIRDFDataBase** dataBase);
    NS_IMETHOD CreateBrowserDatabase(nsIRDFDataBase** dataBase);

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
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;

    // nsIRDFLiteral
    NS_IMETHOD GetValue(const PRUnichar* *value) const;
    NS_IMETHOD EqualsLiteral(const nsIRDFLiteral* literal, PRBool* result) const;

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
LiteralImpl::EqualsNode(nsIRDFNode* node, PRBool* result) const
{
    nsresult rv;
    nsIRDFLiteral* literal;
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
        rv = EqualsLiteral(literal, result);
        NS_RELEASE(literal);
    }
    else {
        *result = PR_FALSE;
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
LiteralImpl::GetValue(const PRUnichar* *value) const
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue.GetUnicode();
    return NS_OK;
}


NS_IMETHODIMP
LiteralImpl::EqualsLiteral(const nsIRDFLiteral* literal, PRBool* result) const
{
    NS_ASSERTION(literal && result, "null ptr");
    if (!literal || !result)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    const PRUnichar* p;
    if (NS_FAILED(rv = literal->GetValue(&p)))
        return rv;

    nsAutoString s(p);

    *result = s.Equals(mValue);
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
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;

    // nsIRDFDate
    NS_IMETHOD GetValue(PRTime *value) const;
    NS_IMETHOD EqualsDate(const nsIRDFDate* date, PRBool* result) const;

private:
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
DateImpl::EqualsNode(nsIRDFNode* node, PRBool* result) const
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
DateImpl::GetValue(PRTime *value) const
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue;
    return NS_OK;
}


NS_IMETHODIMP
DateImpl::EqualsDate(const nsIRDFDate* date, PRBool* result) const
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
    IntImpl(const int32 s);
    virtual ~IntImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;

    // nsIRDFInt
    NS_IMETHOD GetValue(int32 *value) const;
    NS_IMETHOD EqualsInt(const nsIRDFInt* value, PRBool* result) const;

private:
    int32 mValue;
};


IntImpl::IntImpl(const int32 s)
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
IntImpl::EqualsNode(nsIRDFNode* node, PRBool* result) const
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
IntImpl::GetValue(int32 *value) const
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue;
    return NS_OK;
}


NS_IMETHODIMP
IntImpl::EqualsInt(const nsIRDFInt* intValue, PRBool* result) const
{
    NS_ASSERTION(intValue && result, "null ptr");
    if (!intValue || !result)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    int32 p;
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
                                PL_CompareStrings,
                                PL_CompareValues,
                                nsnull, nsnull);

    mNamedDataSources = PL_NewHashTable(23,
                                        PL_HashString,
                                        PL_CompareStrings,
                                        PL_CompareValues,
                                        nsnull, nsnull);
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



NS_IMETHODIMP_(nsrefcnt)
ServiceImpl::AddRef(void)
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt)
ServiceImpl::Release(void)
{
    return 1;
}


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
    nsresult rv;
    nsAutoString uriStr(aURI);
    PRInt32 pos = uriStr.Find(':');
    if (pos < 0) {
        // no colon, so try the default resource factory
#if HACK_DONT_USE_LIBREG
        nsIServiceManager* servMgr;
        nsServiceManager::GetGlobalServiceManager(&servMgr);
        nsIFactory* fact;
        rv = NSGetFactory(servMgr, kRDFDefaultResourceCID,
                          "", NS_RDF_RESOURCE_FACTORY_PROGID, &fact);
        if (rv == NS_OK) {
            rv = fact->CreateInstance(nsnull, nsIRDFResource::GetIID(),
                                      (void**)&result);
            NS_RELEASE(fact);
        }
#else
        rv = nsComponentManager::CreateInstance(NS_RDF_RESOURCE_FACTORY_PROGID,
                                          nsnull, nsIRDFResource::GetIID(),
                                          (void**)&result);
#endif
        if (NS_FAILED(rv)) {
            NS_ERROR("unable to create resource");
            return rv;
        }
    }
    else {
        // the resource is qualified, so construct a ProgID and
        // construct it from the repository.
        nsAutoString progIDStr;
        uriStr.Left(progIDStr, pos);      // truncate
        progIDStr.Insert(NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX, 0);

        // Safely convert to a C-string for the XPCOM APIs
        char buf[128];
        char* progID = buf;
        if (progIDStr.Length() >= sizeof(buf))
            progID = new char[progIDStr.Length() + 1];

        if (progID == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        progIDStr.ToCString(progID, progIDStr.Length() + 1);

#if HACK_DONT_USE_LIBREG
        nsIServiceManager* servMgr;
        nsServiceManager::GetGlobalServiceManager(&servMgr);
        nsIFactory* fact;
        rv = NSGetFactory(servMgr, kRDFDefaultResourceCID,
                          "", progID, &fact);
        if (rv == NS_OK) {
            rv = fact->CreateInstance(nsnull, nsIRDFResource::GetIID(),
                                      (void**)&result);
            NS_RELEASE(fact);
        }
#else
        rv = nsComponentManager::CreateInstance(progID, nsnull,
                                          nsIRDFResource::GetIID(),
                                          (void**)&result);
#endif
        if (progID != buf)
            delete[] progID;

        if (NS_FAILED(rv)) {
            // if we failed, try the default resource factory
#if HACK_DONT_USE_LIBREG
            nsIServiceManager* servMgr;
            nsServiceManager::GetGlobalServiceManager(&servMgr);
            nsIFactory* fact;
            rv = NSGetFactory(servMgr, kRDFDefaultResourceCID,
                              "", NS_RDF_RESOURCE_FACTORY_PROGID, &fact);
            if (rv == NS_OK) {
                rv = fact->CreateInstance(nsnull, nsIRDFResource::GetIID(),
                                          (void**)&result);
                NS_RELEASE(fact);
            }
#else
            rv = nsComponentManager::CreateInstance(NS_RDF_RESOURCE_FACTORY_PROGID,
                                              nsnull, nsIRDFResource::GetIID(),
                                              (void**)&result);
#endif
            if (NS_FAILED(rv)) {
                NS_ERROR("unable to create resource");
                return rv;
            }
        }
    }

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
ServiceImpl::FindResource(const char* uri, nsIRDFResource** resource, PRBool *found)
{
    nsIRDFResource* result =
        NS_STATIC_CAST(nsIRDFResource*, PL_HashTableLookup(mResources, uri));

    if (result) {
        *resource = result;
        *found = 1;
    } else {
        *found = 0;
    }
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::GetUnicodeResource(const PRUnichar* aURI, nsIRDFResource** aResource)
{
    nsString uriStr(aURI);
    char buf[128];
    char* uri = buf;

    if (uriStr.Length() >= sizeof(buf))
        uri = new char[uriStr.Length() + 1];

    uriStr.ToCString(uri, uriStr.Length() + 1);

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
ServiceImpl::GetDateLiteral(const PRTime time, nsIRDFDate** literal)
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
ServiceImpl::GetIntLiteral(const int32 value, nsIRDFInt** literal)
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

    const char* uri;
    rv = aResource->GetValue(&uri);
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to get URI from resource");
        return rv;
    }

    NS_ASSERTION(uri != nsnull, "resource has no URI");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    nsIRDFResource* prevRes =
        NS_STATIC_CAST(nsIRDFResource*, PL_HashTableLookup(mResources, uri));

    if (prevRes && !replace) {
        NS_WARNING("resource already registered, and replace not specified");
        return NS_ERROR_FAILURE;    // already registered
    }

    // This is a little trick to make storage more efficient. For
    // the "key" in the table, we'll use the string value that's
    // stored as a member variable of the nsIRDFResource object.
    PL_HashTableAdd(mResources, uri, aResource);

    // *We* don't AddRef() the resource: that way, the resource
    // can be garbage collected when the last refcount goes
    // away. The single addref that the CreateResource() call made
    // will be owned by the callee.
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::UnregisterResource(nsIRDFResource* resource)
{
    NS_PRECONDITION(resource != nsnull, "null ptr");
    if (! resource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = resource->GetValue(&uri)))
        return rv;

    PL_HashTableRemove(mResources, uri);

    // N.B. that we _don't_ release the resource: we only held a weak
    // reference to it in the hashtable.

    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::RegisterDataSource(nsIRDFDataSource* aDataSource, PRBool replace)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = aDataSource->GetURI(&uri)))
        return rv;

    nsIRDFDataSource* ds =
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    if (ds) {
        if (replace)
            NS_RELEASE(ds);
        else
            return NS_ERROR_FAILURE;    // already registered
    }

    PL_HashTableAdd(mNamedDataSources, uri, aDataSource);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::UnregisterDataSource(nsIRDFDataSource* aDataSource)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = aDataSource->GetURI(&uri)))
        return rv;

    nsIRDFDataSource* ds =
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    if (! ds)
        return NS_ERROR_ILLEGAL_VALUE;

    PL_HashTableRemove(mNamedDataSources, uri);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::GetDataSource(const char* uri, nsIRDFDataSource** aDataSource)
{
    // First, check the cache to see if we already have this
    // datasource loaded and initialized.
    nsIRDFDataSource* ds =
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    if (ds) {
        NS_ADDREF(ds);
        *aDataSource = ds;
        return NS_OK;
    }

    // Nope. So go to the repository to try to create it.
    nsresult rv;
	nsAutoString rdfName(uri);
    if (rdfName.Find("rdf:") == 0) {    // if this is a datasource component
        PRInt32 pos = 3;
        nsAutoString dataSourceName;
        rdfName.Right(dataSourceName, rdfName.Length() - (pos + 1));

        nsAutoString progIDStr(NS_RDF_DATASOURCE_PROGID_PREFIX);
        progIDStr.Append(dataSourceName);

        // Safely convert it to a C-string for the XPCOM APIs
        char buf[64];
        char* progID = buf;
        if (progIDStr.Length() >= sizeof(buf))
            progID = new char[progIDStr.Length() + 1];

        if (progID == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        progIDStr.ToCString(progID, progIDStr.Length() + 1);

#if HACK_DONT_USE_LIBREG
        nsIServiceManager* servMgr;
        nsServiceManager::GetGlobalServiceManager(&servMgr);
        nsIFactory* fact;
        nsCID cid;
        if (dataSourceName.Equals("bookmarks"))
            cid = kRDFBookmarkDataSourceCID;
        else if (dataSourceName.Equals("composite-datasource"))
            cid = kRDFCompositeDataSourceCID;
        else if (dataSourceName.Equals("in-memory-datasource"))
            cid = kRDFInMemoryDataSourceCID;
        else if (dataSourceName.Equals("xml-datasource"))
            cid = kRDFXMLDataSourceCID;
        else if (dataSourceName.Equals("xul-datasource"))
            cid = kXULDataSourceCID;
        else {
            NS_ERROR("unknown data source");
        }

        rv = NSGetFactory(servMgr, cid,
                          "", progID, &fact);
        if (rv == NS_OK) {
            rv = fact->CreateInstance(nsnull, nsIRDFDataSource::GetIID(),
                                      (void**)&ds);
            NS_RELEASE(fact);
        }
#else
        rv = nsComponentManager::CreateInstance(progID, nsnull,
                                                nsIRDFDataSource::GetIID(),
                                                (void**)&ds);
#endif

        if (progID != buf)
            delete[] progID;

        if (NS_FAILED(rv)) {
            // XXX only a warning, because the URI may have been ill-formed.
            NS_WARNING("unable to create data source");
            return rv;
        }
    }
    else {
        // else try to load it as a file and create an xml datasource
        rv = NS_NewRDFXMLDataSource((nsIRDFXMLDataSource**)&ds);
        if (NS_FAILED(rv)) return rv;
    }

    rv = ds->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(ds);
        NS_ERROR("unable to initialize data source");
        return rv;
    }
    *aDataSource = ds;
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::CreateDatabase(const char** uri, nsIRDFDataBase** dataBase)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
ServiceImpl::CreateBrowserDatabase(nsIRDFDataBase** dataBase)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////

nsresult
ServiceImpl::RegisterLiteral(nsIRDFLiteral* aLiteral, PRBool aReplace)
{
    NS_PRECONDITION(aLiteral != nsnull, "null ptr");

    nsresult rv;
    const PRUnichar* value;
    if (NS_FAILED(rv = aLiteral->GetValue(&value))) {
        NS_ERROR("unable to get literal's value");
        return rv;
    }

    nsIRDFLiteral* prevLiteral =
        NS_STATIC_CAST(nsIRDFLiteral*, PL_HashTableLookup(mLiterals, value));

    if (prevLiteral) {
        if (aReplace) {
            NS_RELEASE(prevLiteral);
        }
        else {
            NS_WARNING("literal already registered and replace not specified");
            return NS_ERROR_FAILURE;
        }
    }

    PL_HashTableAdd(mLiterals, value, aLiteral);
    return NS_OK;
}


nsresult
ServiceImpl::UnregisterLiteral(nsIRDFLiteral* aLiteral)
{
    NS_PRECONDITION(aLiteral != nsnull, "null ptr");

    nsresult rv;

    const PRUnichar* value;
    if (NS_FAILED(rv = aLiteral->GetValue(&value))) {
        NS_ERROR("unable to get literal's value");
        return rv;
    }

    PL_HashTableRemove(mLiterals, value);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFService(nsIRDFService** mgr)
{
    return ServiceImpl::GetRDFService(mgr);
}

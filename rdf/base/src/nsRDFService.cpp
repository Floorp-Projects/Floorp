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
     
 */

#include "nsIAtom.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFResourceFactory.h"
#include "nsString.h"
#include "nsRepository.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prlog.h"

#ifdef XP_MAC
#define HACK_DONT_USE_LIBREG 1
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
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////
// PrefixMap
//
//   A simple map from a string prefix to a value. It maintains an
//   ordered list of prefixes that map to a <tt>void*</tt>. The list
//   is sorted in ASCII order such that if one prefix <b>p</b> is
//   itself a prefix of another prefix <b>q</b>, the longest prefix
//   (<b>q</b>) will appear first in the list. The idea is that you
//   want to find the "best" match first. (Will this actually be
//   useful? Probabably not...)
//
class PrefixMap
{
private:
    struct PrefixMapEntry
    {
        const char*     mPrefix;
        PRInt32         mPrefixLen;
        const void*     mValue;
        PrefixMapEntry* mNext;
    };

    PrefixMapEntry* mHead;

public:
    PrefixMap();
    ~PrefixMap();

    PRBool Add(const char* aPrefix, const void* aValue);
    const void* Remove(const char* aPrefix);

    /**
     * Find the most specific value matching the specified string.
     */
    const void* Find(const char* aString);
};


PrefixMap::PrefixMap()
    : mHead(nsnull)
{
}

PrefixMap::~PrefixMap()
{
    while (mHead) {
        PrefixMapEntry* doomed = mHead;
        mHead = mHead->mNext;
        PL_strfree(NS_CONST_CAST(char*, doomed->mPrefix));
        delete doomed;
    }
}

PRBool
PrefixMap::Add(const char* aPrefix, const void* aValue)
{
    PRInt32 newPrefixLen = PL_strlen(aPrefix);
    PrefixMapEntry* entry = mHead;
    PrefixMapEntry* last = nsnull;

    while (entry != nsnull) {
        // check to see if the new prefix is longer than the current
        // entry. If so, we'll want to insert the new prefix *before*
        // this one.
        if (newPrefixLen > entry->mPrefixLen)
            break;

        // check for exact equality: if so, the prefix is already
        // registered, so fail (?)
        if (PL_strcmp(entry->mPrefix, aPrefix) == 0)
            return PR_FALSE;

        // otherwise, the new prefix is the same length or shorter
        // than the current entry: continue on to the next one.

        last = entry;
        entry = entry->mNext;
    }

    PrefixMapEntry* newEntry = new PrefixMapEntry;
    if (! newEntry)
        return PR_FALSE;


    newEntry->mPrefix    = PL_strdup(aPrefix);
    newEntry->mPrefixLen = newPrefixLen;
    newEntry->mValue     = aValue;

    if (last) {
        // we found an entry that we need to insert the current
        // entry *before*
        newEntry->mNext = last->mNext;
        last->mNext = newEntry;
    }
    else {
        // Otherwise, insert at the start
        newEntry->mNext = mHead;
        mHead = newEntry;
    }
    return PR_TRUE;
}

const void*
PrefixMap::Remove(const char* aPrefix)
{
    PrefixMapEntry* entry = mHead;
    PrefixMapEntry* last = nsnull;

    PRInt32 doomedPrefixLen = PL_strlen(aPrefix);

    while (entry != nsnull) {
        if ((doomedPrefixLen == entry->mPrefixLen) &&
            (PL_strcmp(entry->mPrefix, aPrefix) == 0)) {
            if (last) {
                last->mNext = entry->mNext;
            }
            else {
                mHead = entry->mNext;
            }

            const void* value = entry->mValue;

            PL_strfree(NS_CONST_CAST(char*, entry->mPrefix));
            delete entry;

            return value;
        }
        last = entry;
        entry = entry->mNext;
    }
    return nsnull;
}


const void*
PrefixMap::Find(const char* aString)
{
    for (PrefixMapEntry* entry = mHead; entry != nsnull; entry = entry->mNext) {
        PRInt32 cmp = PL_strncmp(entry->mPrefix, aString, entry->mPrefixLen);
        if (cmp == 0)
            return entry->mValue;
    }
    return nsnull;
}


////////////////////////////////////////////////////////////////////////
// LiteralImpl
//
//   Currently, all literals are implemented exactly the same way;
//   i.e., there is are no resource factories to allow you to generate
//   customer resources. I doubt that makes sense, anyway.
//
//   What _may_ make sense is to atomize literals (well, at least
//   short ones), to reduce in memory overhead at the expense of some
//   processing time.
//
class LiteralImpl : public nsIRDFLiteral {
public:
    LiteralImpl(const PRUnichar* s);
    virtual ~LiteralImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_IMETHOD Init(const char* uri);
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
}

LiteralImpl::~LiteralImpl(void)
{
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
LiteralImpl::Init(const char* uri)
{
    // Literals should always be constructed by calling nsIRDFService::GetLiteral,
    // so this method should never get called.
    NS_NOTREACHED("RDF LiteralImpl::Init");
    return NS_ERROR_FAILURE;
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
// ServiceImpl
//
//   This is the RDF service.
//
class ServiceImpl : public nsIRDFService
{
protected:
    PrefixMap    mResourceFactories;
    PLHashTable* mNamedDataSources;
    PLHashTable* mResources;

    ServiceImpl(void);
    virtual ~ServiceImpl(void);

    static nsIRDFService* gRDFService; // The one-and-only RDF service

public:

    static nsresult GetRDFService(nsIRDFService** result);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFService
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource);
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource);
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal);
    NS_IMETHOD RegisterResource(nsIRDFResource* aResource, PRBool replace = PR_FALSE);
    NS_IMETHOD UnregisterResource(nsIRDFResource* aResource);
    NS_IMETHOD RegisterDataSource(nsIRDFDataSource* dataSource, PRBool replace = PR_FALSE);
    NS_IMETHOD UnregisterDataSource(nsIRDFDataSource* dataSource);
    NS_IMETHOD GetDataSource(const char* uri, nsIRDFDataSource** dataSource);
    NS_IMETHOD CreateDatabase(const char** uris, nsIRDFDataBase** dataBase);
    NS_IMETHOD CreateBrowserDatabase(nsIRDFDataBase** dataBase);
};

nsIRDFService* ServiceImpl::gRDFService = nsnull;

////////////////////////////////////////////////////////////////////////

ServiceImpl::ServiceImpl(void)
    : mResources(nsnull), mNamedDataSources(nsnull)
{
    NS_INIT_REFCNT();
    mResources = PL_NewHashTable(1023,              // nbuckets
                                 PL_HashString,     // hash fn
                                 PL_CompareStrings, // key compare fn
                                 PL_CompareValues,  // value compare fn
                                 nsnull, nsnull);   // alloc ops & priv

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
ServiceImpl::GetResource(const char* uri, nsIRDFResource** resource)
{
    nsIRDFResource* result =
        NS_STATIC_CAST(nsIRDFResource*, PL_HashTableLookup(mResources, uri));
    
    if (result) {
        // Addref for the callee.
        NS_ADDREF(result);
        *resource = result;
        return NS_OK;
    }        
        
    nsresult rv;
    nsAutoString uriStr = uri;
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
            rv = fact->CreateInstance(nsnull, nsIRDFResource::IID(),
                                      (void**)&result);
            NS_RELEASE(fact);
        }
#else
        rv = nsRepository::CreateInstance(NS_RDF_RESOURCE_FACTORY_PROGID,
                                          nsnull, nsIRDFResource::IID(),
                                          (void**)&result);
#endif
        if (NS_FAILED(rv)) return rv;
    }
    else {
        nsAutoString prefix;
        uriStr.Left(prefix, pos);      // truncate
        char* prefixStr = prefix.ToNewCString();
        if (prefixStr == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        char* progID = PR_smprintf(NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "%s",
                                   prefixStr);
        delete[] prefixStr;
        if (progID == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
#if HACK_DONT_USE_LIBREG
        nsIServiceManager* servMgr;
        nsServiceManager::GetGlobalServiceManager(&servMgr);
        nsIFactory* fact;
        rv = NSGetFactory(servMgr, kRDFDefaultResourceCID,
                          "", progID, &fact);
        if (rv == NS_OK) {
            rv = fact->CreateInstance(nsnull, nsIRDFResource::IID(),
                                      (void**)&result);
            NS_RELEASE(fact);
        }
#else
        rv = nsRepository::CreateInstance(progID, nsnull,
                                          nsIRDFResource::IID(),
                                          (void**)&result);
#endif
        PR_smprintf_free(progID);
        if (NS_FAILED(rv)) {
            // if we failed, try the default resource factory
#if HACK_DONT_USE_LIBREG
            nsIServiceManager* servMgr;
            nsServiceManager::GetGlobalServiceManager(&servMgr);
            nsIFactory* fact;
            rv = NSGetFactory(servMgr, kRDFDefaultResourceCID,
                              "", NS_RDF_RESOURCE_FACTORY_PROGID, &fact);
            if (rv == NS_OK) {
                rv = fact->CreateInstance(nsnull, nsIRDFResource::IID(),
                                          (void**)&result);
                NS_RELEASE(fact);
            }
#else
            rv = nsRepository::CreateInstance(NS_RDF_RESOURCE_FACTORY_PROGID,
                                              nsnull, nsIRDFResource::IID(),
                                              (void**)&result);
#endif
            if (NS_FAILED(rv)) return rv;
        }
    }
    rv = result->Init(uri);
    if (NS_FAILED(rv)) {
        NS_RELEASE(result);
        return rv;
    }

    *resource = result;
    rv = RegisterResource(result);
    if (NS_FAILED(rv))
        NS_RELEASE(result);
    return rv;
}

NS_IMETHODIMP
ServiceImpl::GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource)
{
    nsString s(uri);
    char* cstr = s.ToNewCString();
    nsresult rv = GetResource(cstr, resource);
    delete[] cstr;
    return rv;
}


NS_IMETHODIMP
ServiceImpl::GetLiteral(const PRUnichar* uri, nsIRDFLiteral** literal)
{
    LiteralImpl* result = new LiteralImpl(uri);
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
    if (NS_FAILED(rv)) return rv;

    nsIRDFResource* prevRes =
        NS_STATIC_CAST(nsIRDFResource*, PL_HashTableLookup(mResources, uri));
    if (prevRes != nsnull) {
        if (replace)
            NS_RELEASE(prevRes);
        else
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
    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = resource->GetValue(&uri)))
        return rv;

    PL_HashTableRemove(mResources, uri);
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
    nsIRDFDataSource* ds =
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    if (ds) {
        NS_ADDREF(ds);
        *aDataSource = ds;
        return NS_OK;
    }
    nsresult rv;
	nsAutoString rdfName = uri;
    nsAutoString dataSourceName;
    PRInt32 pos = rdfName.Find(':');
    if (pos < 0) return NS_ERROR_FAILURE;       // bad URI

    rdfName.Right(dataSourceName, rdfName.Length() - (pos + 1));
    char* name = dataSourceName.ToNewCString();
    if (name == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    char* progID = PR_smprintf(NS_RDF_DATASOURCE_PROGID_PREFIX "%s",
                               name);
    delete[] name;
    if (progID == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
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
    else
        PR_ASSERT(0);

    rv = NSGetFactory(servMgr, cid,
                      "", progID, &fact);
    if (rv == NS_OK) {
        rv = fact->CreateInstance(nsnull, nsIRDFDataSource::IID(),
                                  (void**)aDataSource);
        NS_RELEASE(fact);
    }
#else
    rv = nsRepository::CreateInstance(progID, nsnull,
                                      nsIRDFDataSource::IID(),
                                      (void**)aDataSource);
#endif
    PR_smprintf_free(progID);
    if (NS_FAILED(rv)) return rv;
    rv = (*aDataSource)->Init(uri);
    if (NS_FAILED(rv)) return rv;
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::CreateDatabase(const char** uri, nsIRDFDataBase** dataBase)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
ServiceImpl::CreateBrowserDatabase(nsIRDFDataBase** dataBase)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewRDFService(nsIRDFService** mgr)
{
    return ServiceImpl::GetRDFService(mgr);
}

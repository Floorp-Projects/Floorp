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

  1) Figure out a better way to do "pluggable resources." Currently,
     we have two _major_ hacks:

       RegisterBuiltInNamedDataSources()
       RegisterBuiltInResourceFactories()

     These introduce dependencies on the datasource directory. You'd
     like to have this stuff discovered dynamically at startup or
     something. Maybe from the registry.

  2) Implement the CreateDataBase() methods.
     
 */

#include "nsIAtom.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFResourceFactory.h"
#include "nsString.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"

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

    static void RegisterBuiltInResourceFactories();
    static void RegisterBuiltInNamedDataSources();

public:

    static nsresult GetRDFService(nsIRDFService** result);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFService
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource);
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource);
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal);
    NS_IMETHOD UnCacheResource(nsIRDFResource* resource);

    NS_IMETHOD RegisterResourceFactory(const char* aURIPrefix, nsIRDFResourceFactory* aFactory);
    NS_IMETHOD UnRegisterResourceFactory(const char* aURIPrefix);

    NS_IMETHOD RegisterNamedDataSource(const char* uri, nsIRDFDataSource* dataSource);
    NS_IMETHOD UnRegisterNamedDataSource(const char* uri);
    NS_IMETHOD GetNamedDataSource(const char* uri, nsIRDFDataSource** dataSource);
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
    if (mResources) {
        PL_HashTableDestroy(mResources);
        mResources = nsnull;
    }
    if (mNamedDataSources) {
        PL_HashTableDestroy(mNamedDataSources);
        mNamedDataSources = nsnull;
    }
    gRDFService = nsnull;
}


nsresult
ServiceImpl::GetRDFService(nsIRDFService** mgr)
{
    if (! gRDFService) {
        gRDFService = new ServiceImpl();
        if (! gRDFService)
            return NS_ERROR_OUT_OF_MEMORY;

        RegisterBuiltInResourceFactories();
        RegisterBuiltInNamedDataSources();
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

    if (! result) {
        nsIRDFResourceFactory* factory =
            NS_STATIC_CAST(nsIRDFResourceFactory*,
                           NS_CONST_CAST(void*, mResourceFactories.Find(uri)));

        PR_ASSERT(factory != nsnull);
        if (! factory)
            return NS_ERROR_FAILURE; // XXX

        nsresult rv;

        if (NS_FAILED(rv = factory->CreateResource(uri, &result)))
            return rv;

        const char* uri;
        result->GetValue(&uri);

        // This is a little trick to make storage more efficient. For
        // the "key" in the table, we'll use the string value that's
        // stored as a member variable of the nsIRDFResource object.
        PL_HashTableAdd(mResources, uri, result);

        // *We* don't AddRef() the resource: that way, the resource
        // can be garbage collected when the last refcount goes away.
    }

    result->AddRef();
    *resource = result;

    return NS_OK;
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
ServiceImpl::UnCacheResource(nsIRDFResource* resource)
{
    nsresult rv;

    const char* uri;
    if (NS_FAILED(rv = resource->GetValue(&uri)))
        return rv;

    PL_HashTableRemove(mResources, uri);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::RegisterResourceFactory(const char* aURIPrefix, nsIRDFResourceFactory* aFactory)
{
    if (! mResourceFactories.Add(aURIPrefix, aFactory))
        return NS_ERROR_ILLEGAL_VALUE;

    NS_ADDREF(aFactory); // XXX should we addref?
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::UnRegisterResourceFactory(const char* aURIPrefix)
{
    nsIRDFResourceFactory* factory =
        NS_STATIC_CAST(nsIRDFResourceFactory*,
                       NS_CONST_CAST(void*, mResourceFactories.Remove(aURIPrefix)));

    if (! factory)
        return NS_ERROR_ILLEGAL_VALUE;

    NS_RELEASE(factory); // XXX should we addref?
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::RegisterNamedDataSource(const char* uri, nsIRDFDataSource* dataSource)
{
    // XXX check for dups, etc.
    NS_ADDREF(dataSource); // XXX is this the right thing to do?
    PL_HashTableAdd(mNamedDataSources, uri, dataSource);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::UnRegisterNamedDataSource(const char* uri)
{
    nsIRDFDataSource* ds = 
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    if (! ds)
        return NS_ERROR_ILLEGAL_VALUE;

    PL_HashTableRemove(mNamedDataSources, uri);
    NS_RELEASE(ds);
    return NS_OK;
}

NS_IMETHODIMP
ServiceImpl::GetNamedDataSource(const char* uri, nsIRDFDataSource** dataSource)
{
    nsIRDFDataSource* ds =
        NS_STATIC_CAST(nsIRDFDataSource*, PL_HashTableLookup(mNamedDataSources, uri));

    // XXX if it's not a named data source, and it looks like it might be
    // a URL, then try to create a stream data source on the URL.
    if (! ds) {
		size_t len = strlen(uri);
		if ((len > 4) && (strcmp(&uri[len-4], ".rdf") == 0)) {
            extern nsresult NS_NewRDFStreamDataSource(nsIRDFDataSource** result);
            if (NS_OK != NS_NewRDFStreamDataSource(&ds)) {
                return NS_ERROR_ILLEGAL_VALUE;
            } else {
                ds->Init(uri);

                // XXX do we really want to globally register this datasource?
                RegisterNamedDataSource(uri, ds);
            }
        } else return NS_ERROR_ILLEGAL_VALUE;
    }

    NS_ADDREF(ds);
    *dataSource = ds;
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


////////////////////////////////////////////////////////////////////////
//
//   This is Big Hack #1. Depedencies on all builtin resource
//   factories are *here*, in the ResourceFactoryTable.
//

struct ResourceFactoryTable {
    const char* mPrefix;
    nsresult (*mFactoryConstructor)(nsIRDFResourceFactory** result);
};

void
ServiceImpl::RegisterBuiltInResourceFactories(void)
{
    extern nsresult NS_NewRDFDefaultResourceFactory(nsIRDFResourceFactory** result);
    extern nsresult NS_NewRDFMailResourceFactory(nsIRDFResourceFactory** result);
    extern nsresult NS_NewRDFMailAccountResourceFactory(nsIRDFResourceFactory** result);
    extern nsresult NS_NewRDFFileResourceFactory(nsIRDFResourceFactory** result);

    static ResourceFactoryTable gTable[] = {
        "",                NS_NewRDFDefaultResourceFactory,
        "mailaccount:",    NS_NewRDFMailAccountResourceFactory,
        "mailbox:",        NS_NewRDFMailResourceFactory,
#if 0
        "file:",           NS_NewRDFFileResourceFactory,
#endif
        nsnull,     nsnull
    };

    nsresult rv;
    for (ResourceFactoryTable* entry = gTable; entry->mPrefix != nsnull; ++entry) {
        nsIRDFResourceFactory* factory;
    
        if (NS_FAILED(rv = (entry->mFactoryConstructor)(&factory)))
            continue;

        rv = gRDFService->RegisterResourceFactory(entry->mPrefix, factory);
        PR_ASSERT(NS_SUCCEEDED(rv));

        NS_RELEASE(factory);
    }
}

////////////////////////////////////////////////////////////////////////
//
//   This is Big Hack #2. Dependencies on all builtin datasources are
//   *here*, in the DataSourceTable.
//
//   FWIW, I don't particularly like this interface *anyway*, because
//   it requires each built-in data source to be constructed "up
//   front". Not only does it cause the service manager to be
//   re-entered (which may be a problem), but it's wasteful: I think
//   these data sources should be created on demand, and released when
//   you're done with them.
//

struct DataSourceTable {
    const char* mURI;
    nsresult (*mDataSourceConstructor)(nsIRDFDataSource** result);
};

void
ServiceImpl::RegisterBuiltInNamedDataSources(void)
{
    extern nsresult NS_NewRDFBookmarkDataSource(nsIRDFDataSource** result);
    extern nsresult NS_NewRDFHistoryDataSource(nsIRDFDataSource** result);
    extern nsresult NS_NewRDFLocalFileSystemDataSource(nsIRDFDataSource** result);
    extern nsresult NS_NewRDFMailDataSource(nsIRDFDataSource** result);

    static DataSourceTable gTable[] = {
        "rdf:bookmarks",      NS_NewRDFBookmarkDataSource,
        "rdf:mail",           NS_NewRDFMailDataSource, 
#if 0
        "rdf:history",        NS_NewRDFHistoryDataSource,
        "rdf:lfs",            NS_NewRDFLocalFileSystemDataSource,
#endif
        nsnull,               nsnull
    };

    nsresult rv;
    for (DataSourceTable* entry = gTable; entry->mURI != nsnull; ++entry) {
        nsIRDFDataSource* ds;

        if (NS_FAILED(rv = (entry->mDataSourceConstructor)(&ds)))
            continue;

        if (NS_SUCCEEDED(rv = ds->Init(entry->mURI))) {
            rv = gRDFService->RegisterNamedDataSource(entry->mURI, ds);
            PR_ASSERT(NS_SUCCEEDED(rv));
        }

        NS_RELEASE(ds);
    }
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFService(nsIRDFService** mgr)
{
    return ServiceImpl::GetRDFService(mgr);
}

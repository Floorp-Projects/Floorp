/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*



 */

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIURI.h"
#include "plhash.h"

class nsXULPrototypeCache : public nsIXULPrototypeCache
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    NS_IMETHOD Get(nsIURI* aURI, nsIXULPrototypeDocument** _result);
    NS_IMETHOD Put(nsIURI* aURI, nsIXULPrototypeDocument* aDocument);

protected:
    friend NS_IMETHODIMP
    NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULPrototypeCache();
    virtual ~nsXULPrototypeCache();
    static PRIntn ReleaseTableEntry(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure);
    nsresult Init();

    static PLHashNumber Hash(const void* aKey);
    static PRIntn CompareKeys(const void* aKey1, const void* aKey2);

    PLHashTable* mTable;
};



nsXULPrototypeCache::nsXULPrototypeCache()
{
    NS_INIT_REFCNT();
}


nsXULPrototypeCache::~nsXULPrototypeCache()
{
    if (mTable) {
        PL_HashTableEnumerateEntries(mTable, ReleaseTableEntry, nsnull);
        PL_HashTableDestroy(mTable);
    }
}

PRIntn
nsXULPrototypeCache::ReleaseTableEntry(PLHashEntry* aHashEntry, PRIntn aIndex, void* aClosure)
{
    nsIURI* key = NS_REINTERPRET_CAST(nsIURI*, NS_CONST_CAST(void*, aHashEntry->key));
    NS_RELEASE(key);

    nsIXULPrototypeDocument* value = NS_REINTERPRET_CAST(nsIXULPrototypeDocument*, aHashEntry->value);
    NS_RELEASE(value);

    return HT_ENUMERATE_REMOVE;
}


nsresult
nsXULPrototypeCache::Init()
{
    mTable = PL_NewHashTable(16, Hash, CompareKeys, PL_CompareValues, nsnull, nsnull);
    if (! mTable)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsXULPrototypeCache, nsIXULPrototypeCache);


NS_IMETHODIMP
NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(! aOuter, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULPrototypeCache* result = new nsXULPrototypeCache();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}

PLHashNumber
nsXULPrototypeCache::Hash(const void* aKey)
{
    nsIURI* uri = NS_REINTERPRET_CAST(nsIURI*, NS_CONST_CAST(void*, aKey));
    nsXPIDLCString spec;
    uri->GetSpec(getter_Copies(spec));
    return PL_HashString(spec);
}


PRIntn
nsXULPrototypeCache::CompareKeys(const void* aKey1, const void* aKey2)
{
    nsIURI* uri1 = NS_REINTERPRET_CAST(nsIURI*, NS_CONST_CAST(void*, aKey1));
    nsIURI* uri2 = NS_REINTERPRET_CAST(nsIURI*, NS_CONST_CAST(void*, aKey2));
    PRBool eq;
    uri1->Equals(uri2, &eq);
    return eq;
}


NS_IMETHODIMP
nsXULPrototypeCache::Get(nsIURI* aURI, nsIXULPrototypeDocument** _result)
{
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::Put(nsIURI* aURI, nsIXULPrototypeDocument* aDocument)
{
    return NS_OK;
}

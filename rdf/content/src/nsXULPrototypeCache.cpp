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
#include "nsICSSStyleSheet.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIURI.h"
#include "nsHashtable.h"
#include "nsXPIDLString.h"
#include "plstr.h"


class nsXULPrototypeCache : public nsIXULPrototypeCache
{
public:
    // nsISupports
    NS_DECL_ISUPPORTS

    NS_IMETHOD GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result);
    NS_IMETHOD PutPrototype(nsIXULPrototypeDocument* aDocument);

    NS_IMETHOD GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result);
    NS_IMETHOD PutStyleSheet(nsICSSStyleSheet* aStyleSheet);

    NS_IMETHOD GetScript(nsIURI* aURI, nsString& aScript, const char** aVersion);
    NS_IMETHOD PutScript(nsIURI* aURI, const nsString& aScript, const char* aVersion);

    NS_IMETHOD Flush();

protected:
    friend NS_IMETHODIMP
    NS_NewXULPrototypeCache(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    nsXULPrototypeCache();
    virtual ~nsXULPrototypeCache();

    static PRBool
    ReleaseScriptEntryEnumFunc(nsHashKey* aKey, void* aData, void* aClosure);

    nsSupportsHashtable mPrototypeTable;
    nsSupportsHashtable mStyleSheetTable;
    nsHashtable         mScriptTable;


    class nsIURIKey : public nsHashKey {
    protected:
        nsCOMPtr<nsIURI> mKey;
  
    public:
        nsIURIKey(nsIURI* key) : mKey(key) {}
        ~nsIURIKey(void) {}
  
        PRUint32 HashValue(void) const {
            nsXPIDLCString spec;
            mKey->GetSpec(getter_Copies(spec));
            return (PRUint32) PL_HashString(spec);
        }

        PRBool Equals(const nsHashKey *aKey) const {
            PRBool eq;
            mKey->Equals( ((nsIURIKey*) aKey)->mKey, &eq );
            return eq;
        }

        nsHashKey *Clone(void) const {
            return new nsIURIKey(mKey);
        }
    };

    class ScriptEntry {
    public:
        nsString    mScript;
        const char* mVersion;
    };
};



nsXULPrototypeCache::nsXULPrototypeCache()
{
    NS_INIT_REFCNT();
}


nsXULPrototypeCache::~nsXULPrototypeCache()
{
    mScriptTable.Enumerate(ReleaseScriptEntryEnumFunc, nsnull);
}


PRBool
nsXULPrototypeCache::ReleaseScriptEntryEnumFunc(nsHashKey* aKey, void* aData, void* aClosure)
{
    ScriptEntry* entry = NS_REINTERPRET_CAST(ScriptEntry*, aData);
    delete entry;
    return PR_TRUE;
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

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}


//----------------------------------------------------------------------


NS_IMETHODIMP
nsXULPrototypeCache::GetPrototype(nsIURI* aURI, nsIXULPrototypeDocument** _result)
{
    nsIURIKey key(aURI);
    *_result = NS_STATIC_CAST(nsIXULPrototypeDocument*, mPrototypeTable.Get(&key));
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutPrototype(nsIXULPrototypeDocument* aDocument)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = aDocument->GetURI(getter_AddRefs(uri));

    nsIURIKey key(uri);
    mPrototypeTable.Put(&key, aDocument);

    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetStyleSheet(nsIURI* aURI, nsICSSStyleSheet** _result)
{
    nsIURIKey key(aURI);
    *_result = NS_STATIC_CAST(nsICSSStyleSheet*, mStyleSheetTable.Get(&key));
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutStyleSheet(nsICSSStyleSheet* aStyleSheet)
{
    nsresult rv;
    nsCOMPtr<nsIURI> uri;
    rv = aStyleSheet->GetURL(*getter_AddRefs(uri));

    nsIURIKey key(uri);
    mStyleSheetTable.Put(&key, aStyleSheet);

    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::GetScript(nsIURI* aURI, nsString& aScript, const char** aVersion)
{
    nsIURIKey key(aURI);
    const ScriptEntry* entry = NS_REINTERPRET_CAST(const ScriptEntry*, mScriptTable.Get(&key));
    if (entry) {
        aScript   = entry->mScript;
        *aVersion = entry->mVersion;
    }
    else {
        aScript.Truncate();
        *aVersion = nsnull;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::PutScript(nsIURI* aURI, const nsString& aScript, const char* aVersion)
{
    ScriptEntry* newentry = new ScriptEntry();
    if (! newentry)
        return NS_ERROR_OUT_OF_MEMORY;

    newentry->mScript  = aScript;
    newentry->mVersion = aVersion;

    nsIURIKey key(aURI);
    ScriptEntry* oldentry = NS_REINTERPRET_CAST(ScriptEntry*, mScriptTable.Put(&key, newentry));

    delete oldentry;

    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeCache::Flush()
{
    mPrototypeTable.Reset();
    mStyleSheetTable.Reset();
    mScriptTable.Reset(ReleaseScriptEntryEnumFunc, nsnull);
    return NS_OK;
}


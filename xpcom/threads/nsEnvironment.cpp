/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEnvironment.h"
#include "prenv.h"
#include "prprf.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsPromiseFlatString.h"
#include "nsDependentString.h"
#include "nsNativeCharsetUtils.h"

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsEnvironment, nsIEnvironment)

nsresult
nsEnvironment::Create(nsISupports *aOuter, REFNSIID aIID,
                      void **aResult)
{
    nsresult rv;
    *aResult = nsnull;

    if (aOuter != nsnull) {
        return NS_ERROR_NO_AGGREGATION;
    }

    nsEnvironment* obj = new nsEnvironment();
    if (!obj) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = obj->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
      delete obj;
    }
    return rv;
}

nsEnvironment::~nsEnvironment()
{
}

NS_IMETHODIMP
nsEnvironment::Exists(const nsAString& aName, bool *aOutValue)
{
    nsCAutoString nativeName;
    nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString nativeVal;
#if defined(XP_UNIX)
    /* For Unix/Linux platforms we follow the Unix definition:
     * An environment variable exists when |getenv()| returns a non-NULL value.
     * An environment variable does not exist when |getenv()| returns NULL.
     */
    const char *value = PR_GetEnv(nativeName.get());
    *aOutValue = value && *value;
#else
    /* For non-Unix/Linux platforms we have to fall back to a 
     * "portable" definition (which is incorrect for Unix/Linux!!!!)
     * which simply checks whether the string returned by |Get()| is empty
     * or not.
     */
    nsAutoString value;
    Get(aName, value);
    *aOutValue = !value.IsEmpty();
#endif /* XP_UNIX */

    return NS_OK;
}

NS_IMETHODIMP
nsEnvironment::Get(const nsAString& aName, nsAString& aOutValue)
{
    nsCAutoString nativeName;
    nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString nativeVal;
    const char *value = PR_GetEnv(nativeName.get());
    if (value && *value) {
        rv = NS_CopyNativeToUnicode(nsDependentCString(value), aOutValue);
    } else {
        aOutValue.Truncate();
        rv = NS_OK;
    }

    return rv;
}

/* Environment strings must have static duration; We're gonna leak all of this
 * at shutdown: this is by design, caused how Unix/Linux implement environment
 * vars. 
 */

typedef nsBaseHashtableET<nsCharPtrHashKey,char*> EnvEntryType;
typedef nsTHashtable<EnvEntryType> EnvHashType;

static EnvHashType *gEnvHash = nsnull;

static bool
EnsureEnvHash()
{
    if (gEnvHash)
        return true;

    gEnvHash = new EnvHashType;
    if (!gEnvHash)
        return false;

    gEnvHash->Init();
    return true;
}

NS_IMETHODIMP
nsEnvironment::Set(const nsAString& aName, const nsAString& aValue)
{
    nsCAutoString nativeName;
    nsCAutoString nativeVal;

    nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_CopyUnicodeToNative(aValue, nativeVal);
    NS_ENSURE_SUCCESS(rv, rv);

    MutexAutoLock lock(mLock);

    if (!EnsureEnvHash()){
        return NS_ERROR_UNEXPECTED;
    }

    EnvEntryType* entry = gEnvHash->PutEntry(nativeName.get());
    if (!entry) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    char* newData = PR_smprintf("%s=%s",
                                nativeName.get(),
                                nativeVal.get());
    if (!newData) {
        return NS_ERROR_OUT_OF_MEMORY;
    } 
    
    PR_SetEnv(newData);
    if (entry->mData) {
        PR_smprintf_free(entry->mData);
    }
    entry->mData = newData;
    return NS_OK;
}



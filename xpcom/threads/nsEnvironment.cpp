/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla embedding code.
 *
 * The Initial Developers of the Original Code are
 * Benjamin Smedberg <bsmedberg@covad.net> and 
 * Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2003/2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        return PR_TRUE;

    gEnvHash = new EnvHashType;
    if (!gEnvHash)
        return PR_FALSE;

    if(gEnvHash->Init())
        return PR_TRUE;

    delete gEnvHash;
    gEnvHash = nsnull;
    return PR_FALSE;
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



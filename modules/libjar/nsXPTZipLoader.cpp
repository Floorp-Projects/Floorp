/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the XPT zip reader.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *   Alec Flett <alecf@netscape.com>
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


#include "nsXPTZipLoader.h"
#include "nsIZipReader.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"

static const char gCacheContractID[] =
  "@mozilla.org/libjar/zip-reader-cache;1";
static const PRUint32 gCacheSize = 1;

nsXPTZipLoader::nsXPTZipLoader() {
}

NS_IMPL_ISUPPORTS1(nsXPTZipLoader, nsIXPTLoader)

nsresult
nsXPTZipLoader::LoadEntry(nsILocalFile* aFile,
                          const char* aName,
                          nsIInputStream** aResult)
{
    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(aFile));

    if (!zip)
        return NS_OK;

    return zip->GetInputStream(aName, aResult);
}
    
nsresult
nsXPTZipLoader::EnumerateEntries(nsILocalFile* aFile,
                                 nsIXPTLoaderSink* aSink)
{
    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(aFile));

    if (!zip) {
        NS_WARNING("Could not get Zip Reader");
        return NS_OK;
    }

    nsCOMPtr<nsISimpleEnumerator> entries;
    if (NS_FAILED(zip->FindEntries("*.xpt", getter_AddRefs(entries))) ||
        !entries) {
        // no problem, just no .xpt files in this archive
        return NS_OK;
    }

    
    PRBool hasMore;
    while (NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
        int index=0;

        nsCOMPtr<nsISupports> sup;
        if (NS_FAILED(entries->GetNext(getter_AddRefs(sup))) || !sup)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIZipEntry> entry = do_QueryInterface(sup);
        if (!entry)
            return NS_ERROR_UNEXPECTED;

        nsXPIDLCString itemName;
        if (NS_FAILED(entry->GetName(getter_Copies(itemName))))
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIInputStream> stream;
        if (NS_FAILED(zip->GetInputStream(itemName, getter_AddRefs(stream))))
            return NS_ERROR_FAILURE;

        // ignore the result
        aSink->FoundEntry(itemName, index++, stream);
    }

    return NS_OK;
}

nsIZipReader*
nsXPTZipLoader::GetZipReader(nsILocalFile* file)
{
    NS_ASSERTION(file, "bad file");
    
    if(!mCache)
    {
        mCache = do_CreateInstance(gCacheContractID);
        if(!mCache || NS_FAILED(mCache->Init(gCacheSize)))
            return nsnull;
    }

    nsIZipReader* reader = nsnull;

    if(NS_FAILED(mCache->GetZip(file, &reader)))
        return nsnull;

    return reader;
}

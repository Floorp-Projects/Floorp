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


#include "nsManifestZIPLoader.h"
#include "nsJAR.h"
#include "nsString.h"
#include "nsStringEnumerator.h"

nsManifestZIPLoader::nsManifestZIPLoader() {
}

NS_IMPL_ISUPPORTS1(nsManifestZIPLoader, nsIManifestLoader)

nsresult
nsManifestZIPLoader::LoadEntry(nsILocalFile* aFile,
                               const char* aName,
                               nsIInputStream** aResult)
{
    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(aFile));

    if (!zip)
        return NS_OK;

    return zip->GetInputStream(aName, aResult);
}

static void
EnumerateEntriesForPattern(nsIZipReader* zip, const char* pattern,
                           nsIManifestLoaderSink* aSink)
{
    nsCOMPtr<nsIUTF8StringEnumerator> entries;
    if (NS_FAILED(zip->FindEntries(pattern, getter_AddRefs(entries))) ||
        !entries) {
        return;
    }

    PRBool hasMore;
    int index = 0;
    while (NS_SUCCEEDED(entries->HasMore(&hasMore)) && hasMore) {
        nsCAutoString itemName;
        if (NS_FAILED(entries->GetNext(itemName)))
            return;

        nsCOMPtr<nsIInputStream> stream;
        if (NS_FAILED(zip->GetInputStream(itemName.get(), getter_AddRefs(stream))))
            continue;

        // ignore the result
        aSink->FoundEntry(itemName.get(), index++, stream);
    }
}
    
nsresult
nsManifestZIPLoader::EnumerateEntries(nsILocalFile* aFile,
                                      nsIManifestLoaderSink* aSink)
{
    nsCOMPtr<nsIZipReader> zip = dont_AddRef(GetZipReader(aFile));

    if (!zip) {
        NS_WARNING("Could not get Zip Reader");
        return NS_OK;
    }

    EnumerateEntriesForPattern(zip, "components/*.manifest$", aSink);
    EnumerateEntriesForPattern(zip, "chrome/*.manifest$", aSink);

    return NS_OK;
}

already_AddRefed<nsIZipReader>
nsManifestZIPLoader::GetZipReader(nsILocalFile* file)
{
    NS_ASSERTION(file, "bad file");
    
    nsCOMPtr<nsIZipReader> reader = new nsJAR();
    nsresult rv = reader->Open(file);
    if (NS_FAILED(rv))
        return NULL;

    return reader.forget();
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsAboutCacheEntry.h"
#include "nsICacheService.h"

NS_IMPL_ISUPPORTS1(nsAboutCacheEntry, nsIAboutModule)

NS_IMETHODIMP
nsAboutCacheEntry::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIOutputStream> outputStream;
    PRUint32 bytesWritten;
    nsCString buffer;
    nsresult rv;

    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, ULONG_MAX, getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;

    buffer.Assign("<html>\n<head>\n<title>Cache entry information</title>\n</head>\n<body>\n");
    outputStream->Write(buffer, buffer.Length(), &bytesWritten);

    // Add other stuff here...

    buffer.Assign("</body>\n</html>\n");
    outputStream->Write(buffer, buffer.Length(), &bytesWritten);
        
    nsCOMPtr<nsIInputStream> inStr;
    PRUint32 size;

    rv = storageStream->GetLength(&size);
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inStr));
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = NS_NewInputStreamChannel(&channel, aURI, inStr, "text/html", size);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

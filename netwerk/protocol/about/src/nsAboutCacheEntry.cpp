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

#include <limits.h>

#ifdef MOZ_NEW_CACHE

#include "nsAboutCacheEntry.h"
#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIStorageStream.h"
#include "nsNetUtil.h"

//-----------------------------------------------------------------------------
// nsISupports implementation
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsAboutCacheEntry,
                   nsIAboutModule,
                   nsIChannel,
                   nsIRequest,
                   nsICacheListener)

//-----------------------------------------------------------------------------
// nsIAboutModule implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    nsresult rv;

    mStreamChannel = do_CreateInstance(NS_STREAMIOCHANNEL_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mStreamChannel->SetURI(aURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface((nsIAboutModule *) this, result);
}

//-----------------------------------------------------------------------------
// nsICacheListener implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::OnCacheEntryAvailable(nsICacheEntryDescriptor *descriptor,
                                         nsCacheAccessMode accessGranted,
                                         nsresult status)
{
    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIOutputStream> outputStream;
    PRUint32 n;
    nsCString buffer;
    nsresult rv;

    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, PRUint32(-1), getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;

    buffer.Assign("<html>\n<head>\n<title>Cache entry information</title>\n</head>\n<body>\n");
    outputStream->Write(buffer, buffer.Length(), &n);

    if (NS_SUCCEEDED(status))
        rv = WriteCacheEntryDescription(outputStream, descriptor);
    else
        rv = WriteCacheEntryUnavailable(outputStream, status);
    if (NS_FAILED(rv)) return rv;

    buffer.Assign("</body>\n</html>\n");
    outputStream->Write(buffer, buffer.Length(), &n);
        
    nsCOMPtr<nsIInputStream> inStr;
    PRUint32 size;

    rv = storageStream->GetLength(&size);
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inStr));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> uri;
    rv = mStreamChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    rv = uri->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStreamIO> io;
    rv = NS_NewInputStreamIO(getter_AddRefs(io), spec, inStr, "text/html", size);

    nsCOMPtr<nsIStreamIOChannel> chan = do_QueryInterface(mStreamChannel, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = chan->Init(uri, io);
    if (NS_FAILED(rv)) return rv;

    return mStreamChannel->AsyncOpen(mListener, mListenerContext);
}

//-----------------------------------------------------------------------------
// nsIRequest implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::GetName(PRUnichar **result)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetName(result);
}

NS_IMETHODIMP
nsAboutCacheEntry::IsPending(PRBool *result)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->IsPending(result);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetStatus(nsresult *result)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetStatus(result);
}

NS_IMETHODIMP
nsAboutCacheEntry::Cancel(nsresult status)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->Cancel(status);
}

NS_IMETHODIMP
nsAboutCacheEntry::Suspend()
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->Suspend();
}

NS_IMETHODIMP
nsAboutCacheEntry::Resume()
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->Resume();
}

//-----------------------------------------------------------------------------
// nsIChannel implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::GetOriginalURI(nsIURI **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetOriginalURI(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetOriginalURI(nsIURI *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetOriginalURI(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetURI(nsIURI **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetURI(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetURI(nsIURI *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetURI(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetOwner(nsISupports **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetOwner(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetOwner(nsISupports *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetOwner(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetLoadGroup(nsILoadGroup **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetLoadGroup(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetLoadGroup(nsILoadGroup *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetLoadGroup(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetLoadAttributes(PRUint32 *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetLoadAttributes(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetLoadAttributes(PRUint32 value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetLoadAttributes(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetNotificationCallbacks(nsIInterfaceRequestor **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetNotificationCallbacks(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetNotificationCallbacks(nsIInterfaceRequestor *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetNotificationCallbacks(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetSecurityInfo(nsISupports **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetSecurityInfo(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetContentType(char **value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetContentType(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetContentType(const char *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetContentType(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetContentLength(PRInt32 *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetContentLength(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetContentLength(PRInt32 value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetContentLength(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::Open(nsIInputStream **result)
{
    NS_NOTREACHED("nsAboutCacheEntry::Open");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAboutCacheEntry::AsyncOpen(nsIStreamListener *listener, nsISupports *context)
{
    nsresult rv;
    nsCAutoString clientID, key;
    PRBool streamBased = PR_TRUE;

    rv = ParseURI(clientID, streamBased, key);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serv->CreateSession(clientID,
                             nsICache::STORE_ANYWHERE,
                             streamBased,
                             getter_AddRefs(mCacheSession));
    if (NS_FAILED(rv)) return rv;

    mListener = listener;
    mListenerContext = context;

    return mCacheSession->AsyncOpenCacheEntry(key, nsICache::ACCESS_READ, this);
}


//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------

#define TD_RIGHT_ALIGN "<td valign=top align=right>"

#define APPEND_ROW(label, value) \
    PR_BEGIN_MACRO \
    buffer.Append("<tr>" TD_RIGHT_ALIGN "<tt><b>"); \
    buffer.Append(label); \
    buffer.Append(":</b></tt></td>\n<td><pre>"); \
    buffer.Append(value); \
    buffer.Append("</pre></td></tr>\n"); \
    PR_END_MACRO

nsresult
nsAboutCacheEntry::WriteCacheEntryDescription(nsIOutputStream *outputStream,
                                              nsICacheEntryDescriptor *descriptor)
{
    nsresult rv;
    nsCAutoString buffer;
    PRUint32 n;

    nsXPIDLCString str;

    rv = descriptor->GetKey(getter_Copies(str));
    if (NS_FAILED(rv)) return rv;
    
    buffer.Assign("<table>");

    buffer.Append("<tr>" TD_RIGHT_ALIGN "<tt><b>key:</b></tt></td><td>");

    // Test if the key is actually a URI
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), str);
    if (NS_SUCCEEDED(rv)) {
        buffer.Append("<a href=\"");
        buffer.Append(str);
        buffer.Append("\">");
        buffer.Append(str);
        buffer.Append("</a>");
        uri = 0;
    }
    else
        buffer.Append(str);
    buffer.Append("</td></tr>");

    // Since meta data enumeration is not hooked up yet, let's just look for some
    // well known (HTTP) meta data tags.

    str = 0;
    descriptor->GetMetaDataElement("request-time", getter_Copies(str));
    if (str)
        APPEND_ROW("request-time", str);

    str = 0;
    descriptor->GetMetaDataElement("response-time", getter_Copies(str));
    if (str)
        APPEND_ROW("response-time", str);

    str = 0;
    descriptor->GetMetaDataElement("http-headers", getter_Copies(str));
    if (str)
        APPEND_ROW("http-headers", str);

    buffer.Append("</table>");

    outputStream->Write(buffer, buffer.Length(), &n);
    return NS_OK;
}

nsresult
nsAboutCacheEntry::WriteCacheEntryUnavailable(nsIOutputStream *outputStream,
                                              nsresult reason)
{
    PRUint32 n;
    nsCAutoString buffer;
    buffer.Assign("The cache entry you selected is no longer available.");
    outputStream->Write(buffer, buffer.Length(), &n);
    return NS_OK;
}

nsresult
nsAboutCacheEntry::ParseURI(nsCString &clientID, PRBool &streamBased, nsCString &key)
{
    //
    // about:cache-entry?client=[string]&sb=[boolean]&key=[string]
    //
    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = mStreamChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString path;
    rv = uri->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString p(path);

    nsReadingIterator<char> i1, i2, i3, end;
    p.BeginReading(i1);
    p.EndReading(end);

    i2 = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("?client="), i1, i2))
        return NS_ERROR_FAILURE;
    // i2 points to the start of clientID

    i1 = i2;
    i3 = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("&sb="), i1, i3))
        return NS_ERROR_FAILURE;
    // i1 points to the end of clientID
    // i3 points to the start of isStreamBased

    clientID.Assign(Substring(i2, i1));

    i1 = i3;
    i2 = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("&key="), i1, i2))
        return NS_ERROR_FAILURE;
    // i1 points to the end of isStreamBased
    // i2 points to the start of key

    streamBased = FindCharInReadable('1', i3, i1);
    key.Assign(Substring(i2, end));

    return NS_OK;
}

#endif // MOZ_NEW_CACHE

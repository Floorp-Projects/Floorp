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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Alexey Chernyak <alexeyc@bigfoot.com> (XHTML 1.1 conversion)
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

#include <limits.h>

#include "nsAboutCacheEntry.h"
#include "nsICacheService.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIStorageStream.h"
#include "nsNetUtil.h"
#include "prtime.h"
#include "nsEscape.h"

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

    nsCOMPtr<nsIChannel> chan;
    rv = NS_NewInputStreamChannel(getter_AddRefs(chan), aURI, nsnull,
                                  NS_LITERAL_CSTRING("application/xhtml+xml"));
    if (NS_FAILED(rv)) return rv;

    mStreamChannel = do_QueryInterface(chan, &rv);
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

    buffer.Assign("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\"\n"
                  "    \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
                  "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
                  "<head>\n<title>Cache entry information</title>\n"
                  "<style type=\"text/css\">\npre {\n  margin: 0;\n}\n"
                  "td:first-child {\n  text-align: right;\n  vertical-align: top;\n"
                  "  line-height: 0.8em;\n}\n</style>\n</head>\n<body>\n");
    outputStream->Write(buffer.get(), buffer.Length(), &n);

    if (NS_SUCCEEDED(status))
        rv = WriteCacheEntryDescription(outputStream, descriptor);
    else
        rv = WriteCacheEntryUnavailable(outputStream, status);
    if (NS_FAILED(rv)) return rv;

    buffer.Assign("</body>\n</html>\n");
    outputStream->Write(buffer.get(), buffer.Length(), &n);
        
    nsCOMPtr<nsIInputStream> inStr;
    PRUint32 size;

    rv = storageStream->GetLength(&size);
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inStr));
    if (NS_FAILED(rv)) return rv;

    rv = mStreamChannel->SetContentStream(inStr);
    if (NS_FAILED(rv)) return rv;

    return mStreamChannel->AsyncOpen(mListener, mListenerContext);
}

//-----------------------------------------------------------------------------
// nsIRequest implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::GetName(nsACString &result)
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
nsAboutCacheEntry::GetLoadFlags(PRUint32 *value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetLoadFlags(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetLoadFlags(PRUint32 value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetLoadFlags(value);
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
nsAboutCacheEntry::GetContentType(nsACString &value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetContentType(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetContentType(const nsACString &value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetContentType(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::GetContentCharset(nsACString &value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->GetContentCharset(value);
}

NS_IMETHODIMP
nsAboutCacheEntry::SetContentCharset(const nsACString &value)
{
    NS_ENSURE_TRUE(mStreamChannel, NS_ERROR_NOT_INITIALIZED);
    return mStreamChannel->SetContentCharset(value);
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

    rv = serv->CreateSession(clientID.get(),
                             nsICache::STORE_ANYWHERE,
                             streamBased,
                             getter_AddRefs(mCacheSession));
    if (NS_FAILED(rv)) return rv;

    rv = mCacheSession->SetDoomEntriesIfExpired(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    mListener = listener;
    mListenerContext = context;

    return mCacheSession->AsyncOpenCacheEntry(key.get(), nsICache::ACCESS_READ, this);
}


//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------

static PRTime SecondsToPRTime(PRUint32 t_sec)
{
    PRTime t_usec, usec_per_sec;
    LL_I2L(t_usec, t_sec);
    LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
    LL_MUL(t_usec, t_usec, usec_per_sec);
    return t_usec;
}
static void PrintTimeString(char *buf, PRUint32 bufsize, PRUint32 t_sec)
{
    PRExplodedTime et;
    PRTime t_usec = SecondsToPRTime(t_sec);
    PR_ExplodeTime(t_usec, PR_LocalTimeParameters, &et);
    PR_FormatTime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &et);
}

#define APPEND_ROW(label, value) \
    PR_BEGIN_MACRO \
    buffer.Append("<tr><td><tt><b>"); \
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

    buffer.Append("<tr><td><tt><b>key:</b></tt></td><td>");

    // Test if the key is actually a URI
    nsCOMPtr<nsIURI> uri;
    PRBool isJS = PR_FALSE;
    PRBool isData = PR_FALSE;

    rv = NS_NewURI(getter_AddRefs(uri), str);
    // javascript: and data: URLs should not be linkified
    // since clicking them can cause scripts to run - bug 162584
    if (NS_SUCCEEDED(rv)) {
        uri->SchemeIs("javascript", &isJS);
        uri->SchemeIs("data", &isData);
    }
    char* escapedStr = nsEscapeHTML(str);
    if (NS_SUCCEEDED(rv) && !(isJS || isData)) {
        buffer.Append("<a href=\"");
        buffer.Append(escapedStr);
        buffer.Append("\">");
        buffer.Append(escapedStr);
        buffer.Append("</a>");
        uri = 0;
    }
    else
        buffer.Append(escapedStr);
    nsMemory::Free(escapedStr);
    buffer.Append("</td></tr>\n");


    // temp vars for reporting
    char timeBuf[255];
    PRUint32 u = 0;
    PRInt32  i = 0;
    nsCAutoString s;

    // Fetch Count
    s.Truncate();
    descriptor->GetFetchCount(&i);
    s.AppendInt(i);
    APPEND_ROW("fetch count", s);

    // Last Fetched
    descriptor->GetLastFetched(&u);
    if (u) {
        PrintTimeString(timeBuf, sizeof(timeBuf), u);
        APPEND_ROW("last fetched", timeBuf);
    } else {
        APPEND_ROW("last fetched", "No last fetch time");
    }

    // Last Modified
    descriptor->GetLastModified(&u);
    if (u) {
        PrintTimeString(timeBuf, sizeof(timeBuf), u);
        APPEND_ROW("last modified", timeBuf);
    } else {
        APPEND_ROW("last modified", "No last modified time");
    }

    // Expiration Time
    descriptor->GetExpirationTime(&u);
    if (u < 0xFFFFFFFF) {
        PrintTimeString(timeBuf, sizeof(timeBuf), u);
        APPEND_ROW("expires", timeBuf);
    } else {
        APPEND_ROW("expires", "No expiration time");
    }

    // Data Size
    s.Truncate();
    descriptor->GetDataSize(&u);
    s.AppendInt((PRInt32)u);     // XXX nsICacheEntryInfo interfaces should be fixed.
    APPEND_ROW("Data size", s);

    // Storage Policy

    // XXX Stream Based?

    // XXX Cache Device
    // File on disk
    nsCOMPtr<nsIFile> cacheFile;
    rv = descriptor->GetFile(getter_AddRefs(cacheFile));
    if (NS_SUCCEEDED(rv)) {
        nsAutoString filePath;
        cacheFile->GetPath(filePath);
        APPEND_ROW("file on disk", NS_ConvertUCS2toUTF8(filePath));
    }
    else
        APPEND_ROW("file on disk", "none");

    // Security Info
    str.Adopt(0);
    nsCOMPtr<nsISupports> securityInfo;
    descriptor->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (securityInfo) {
        APPEND_ROW("Security", "This is a secure document.");
    } else {
        APPEND_ROW("Security",
                   "This document does not have any security info associated with it.");
    }

    buffer.Append("</table>\n");
    // Meta Data
    // let's just look for some well known (HTTP) meta data tags, for now.
    buffer.Append("<hr />\n<table>");

    // Client ID
    str.Adopt(0);
    descriptor->GetClientID(getter_Copies(str));
    if (str)  APPEND_ROW("Client", str);


    mBuffer = &buffer;  // make it available for VisitMetaDataElement()
    descriptor->VisitMetaData(this);
    mBuffer = nsnull;
    

    buffer.Append("</table>\n");

    outputStream->Write(buffer.get(), buffer.Length(), &n);
    return NS_OK;
}

nsresult
nsAboutCacheEntry::WriteCacheEntryUnavailable(nsIOutputStream *outputStream,
                                              nsresult reason)
{
    PRUint32 n;
    nsCAutoString buffer;
    buffer.Assign("The cache entry you selected is no longer available.");
    outputStream->Write(buffer.get(), buffer.Length(), &n);
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

    nsCAutoString path;
    rv = uri->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    nsACString::const_iterator i1, i2, i3, end;
    path.BeginReading(i1);
    path.EndReading(end);

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


//-----------------------------------------------------------------------------
// nsICacheMetaDataVisitor implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::VisitMetaDataElement(const char * key,
                                        const char * value,
                                        PRBool *     keepGoing)
{
    mBuffer->Append("<tr><td><tt><b>");
    mBuffer->Append(key);
    mBuffer->Append(":</b></tt></td>\n<td><pre>");
    char* escapedValue = nsEscapeHTML(value);
    mBuffer->Append(escapedValue);
    nsMemory::Free(escapedValue);
    mBuffer->Append("</pre></td></tr>\n");

    *keepGoing = PR_TRUE;
    return NS_OK;
}


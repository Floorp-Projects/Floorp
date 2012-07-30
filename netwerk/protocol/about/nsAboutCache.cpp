/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAboutCache.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsISimpleEnumerator.h"
#include "nsXPIDLString.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "prtime.h"
#include "nsEscape.h"

#include "nsICacheService.h"

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


NS_IMPL_ISUPPORTS2(nsAboutCache, nsIAboutModule, nsICacheVisitor)

NS_IMETHODIMP
nsAboutCache::NewChannel(nsIURI *aURI, nsIChannel **result)
{
    NS_ENSURE_ARG_POINTER(aURI);
    nsresult rv;
    PRUint32 bytesWritten;

    *result = nullptr;
    // Get the cache manager service
    nsCOMPtr<nsICacheService> cacheService = 
             do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStorageStream> storageStream;
    nsCOMPtr<nsIOutputStream> outputStream;

    // Init: (block size, maximum length)
    rv = NS_NewStorageStream(256, (PRUint32)-1, getter_AddRefs(storageStream));
    if (NS_FAILED(rv)) return rv;

    rv = storageStream->GetOutputStream(0, getter_AddRefs(outputStream));
    if (NS_FAILED(rv)) return rv;

    mBuffer.AssignLiteral(
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "  <title>Information about the Cache Service</title>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/about.css\" type=\"text/css\"/>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/aboutCache.css\" type=\"text/css\"/>\n"
      "</head>\n"
      "<body class=\"aboutPageWideContainer\">\n"
      "<h1>Information about the Cache Service</h1>\n");

    outputStream->Write(mBuffer.get(), mBuffer.Length(), &bytesWritten);

    rv = ParseURI(aURI, mDeviceID);
    if (NS_FAILED(rv)) return rv;

    mStream = outputStream;

    // nsCacheService::VisitEntries calls nsMemoryCacheDevice::Visit,
    // nsDiskCacheDevice::Visit and nsOfflineCacheDevice::Visit,
    // each of which call
    //   1. VisitDevice (for about:cache),
    //   2. VisitEntry in a loop (for about:cache?device=disk etc.)
    rv = cacheService->VisitEntries(this);
    mBuffer.Truncate();
    if (rv == NS_ERROR_NOT_AVAILABLE) {
        mBuffer.AppendLiteral("<h2>The cache is disabled.</h2>\n");
    }
    else if (NS_FAILED(rv)) {
        return rv;
    }

    if (!mDeviceID.IsEmpty()) {
        mBuffer.AppendLiteral("</table>\n");
    }
    mBuffer.AppendLiteral("</body>\n"
                          "</html>\n");
    outputStream->Write(mBuffer.get(), mBuffer.Length(), &bytesWritten);

    nsCOMPtr<nsIInputStream> inStr;

    rv = storageStream->NewInputStream(0, getter_AddRefs(inStr));
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = NS_NewInputStreamChannel(&channel, aURI, inStr,
                                  NS_LITERAL_CSTRING("text/html"),
                                  NS_LITERAL_CSTRING("utf-8"));
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NS_IMETHODIMP
nsAboutCache::GetURIFlags(nsIURI *aURI, PRUint32 *result)
{
    *result = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsAboutCache::VisitDevice(const char *deviceID,
                          nsICacheDeviceInfo *deviceInfo,
                          bool *visitEntries)
{
    PRUint32 bytesWritten, value, entryCount;
    nsXPIDLCString str;

    *visitEntries = false;

    if (mDeviceID.IsEmpty() || mDeviceID.Equals(deviceID)) {

        // We need mStream for this
        if (!mStream)
          return NS_ERROR_FAILURE;
	
        // Write out the Cache Name
        deviceInfo->GetDescription(getter_Copies(str));

        mBuffer.AssignLiteral("<h2>");
        mBuffer.Append(str);
        mBuffer.AppendLiteral("</h2>\n"
                              "<table id=\"");
        mBuffer.Append(deviceID);
        mBuffer.AppendLiteral("\">\n");

        // Write out cache info
        // Number of entries
        mBuffer.AppendLiteral("  <tr>\n"
                              "    <th>Number of entries:</th>\n"
                              "    <td>");
        entryCount = 0;
        deviceInfo->GetEntryCount(&entryCount);
        mBuffer.AppendInt(entryCount);
        mBuffer.AppendLiteral("</td>\n"
                              "  </tr>\n");

        // Maximum storage size
        mBuffer.AppendLiteral("  <tr>\n"
                              "    <th>Maximum storage size:</th>\n"
                              "    <td>");
        value = 0;
        deviceInfo->GetMaximumSize(&value);
        mBuffer.AppendInt(value/1024);
        mBuffer.AppendLiteral(" KiB</td>\n"
                              "  </tr>\n");

        // Storage in use
        mBuffer.AppendLiteral("  <tr>\n"
                              "    <th>Storage in use:</th>\n"
                              "    <td>");
        value = 0;
        deviceInfo->GetTotalSize(&value);
        mBuffer.AppendInt(value/1024);
        mBuffer.AppendLiteral(" KiB</td>\n"
                              "  </tr>\n");

        deviceInfo->GetUsageReport(getter_Copies(str));
        mBuffer.Append(str);

        if (mDeviceID.IsEmpty()) { // The about:cache case
            if (entryCount != 0) { // Add the "List Cache Entries" link
                mBuffer.AppendLiteral("  <tr>\n"
                                      "    <th><a href=\"about:cache?device=");
                mBuffer.Append(deviceID);
                mBuffer.AppendLiteral("\">List Cache Entries</a></th>\n"
                                      "  </tr>\n");
            }
            mBuffer.AppendLiteral("</table>\n");
        } else { // The about:cache?device=disk etc. case
            mBuffer.AppendLiteral("</table>\n");
            if (entryCount != 0) {
                *visitEntries = true;
                mBuffer.AppendLiteral("<hr/>\n"
                                      "<table id=\"entries\">\n"
                                      "  <colgroup>\n"
                                      "   <col id=\"col-key\">\n"
                                      "   <col id=\"col-dataSize\">\n"
                                      "   <col id=\"col-fetchCount\">\n"
                                      "   <col id=\"col-lastModified\">\n"
                                      "   <col id=\"col-expires\">\n"
                                      "  </colgroup>\n"
                                      "  <thead>\n"
                                      "    <tr>\n"
                                      "      <th>Key</th>\n"
                                      "      <th>Data size</th>\n"
                                      "      <th>Fetch count</th>\n"
                                      "      <th>Last modified</th>\n"
                                      "      <th>Expires</th>\n"
                                      "    </tr>\n"
                                      "  </thead>\n");
            }
        }

        mStream->Write(mBuffer.get(), mBuffer.Length(), &bytesWritten);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAboutCache::VisitEntry(const char *deviceID,
                         nsICacheEntryInfo *entryInfo,
                         bool *visitNext)
{
    // We need mStream for this
    if (!mStream)
      return NS_ERROR_FAILURE;

    nsresult        rv;
    PRUint32        bytesWritten;
    nsCAutoString   key;
    nsXPIDLCString  clientID;
    bool            streamBased;
    
    rv = entryInfo->GetKey(key);
    if (NS_FAILED(rv))  return rv;

    rv = entryInfo->GetClientID(getter_Copies(clientID));
    if (NS_FAILED(rv))  return rv;

    rv = entryInfo->IsStreamBased(&streamBased);
    if (NS_FAILED(rv)) return rv;

    // Generate a about:cache-entry URL for this entry...
    nsCAutoString url;
    url.AssignLiteral("about:cache-entry?client=");
    url += clientID;
    url.AppendLiteral("&amp;sb=");
    url += streamBased ? '1' : '0';
    url.AppendLiteral("&amp;key=");
    char* escapedKey = nsEscapeHTML(key.get());
    url += escapedKey; // key

    // Entry start...
    mBuffer.AssignLiteral("  <tr>\n");

    // URI
    mBuffer.AppendLiteral("    <td><a href=\"");
    mBuffer.Append(url);
    mBuffer.AppendLiteral("\">");
    mBuffer.Append(escapedKey);
    nsMemory::Free(escapedKey);
    mBuffer.AppendLiteral("</a></td>\n");

    // Content length
    PRUint32 length = 0;
    entryInfo->GetDataSize(&length);
    mBuffer.AppendLiteral("    <td>");
    mBuffer.AppendInt(length);
    mBuffer.AppendLiteral(" bytes</td>\n");

    // Number of accesses
    PRInt32 fetchCount = 0;
    entryInfo->GetFetchCount(&fetchCount);
    mBuffer.AppendLiteral("    <td>");
    mBuffer.AppendInt(fetchCount);
    mBuffer.AppendLiteral("</td>\n");

    // vars for reporting time
    char buf[255];
    PRUint32 t;

    // Last modified time
    mBuffer.AppendLiteral("    <td>");
    entryInfo->GetLastModified(&t);
    if (t) {
        PrintTimeString(buf, sizeof(buf), t);
        mBuffer.Append(buf);
    } else
        mBuffer.AppendLiteral("No last modified time");
    mBuffer.AppendLiteral("</td>\n");

    // Expires time
    mBuffer.AppendLiteral("    <td>");
    entryInfo->GetExpirationTime(&t);
    if (t < 0xFFFFFFFF) {
        PrintTimeString(buf, sizeof(buf), t);
        mBuffer.Append(buf);
    } else {
        mBuffer.AppendLiteral("No expiration time");
    }
    mBuffer.AppendLiteral("</td>\n");

    // Entry is done...
    mBuffer.AppendLiteral("  </tr>\n");

    mStream->Write(mBuffer.get(), mBuffer.Length(), &bytesWritten);

    *visitNext = true;
    return NS_OK;
}


nsresult
nsAboutCache::ParseURI(nsIURI * uri, nsCString &deviceID)
{
    //
    // about:cache[?device=string]
    //
    nsresult rv;

    deviceID.Truncate();

    nsCAutoString path;
    rv = uri->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    nsACString::const_iterator start, valueStart, end;
    path.BeginReading(start);
    path.EndReading(end);

    valueStart = end;
    if (!FindInReadable(NS_LITERAL_CSTRING("?device="), start, valueStart))
        return NS_OK;

    deviceID.Assign(Substring(valueStart, end));
    return NS_OK;
}


nsresult
nsAboutCache::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsAboutCache* about = new nsAboutCache();
    if (about == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(about);
    nsresult rv = about->QueryInterface(aIID, aResult);
    NS_RELEASE(about);
    return rv;
}



////////////////////////////////////////////////////////////////////////////////

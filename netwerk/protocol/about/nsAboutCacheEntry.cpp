/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>

#include "nsAboutCacheEntry.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsIStorageStream.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "prprf.h"
#include "nsEscape.h"
#include <algorithm>

#define HEXDUMP_MAX_ROWS 16

static void
HexDump(uint32_t *state, const char *buf, int32_t n, nsCString &result)
{
  char temp[16];

  const unsigned char *p;
  while (n) {
    PR_snprintf(temp, sizeof(temp), "%08x:  ", *state);
    result.Append(temp);
    *state += HEXDUMP_MAX_ROWS;

    p = (const unsigned char *) buf;

    int32_t i, row_max = std::min(HEXDUMP_MAX_ROWS, n);

    // print hex codes:
    for (i = 0; i < row_max; ++i) {
      PR_snprintf(temp, sizeof(temp), "%02x  ", *p++);
      result.Append(temp);
    }
    for (i = row_max; i < HEXDUMP_MAX_ROWS; ++i) {
      result.AppendLiteral("    ");
    }

    // print ASCII glyphs if possible:
    p = (const unsigned char *) buf;
    for (i = 0; i < row_max; ++i, ++p) {
      switch (*p) {
      case '<':
        result.AppendLiteral("&lt;");
        break;
      case '>':
        result.AppendLiteral("&gt;");
        break;
      case '&':
        result.AppendLiteral("&amp;");
        break;
      default:
        if (*p < 0x7F && *p > 0x1F) {
          result.Append(*p);
        } else {
          result.Append('.');
        }
      }
    }

    result.Append('\n');

    buf += row_max;
    n -= row_max;
  }
}

//-----------------------------------------------------------------------------
// nsAboutCacheEntry::nsISupports

NS_IMPL_ISUPPORTS2(nsAboutCacheEntry,
                   nsIAboutModule,
                   nsICacheMetaDataVisitor)

//-----------------------------------------------------------------------------
// nsAboutCacheEntry::nsIAboutModule

NS_IMETHODIMP
nsAboutCacheEntry::NewChannel(nsIURI *uri, nsIChannel **result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsresult rv;

    nsCOMPtr<nsIInputStream> stream;
    rv = GetContentStream(uri, getter_AddRefs(stream));
    if (NS_FAILED(rv)) return rv;

    return NS_NewInputStreamChannel(result, uri, stream,
                                    NS_LITERAL_CSTRING("text/html"),
                                    NS_LITERAL_CSTRING("utf-8"));
}

NS_IMETHODIMP
nsAboutCacheEntry::GetURIFlags(nsIURI *aURI, uint32_t *result)
{
    *result = nsIAboutModule::HIDE_FROM_ABOUTABOUT;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsAboutCacheEntry

nsresult
nsAboutCacheEntry::GetContentStream(nsIURI *uri, nsIInputStream **result)
{
    nsresult rv;

    // Init: (block size, maximum length)
    nsCOMPtr<nsIAsyncInputStream> inputStream;
    rv = NS_NewPipe2(getter_AddRefs(inputStream),
                     getter_AddRefs(mOutputStream),
                     true, false,
                     256, UINT32_MAX);
    if (NS_FAILED(rv)) return rv;

    NS_NAMED_LITERAL_CSTRING(
      buffer,
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "  <title>Cache entry information</title>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/about.css\" type=\"text/css\"/>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/aboutCacheEntry.css\" type=\"text/css\"/>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Cache entry information</h1>\n");
    uint32_t n;
    rv = mOutputStream->Write(buffer.get(), buffer.Length(), &n);
    if (NS_FAILED(rv)) return rv;
    if (n != buffer.Length()) return NS_ERROR_UNEXPECTED;

    rv = OpenCacheEntry(uri);
    if (NS_FAILED(rv)) return rv;

    *result = inputStream.forget().get();
    return NS_OK;
}

nsresult
nsAboutCacheEntry::OpenCacheEntry(nsIURI *uri)
{
    nsresult rv;
    nsAutoCString clientID, key;
    bool streamBased = true;

    rv = ParseURI(uri, clientID, streamBased, key);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheSession> session;
    rv = serv->CreateSession(clientID.get(),
                             nsICache::STORE_ANYWHERE,
                             streamBased,
                             getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    rv = session->SetDoomEntriesIfExpired(false);
    if (NS_FAILED(rv)) return rv;

    return session->AsyncOpenCacheEntry(key, nsICache::ACCESS_READ, this, true);
}


//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------

static PRTime SecondsToPRTime(uint32_t t_sec)
{
    PRTime t_usec, usec_per_sec;
    t_usec = t_sec;
    usec_per_sec = PR_USEC_PER_SEC;
    return t_usec *= usec_per_sec;
}
static void PrintTimeString(char *buf, uint32_t bufsize, uint32_t t_sec)
{
    PRExplodedTime et;
    PRTime t_usec = SecondsToPRTime(t_sec);
    PR_ExplodeTime(t_usec, PR_LocalTimeParameters, &et);
    PR_FormatTime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &et);
}

#define APPEND_ROW(label, value) \
    PR_BEGIN_MACRO \
    buffer.AppendLiteral("  <tr>\n" \
                         "    <th>"); \
    buffer.AppendLiteral(label); \
    buffer.AppendLiteral(":</th>\n" \
                         "    <td>"); \
    buffer.Append(value); \
    buffer.AppendLiteral("</td>\n" \
                         "  </tr>\n"); \
    PR_END_MACRO

nsresult
nsAboutCacheEntry::WriteCacheEntryDescription(nsICacheEntryDescriptor *descriptor)
{
    nsresult rv;
    nsCString buffer;
    uint32_t n;

    nsAutoCString str;

    rv = descriptor->GetKey(str);
    if (NS_FAILED(rv)) return rv;

    buffer.SetCapacity(4096);
    buffer.AssignLiteral("<table>\n"
                         "  <tr>\n"
                         "    <th>key:</th>\n"
                         "    <td id=\"td-key\">");

    // Test if the key is actually a URI
    nsCOMPtr<nsIURI> uri;
    bool isJS = false;
    bool isData = false;

    rv = NS_NewURI(getter_AddRefs(uri), str);
    // javascript: and data: URLs should not be linkified
    // since clicking them can cause scripts to run - bug 162584
    if (NS_SUCCEEDED(rv)) {
        uri->SchemeIs("javascript", &isJS);
        uri->SchemeIs("data", &isData);
    }
    char* escapedStr = nsEscapeHTML(str.get());
    if (NS_SUCCEEDED(rv) && !(isJS || isData)) {
        buffer.AppendLiteral("<a href=\"");
        buffer.Append(escapedStr);
        buffer.AppendLiteral("\">");
        buffer.Append(escapedStr);
        buffer.AppendLiteral("</a>");
        uri = 0;
    }
    else
        buffer.Append(escapedStr);
    nsMemory::Free(escapedStr);
    buffer.AppendLiteral("</td>\n"
                         "  </tr>\n");

    // temp vars for reporting
    char timeBuf[255];
    uint32_t u = 0;
    int32_t  i = 0;
    nsAutoCString s;

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
    uint32_t dataSize;
    descriptor->GetStorageDataSize(&dataSize);
    s.AppendInt((int32_t)dataSize);     // XXX nsICacheEntryInfo interfaces should be fixed.
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
        APPEND_ROW("file on disk", NS_ConvertUTF16toUTF8(filePath));
    }
    else
        APPEND_ROW("file on disk", "none");

    // Security Info
    nsCOMPtr<nsISupports> securityInfo;
    descriptor->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (securityInfo) {
        APPEND_ROW("Security", "This is a secure document.");
    } else {
        APPEND_ROW("Security",
                   "This document does not have any security info associated with it.");
    }

    buffer.AppendLiteral("</table>\n"
                         "<hr/>\n"
                         "<table>\n");
    // Meta Data
    // let's just look for some well known (HTTP) meta data tags, for now.

    // Client ID
    nsXPIDLCString str2;
    descriptor->GetClientID(getter_Copies(str2));
    if (!str2.IsEmpty())  APPEND_ROW("Client", str2);


    mBuffer = &buffer;  // make it available for VisitMetaDataElement().
    // nsCacheEntryDescriptor::VisitMetaData calls
    // nsCacheEntry.h VisitMetaDataElements, which returns
    // nsCacheMetaData::VisitElements, which calls
    // nsAboutCacheEntry::VisitMetaDataElement (below) in a loop.
    descriptor->VisitMetaData(this);
    mBuffer = nullptr;

    buffer.AppendLiteral("</table>\n");
    mOutputStream->Write(buffer.get(), buffer.Length(), &n);

    buffer.Truncate();

    // Provide a hexdump of the data
    if (dataSize) { // don't draw an <hr> if the Data Size is 0.
        nsCOMPtr<nsIInputStream> stream;
        descriptor->OpenInputStream(0, getter_AddRefs(stream));
        if (stream) {
            buffer.AssignLiteral("<hr/>\n"
                                 "<pre>");
            uint32_t hexDumpState = 0;
            char chunk[4096];
            while(NS_SUCCEEDED(stream->Read(chunk, sizeof(chunk), &n)) && 
                  n > 0) {
                HexDump(&hexDumpState, chunk, n, buffer);
                mOutputStream->Write(buffer.get(), buffer.Length(), &n);
                buffer.Truncate();
            }
            buffer.AssignLiteral("</pre>\n");
            mOutputStream->Write(buffer.get(), buffer.Length(), &n);
      }
    }
    return NS_OK;
}

nsresult
nsAboutCacheEntry::WriteCacheEntryUnavailable()
{
    uint32_t n;
    NS_NAMED_LITERAL_CSTRING(buffer,
        "The cache entry you selected is not available.");
    mOutputStream->Write(buffer.get(), buffer.Length(), &n);
    return NS_OK;
}

nsresult
nsAboutCacheEntry::ParseURI(nsIURI *uri, nsCString &clientID,
                            bool &streamBased, nsCString &key)
{
    //
    // about:cache-entry?client=[string]&sb=[boolean]&key=[string]
    //
    nsresult rv;

    nsAutoCString path;
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
                                        bool *     keepGoing)
{
    mBuffer->AppendLiteral("  <tr>\n"
                           "    <th>");
    mBuffer->Append(key);
    mBuffer->AppendLiteral(":</th>\n"
                           "    <td>");
    char* escapedValue = nsEscapeHTML(value);
    mBuffer->Append(escapedValue);
    nsMemory::Free(escapedValue);
    mBuffer->AppendLiteral("</td>\n"
                           "  </tr>\n");

    *keepGoing = true;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsICacheListener implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                         nsCacheAccessMode access,
                                         nsresult status)
{
    nsresult rv;

    if (entry)
        rv = WriteCacheEntryDescription(entry);
    else
        rv = WriteCacheEntryUnavailable();
    if (NS_FAILED(rv)) return rv;

    uint32_t n;
    NS_NAMED_LITERAL_CSTRING(buffer, "</body>\n</html>\n");
    mOutputStream->Write(buffer.get(), buffer.Length(), &n);
    mOutputStream->Close();
    mOutputStream = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
nsAboutCacheEntry::OnCacheEntryDoomed(nsresult status)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

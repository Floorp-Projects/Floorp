/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAboutCacheEntry.h"

#include "mozilla/Sprintf.h"

#include "nsAboutCache.h"
#include "nsICacheStorage.h"
#include "CacheObserver.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"
#include "nsAboutProtocolUtils.h"
#include "nsContentUtils.h"
#include "nsInputStreamPump.h"
#include "CacheFileUtils.h"
#include <algorithm>

using namespace mozilla::net;

#define HEXDUMP_MAX_ROWS 16

static void HexDump(uint32_t* state, const char* buf, int32_t n,
                    nsCString& result) {
  char temp[16];

  const unsigned char* p;
  while (n) {
    SprintfLiteral(temp, "%08x:  ", *state);
    result.Append(temp);
    *state += HEXDUMP_MAX_ROWS;

    p = (const unsigned char*)buf;

    int32_t i, row_max = std::min(HEXDUMP_MAX_ROWS, n);

    // print hex codes:
    for (i = 0; i < row_max; ++i) {
      SprintfLiteral(temp, "%02x  ", *p++);
      result.Append(temp);
    }
    for (i = row_max; i < HEXDUMP_MAX_ROWS; ++i) {
      result.AppendLiteral("    ");
    }

    // print ASCII glyphs if possible:
    p = (const unsigned char*)buf;
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

NS_IMPL_ISUPPORTS(nsAboutCacheEntry, nsIAboutModule)
NS_IMPL_ISUPPORTS(nsAboutCacheEntry::Channel, nsICacheEntryOpenCallback,
                  nsICacheEntryMetaDataVisitor, nsIStreamListener, nsIRequest,
                  nsIChannel)

//-----------------------------------------------------------------------------
// nsAboutCacheEntry::nsIAboutModule

NS_IMETHODIMP
nsAboutCacheEntry::NewChannel(nsIURI* uri, nsILoadInfo* aLoadInfo,
                              nsIChannel** result) {
  NS_ENSURE_ARG_POINTER(uri);
  nsresult rv;

  RefPtr<Channel> channel = new Channel();
  rv = channel->Init(uri, aLoadInfo);
  if (NS_FAILED(rv)) return rv;

  channel.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsAboutCacheEntry::GetURIFlags(nsIURI* aURI, uint32_t* result) {
  *result = nsIAboutModule::HIDE_FROM_ABOUTABOUT |
            nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT;
  return NS_OK;
}

NS_IMETHODIMP
nsAboutCacheEntry::GetChromeURI(nsIURI* aURI, nsIURI** chromeURI) {
  return NS_ERROR_ILLEGAL_VALUE;
}

//-----------------------------------------------------------------------------
// nsAboutCacheEntry::Channel

nsresult nsAboutCacheEntry::Channel::Init(nsIURI* uri, nsILoadInfo* aLoadInfo) {
  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = GetContentStream(uri, getter_AddRefs(stream));
  if (NS_FAILED(rv)) return rv;

  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(mChannel), uri,
                                        stream.forget(), "text/html"_ns,
                                        "utf-8"_ns, aLoadInfo);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult nsAboutCacheEntry::Channel::GetContentStream(nsIURI* uri,
                                                      nsIInputStream** result) {
  nsresult rv;

  // Init: (block size, maximum length)
  nsCOMPtr<nsIAsyncInputStream> inputStream;
  rv = NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(mOutputStream),
                   true, false, 256, UINT32_MAX);
  if (NS_FAILED(rv)) return rv;

  constexpr auto buffer =
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "  <meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
      "chrome:; object-src 'none'\" />\n"
      "  <title>Cache entry information</title>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/in-content/info-pages.css\" "
      "type=\"text/css\"/>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/aboutCacheEntry.css\" type=\"text/css\"/>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Cache entry information</h1>\n"_ns;
  uint32_t n;
  rv = mOutputStream->Write(buffer.get(), buffer.Length(), &n);
  if (NS_FAILED(rv)) return rv;
  if (n != buffer.Length()) return NS_ERROR_UNEXPECTED;

  rv = OpenCacheEntry(uri);
  if (NS_FAILED(rv)) return rv;

  inputStream.forget(result);
  return NS_OK;
}

nsresult nsAboutCacheEntry::Channel::OpenCacheEntry(nsIURI* uri) {
  nsresult rv;

  rv = ParseURI(uri, mStorageName, getter_AddRefs(mLoadInfo), mEnhanceId,
                getter_AddRefs(mCacheURI));
  if (NS_FAILED(rv)) return rv;

  return OpenCacheEntry();
}

nsresult nsAboutCacheEntry::Channel::OpenCacheEntry() {
  nsresult rv;

  nsCOMPtr<nsICacheStorage> storage;
  rv = nsAboutCache::GetStorage(mStorageName, mLoadInfo,
                                getter_AddRefs(storage));
  if (NS_FAILED(rv)) return rv;

  // Invokes OnCacheEntryAvailable()
  rv = storage->AsyncOpenURI(
      mCacheURI, mEnhanceId,
      nsICacheStorage::OPEN_READONLY | nsICacheStorage::OPEN_SECRETLY, this);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

nsresult nsAboutCacheEntry::Channel::ParseURI(nsIURI* uri,
                                              nsACString& storageName,
                                              nsILoadContextInfo** loadInfo,
                                              nsCString& enahnceID,
                                              nsIURI** cacheUri) {
  //
  // about:cache-entry?storage=[string]&contenxt=[string]&eid=[string]&uri=[string]
  //
  nsresult rv;

  nsAutoCString path;
  rv = uri->GetPathQueryRef(path);
  if (NS_FAILED(rv)) return rv;

  nsACString::const_iterator keyBegin, keyEnd, valBegin, begin, end;
  path.BeginReading(begin);
  path.EndReading(end);

  keyBegin = begin;
  keyEnd = end;
  if (!FindInReadable("?storage="_ns, keyBegin, keyEnd)) {
    return NS_ERROR_FAILURE;
  }

  valBegin = keyEnd;  // the value of the storage key starts after the key

  keyBegin = keyEnd;
  keyEnd = end;
  if (!FindInReadable("&context="_ns, keyBegin, keyEnd)) {
    return NS_ERROR_FAILURE;
  }

  storageName.Assign(Substring(valBegin, keyBegin));
  valBegin = keyEnd;  // the value of the context key starts after the key

  keyBegin = keyEnd;
  keyEnd = end;
  if (!FindInReadable("&eid="_ns, keyBegin, keyEnd)) return NS_ERROR_FAILURE;

  nsAutoCString contextKey(Substring(valBegin, keyBegin));
  valBegin = keyEnd;  // the value of the eid key starts after the key

  keyBegin = keyEnd;
  keyEnd = end;
  if (!FindInReadable("&uri="_ns, keyBegin, keyEnd)) return NS_ERROR_FAILURE;

  enahnceID.Assign(Substring(valBegin, keyBegin));

  valBegin = keyEnd;  // the value of the uri key starts after the key
  nsAutoCString uriSpec(Substring(valBegin, end));  // uri is the last one

  // Uf... parsing done, now get some objects from it...

  nsCOMPtr<nsILoadContextInfo> info = CacheFileUtils::ParseKey(contextKey);
  if (!info) return NS_ERROR_FAILURE;
  info.forget(loadInfo);

  rv = NS_NewURI(cacheUri, uriSpec);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsICacheEntryOpenCallback implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnCacheEntryCheck(nsICacheEntry* aEntry,
                                              uint32_t* result) {
  *result = nsICacheEntryOpenCallback::ENTRY_WANTED;
  return NS_OK;
}

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnCacheEntryAvailable(nsICacheEntry* entry,
                                                  bool isNew, nsresult status) {
  nsresult rv;

  mWaitingForData = false;
  if (entry) {
    rv = WriteCacheEntryDescription(entry);
  } else {
    rv = WriteCacheEntryUnavailable();
  }
  if (NS_FAILED(rv)) return rv;

  if (!mWaitingForData) {
    // Data is not expected, close the output of content now.
    CloseContent();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// Print-out helper methods
//-----------------------------------------------------------------------------

#define APPEND_ROW(label, value) \
  PR_BEGIN_MACRO                 \
  buffer.AppendLiteral(          \
      "  <tr>\n"                 \
      "    <th>");               \
  buffer.AppendLiteral(label);   \
  buffer.AppendLiteral(          \
      ":</th>\n"                 \
      "    <td>");               \
  buffer.Append(value);          \
  buffer.AppendLiteral(          \
      "</td>\n"                  \
      "  </tr>\n");              \
  PR_END_MACRO

nsresult nsAboutCacheEntry::Channel::WriteCacheEntryDescription(
    nsICacheEntry* entry) {
  nsresult rv;
  // This method appears to run in a situation where the run-time stack
  // should have plenty of space, so allocating a large string on the
  // stack is OK.
  nsAutoCStringN<4097> buffer;
  uint32_t n;

  nsAutoCString str;

  rv = entry->GetKey(str);
  if (NS_FAILED(rv)) return rv;

  buffer.AssignLiteral(
      "<table>\n"
      "  <tr>\n"
      "    <th>key:</th>\n"
      "    <td id=\"td-key\">");

  // Test if the key is actually a URI
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), str);

  nsAutoCString escapedStr;
  nsAppendEscapedHTML(str, escapedStr);

  // javascript: and data: URLs should not be linkified
  // since clicking them can cause scripts to run - bug 162584
  if (NS_SUCCEEDED(rv) &&
      !(uri->SchemeIs("javascript") || uri->SchemeIs("data"))) {
    buffer.AppendLiteral("<a href=\"");
    buffer.Append(escapedStr);
    buffer.AppendLiteral("\">");
    buffer.Append(escapedStr);
    buffer.AppendLiteral("</a>");
    uri = nullptr;
  } else {
    buffer.Append(escapedStr);
  }
  buffer.AppendLiteral(
      "</td>\n"
      "  </tr>\n");

  // temp vars for reporting
  char timeBuf[255];
  uint32_t u = 0;
  int32_t i = 0;
  nsAutoCString s;

  // Fetch Count
  s.Truncate();
  entry->GetFetchCount(&i);
  s.AppendInt(i);
  APPEND_ROW("fetch count", s);

  // Last Fetched
  entry->GetLastFetched(&u);
  if (u) {
    PrintTimeString(timeBuf, sizeof(timeBuf), u);
    APPEND_ROW("last fetched", timeBuf);
  } else {
    APPEND_ROW("last fetched", "No last fetch time (bug 1000338)");
  }

  // Last Modified
  entry->GetLastModified(&u);
  if (u) {
    PrintTimeString(timeBuf, sizeof(timeBuf), u);
    APPEND_ROW("last modified", timeBuf);
  } else {
    APPEND_ROW("last modified", "No last modified time (bug 1000338)");
  }

  // Expiration Time
  entry->GetExpirationTime(&u);

  // Bug - 633747.
  // When expiration time is 0, we show 1970-01-01 01:00:00 which is confusing.
  // So we check if time is 0, then we show a message, "Expired Immediately"
  if (u == 0) {
    APPEND_ROW("expires", "Expired Immediately");
  } else if (u < 0xFFFFFFFF) {
    PrintTimeString(timeBuf, sizeof(timeBuf), u);
    APPEND_ROW("expires", timeBuf);
  } else {
    APPEND_ROW("expires", "No expiration time");
  }

  // Data Size
  s.Truncate();
  uint32_t dataSize;
  if (NS_FAILED(entry->GetStorageDataSize(&dataSize))) dataSize = 0;
  s.AppendInt(
      (int32_t)dataSize);  // XXX nsICacheEntryInfo interfaces should be fixed.
  s.AppendLiteral(" B");
  APPEND_ROW("Data size", s);

  // TODO - mayhemer
  // Here used to be a link to the disk file (in the old cache for entries that
  // did not fit any of the block files, in the new cache every time).
  // I'd rather have a small set of buttons here to action on the entry:
  // 1. save the content
  // 2. save as a complete HTTP response (response head, headers, content)
  // 3. doom the entry
  // A new bug(s) should be filed here.

  // Security Info
  nsCOMPtr<nsISupports> securityInfo;
  entry->GetSecurityInfo(getter_AddRefs(securityInfo));
  if (securityInfo) {
    APPEND_ROW("Security", "This is a secure document.");
  } else {
    APPEND_ROW(
        "Security",
        "This document does not have any security info associated with it.");
  }

  buffer.AppendLiteral(
      "</table>\n"
      "<hr/>\n"
      "<table>\n");

  mBuffer = &buffer;  // make it available for OnMetaDataElement().
  entry->VisitMetaData(this);
  mBuffer = nullptr;

  buffer.AppendLiteral("</table>\n");
  mOutputStream->Write(buffer.get(), buffer.Length(), &n);
  buffer.Truncate();

  // Provide a hexdump of the data
  if (!dataSize) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> stream;
  entry->OpenInputStream(0, getter_AddRefs(stream));
  if (!stream) {
    return NS_OK;
  }

  RefPtr<nsInputStreamPump> pump;
  rv = nsInputStreamPump::Create(getter_AddRefs(pump), stream);
  if (NS_FAILED(rv)) {
    return NS_OK;  // just ignore
  }

  rv = pump->AsyncRead(this);
  if (NS_FAILED(rv)) {
    return NS_OK;  // just ignore
  }

  mWaitingForData = true;
  return NS_OK;
}

nsresult nsAboutCacheEntry::Channel::WriteCacheEntryUnavailable() {
  uint32_t n;
  constexpr auto buffer = "The cache entry you selected is not available."_ns;
  mOutputStream->Write(buffer.get(), buffer.Length(), &n);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsICacheEntryMetaDataVisitor implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnMetaDataElement(char const* key,
                                              char const* value) {
  mBuffer->AppendLiteral(
      "  <tr>\n"
      "    <th>");
  mBuffer->Append(key);
  mBuffer->AppendLiteral(
      ":</th>\n"
      "    <td>");
  nsAppendEscapedHTML(nsDependentCString(value), *mBuffer);
  mBuffer->AppendLiteral(
      "</td>\n"
      "  </tr>\n");

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIStreamListener implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnStartRequest(nsIRequest* request) {
  mHexDumpState = 0;

  constexpr auto buffer = "<hr/>\n<pre>"_ns;
  uint32_t n;
  return mOutputStream->Write(buffer.get(), buffer.Length(), &n);
}

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnDataAvailable(nsIRequest* request,
                                            nsIInputStream* aInputStream,
                                            uint64_t aOffset, uint32_t aCount) {
  uint32_t n;
  return aInputStream->ReadSegments(&nsAboutCacheEntry::Channel::PrintCacheData,
                                    this, aCount, &n);
}

/* static */
nsresult nsAboutCacheEntry::Channel::PrintCacheData(
    nsIInputStream* aInStream, void* aClosure, const char* aFromSegment,
    uint32_t aToOffset, uint32_t aCount, uint32_t* aWriteCount) {
  nsAboutCacheEntry::Channel* a =
      static_cast<nsAboutCacheEntry::Channel*>(aClosure);

  nsCString buffer;
  HexDump(&a->mHexDumpState, aFromSegment, aCount, buffer);

  uint32_t n;
  a->mOutputStream->Write(buffer.get(), buffer.Length(), &n);

  *aWriteCount = aCount;

  return NS_OK;
}

NS_IMETHODIMP
nsAboutCacheEntry::Channel::OnStopRequest(nsIRequest* request,
                                          nsresult result) {
  constexpr auto buffer = "</pre>\n"_ns;
  uint32_t n;
  mOutputStream->Write(buffer.get(), buffer.Length(), &n);

  CloseContent();

  return NS_OK;
}

void nsAboutCacheEntry::Channel::CloseContent() {
  constexpr auto buffer = "</body>\n</html>\n"_ns;
  uint32_t n;
  mOutputStream->Write(buffer.get(), buffer.Length(), &n);

  mOutputStream->Close();
  mOutputStream = nullptr;
}

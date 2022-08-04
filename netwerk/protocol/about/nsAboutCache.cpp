/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAboutCache.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsIPipe.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsAboutProtocolUtils.h"
#include "nsPrintfCString.h"

#include "nsICacheStorageService.h"
#include "nsICacheStorage.h"
#include "CacheFileUtils.h"
#include "CacheObserver.h"

#include "nsThreadUtils.h"

using namespace mozilla::net;

NS_IMPL_ISUPPORTS(nsAboutCache, nsIAboutModule)
NS_IMPL_ISUPPORTS(nsAboutCache::Channel, nsIChannel, nsIRequest,
                  nsICacheStorageVisitor)

NS_IMETHODIMP
nsAboutCache::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                         nsIChannel** result) {
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aURI);

  RefPtr<Channel> channel = new Channel();
  rv = channel->Init(aURI, aLoadInfo);
  if (NS_FAILED(rv)) return rv;

  channel.forget(result);

  return NS_OK;
}

nsresult nsAboutCache::Channel::Init(nsIURI* aURI, nsILoadInfo* aLoadInfo) {
  nsresult rv;

  mCancel = false;

  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewPipe(getter_AddRefs(inputStream), getter_AddRefs(mStream), 16384,
                  (uint32_t)-1,
                  true,  // non-blocking input
                  false  // blocking output
  );
  if (NS_FAILED(rv)) return rv;

  nsAutoCString storageName;
  rv = ParseURI(aURI, storageName);
  if (NS_FAILED(rv)) return rv;

  mOverview = storageName.IsEmpty();
  if (mOverview) {
    // ...and visit all we can
    mStorageList.AppendElement("memory"_ns);
    mStorageList.AppendElement("disk"_ns);
  } else {
    // ...and visit just the specified storage, entries will output too
    mStorageList.AppendElement(storageName);
  }

  // The entries header is added on encounter of the first entry
  mEntriesHeaderAdded = false;

  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(mChannel), aURI,
                                        inputStream.forget(), "text/html"_ns,
                                        "utf-8"_ns, aLoadInfo);
  if (NS_FAILED(rv)) return rv;

  mBuffer.AssignLiteral(
      "<!DOCTYPE html>\n"
      "<html>\n"
      "<head>\n"
      "  <title>Network Cache Storage Information</title>\n"
      "  <meta charset=\"utf-8\">\n"
      "  <meta name=\"color-scheme\" content=\"light dark\">\n"
      "  <meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
      "chrome:; object-src 'none'\"/>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/in-content/info-pages.css\"/>\n"
      "  <link rel=\"stylesheet\" "
      "href=\"chrome://global/skin/aboutCache.css\"/>\n"
      "</head>\n"
      "<body class=\"aboutPageWideContainer\">\n"
      "<h1>Information about the Network Cache Storage Service</h1>\n");

  if (!mOverview) {
    mBuffer.AppendLiteral(
        "<a href=\"about:cache?storage=\">Back to overview</a>");
  }

  rv = FlushBuffer();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to flush buffer");
  }

  return NS_OK;
}

NS_IMETHODIMP nsAboutCache::Channel::AsyncOpen(nsIStreamListener* aListener) {
  nsresult rv;

  if (!mChannel) {
    return NS_ERROR_UNEXPECTED;
  }

  // Kick the walk loop.
  rv = VisitNextStorage();
  if (NS_FAILED(rv)) return rv;

  rv = NS_MaybeOpenChannelUsingAsyncOpen(mChannel, aListener);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

NS_IMETHODIMP nsAboutCache::Channel::Open(nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsAboutCache::Channel::ParseURI(nsIURI* uri, nsACString& storage) {
  //
  // about:cache[?storage=<storage-name>[&context=<context-key>]]
  //
  nsresult rv;

  nsAutoCString path;
  rv = uri->GetPathQueryRef(path);
  if (NS_FAILED(rv)) return rv;

  storage.Truncate();

  nsACString::const_iterator start, valueStart, end;
  path.BeginReading(start);
  path.EndReading(end);

  valueStart = end;
  if (!FindInReadable("?storage="_ns, start, valueStart)) {
    return NS_OK;
  }

  storage.Assign(Substring(valueStart, end));

  return NS_OK;
}

nsresult nsAboutCache::Channel::VisitNextStorage() {
  if (!mStorageList.Length()) return NS_ERROR_NOT_AVAILABLE;

  mStorageName = mStorageList[0];
  mStorageList.RemoveElementAt(0);

  // Must re-dispatch since we cannot start another visit cycle
  // from visitor callback.  The cache v1 service doesn't like it.
  // TODO - mayhemer, bug 913828, remove this dispatch and call
  // directly.
  return NS_DispatchToMainThread(mozilla::NewRunnableMethod(
      "nsAboutCache::Channel::FireVisitStorage", this,
      &nsAboutCache::Channel::FireVisitStorage));
}

void nsAboutCache::Channel::FireVisitStorage() {
  nsresult rv;

  rv = VisitStorage(mStorageName);
  if (NS_FAILED(rv)) {
    nsAutoCString escaped;
    nsAppendEscapedHTML(mStorageName, escaped);
    mBuffer.Append(nsPrintfCString(
        "<p>Unrecognized storage name '%s' in about:cache URL</p>",
        escaped.get()));

    rv = FlushBuffer();
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to flush buffer");
    }

    // Simulate finish of a visit cycle, this tries the next storage
    // or closes the output stream (i.e. the UI loader will stop spinning)
    OnCacheEntryVisitCompleted();
  }
}

nsresult nsAboutCache::Channel::VisitStorage(nsACString const& storageName) {
  nsresult rv;

  rv = GetStorage(storageName, nullptr, getter_AddRefs(mStorage));
  if (NS_FAILED(rv)) return rv;

  rv = mStorage->AsyncVisitStorage(this, !mOverview);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

// static
nsresult nsAboutCache::GetStorage(nsACString const& storageName,
                                  nsILoadContextInfo* loadInfo,
                                  nsICacheStorage** storage) {
  nsresult rv;

  nsCOMPtr<nsICacheStorageService> cacheService =
      do_GetService("@mozilla.org/netwerk/cache-storage-service;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsICacheStorage> cacheStorage;
  if (storageName == "disk") {
    rv = cacheService->DiskCacheStorage(loadInfo, getter_AddRefs(cacheStorage));
  } else if (storageName == "memory") {
    rv = cacheService->MemoryCacheStorage(loadInfo,
                                          getter_AddRefs(cacheStorage));
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }
  if (NS_FAILED(rv)) return rv;

  cacheStorage.forget(storage);
  return NS_OK;
}

NS_IMETHODIMP
nsAboutCache::Channel::OnCacheStorageInfo(uint32_t aEntryCount,
                                          uint64_t aConsumption,
                                          uint64_t aCapacity,
                                          nsIFile* aDirectory) {
  // We need mStream for this
  if (!mStream) {
    return NS_ERROR_FAILURE;
  }

  mBuffer.AssignLiteral("<h2>");
  nsAppendEscapedHTML(mStorageName, mBuffer);
  mBuffer.AppendLiteral(
      "</h2>\n"
      "<table id=\"");
  mBuffer.AppendLiteral("\">\n");

  // Write out cache info
  // Number of entries
  mBuffer.AppendLiteral(
      "  <tr>\n"
      "    <th>Number of entries:</th>\n"
      "    <td>");
  mBuffer.AppendInt(aEntryCount);
  mBuffer.AppendLiteral(
      "</td>\n"
      "  </tr>\n");

  // Maximum storage size
  mBuffer.AppendLiteral(
      "  <tr>\n"
      "    <th>Maximum storage size:</th>\n"
      "    <td>");
  mBuffer.AppendInt(aCapacity / 1024);
  mBuffer.AppendLiteral(
      " KiB</td>\n"
      "  </tr>\n");

  // Storage in use
  mBuffer.AppendLiteral(
      "  <tr>\n"
      "    <th>Storage in use:</th>\n"
      "    <td>");
  mBuffer.AppendInt(aConsumption / 1024);
  mBuffer.AppendLiteral(
      " KiB</td>\n"
      "  </tr>\n");

  // Storage disk location
  mBuffer.AppendLiteral(
      "  <tr>\n"
      "    <th>Storage disk location:</th>\n"
      "    <td>");
  if (aDirectory) {
    nsAutoString path;
    aDirectory->GetPath(path);
    mBuffer.Append(NS_ConvertUTF16toUTF8(path));
  } else {
    mBuffer.AppendLiteral("none, only stored in memory");
  }
  mBuffer.AppendLiteral(
      "    </td>\n"
      "  </tr>\n");

  if (mOverview) {           // The about:cache case
    if (aEntryCount != 0) {  // Add the "List Cache Entries" link
      mBuffer.AppendLiteral(
          "  <tr>\n"
          "    <td colspan=\"2\"><a href=\"about:cache?storage=");
      nsAppendEscapedHTML(mStorageName, mBuffer);
      mBuffer.AppendLiteral(
          "\">List Cache Entries</a></td>\n"
          "  </tr>\n");
    }
  }

  mBuffer.AppendLiteral("</table>\n");

  // The entries header is added on encounter of the first entry
  mEntriesHeaderAdded = false;

  nsresult rv = FlushBuffer();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to flush buffer");
  }

  if (mOverview) {
    // OnCacheEntryVisitCompleted() is not called when we do not iterate
    // cache entries.  Since this moves forward to the next storage in
    // the list we want to visit, artificially call it here.
    OnCacheEntryVisitCompleted();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAboutCache::Channel::OnCacheEntryInfo(
    nsIURI* aURI, const nsACString& aIdEnhance, int64_t aDataSize,
    int64_t aAltDataSize, uint32_t aFetchCount, uint32_t aLastModified,
    uint32_t aExpirationTime, bool aPinned, nsILoadContextInfo* aInfo) {
  // We need mStream for this
  if (!mStream || mCancel) {
    // Returning a failure from this callback stops the iteration
    return NS_ERROR_FAILURE;
  }

  if (!mEntriesHeaderAdded) {
    mBuffer.AppendLiteral(
        "<hr/>\n"
        "<table id=\"entries\">\n"
        "  <colgroup>\n"
        "   <col id=\"col-key\">\n"
        "   <col id=\"col-dataSize\">\n"
        "   <col id=\"col-altDataSize\">\n"
        "   <col id=\"col-fetchCount\">\n"
        "   <col id=\"col-lastModified\">\n"
        "   <col id=\"col-expires\">\n"
        "   <col id=\"col-pinned\">\n"
        "  </colgroup>\n"
        "  <thead>\n"
        "    <tr>\n"
        "      <th>Key</th>\n"
        "      <th>Data size</th>\n"
        "      <th>Alternative Data size</th>\n"
        "      <th>Fetch count</th>\n"
        "      <th>Last Modifed</th>\n"
        "      <th>Expires</th>\n"
        "      <th>Pinning</th>\n"
        "    </tr>\n"
        "  </thead>\n");
    mEntriesHeaderAdded = true;
  }

  // Generate a about:cache-entry URL for this entry...

  nsAutoCString url;
  url.AssignLiteral("about:cache-entry?storage=");
  nsAppendEscapedHTML(mStorageName, url);

  nsAutoCString context;
  CacheFileUtils::AppendKeyPrefix(aInfo, context);
  url.AppendLiteral("&amp;context=");
  nsAppendEscapedHTML(context, url);

  url.AppendLiteral("&amp;eid=");
  nsAppendEscapedHTML(aIdEnhance, url);

  nsAutoCString cacheUriSpec;
  aURI->GetAsciiSpec(cacheUriSpec);
  nsAutoCString escapedCacheURI;
  nsAppendEscapedHTML(cacheUriSpec, escapedCacheURI);
  url.AppendLiteral("&amp;uri=");
  url += escapedCacheURI;

  // Entry start...
  mBuffer.AppendLiteral("  <tr>\n");

  // URI
  mBuffer.AppendLiteral("    <td><a href=\"");
  mBuffer.Append(url);
  mBuffer.AppendLiteral("\">");
  if (!aIdEnhance.IsEmpty()) {
    nsAppendEscapedHTML(aIdEnhance, mBuffer);
    mBuffer.Append(':');
  }
  mBuffer.Append(escapedCacheURI);
  mBuffer.AppendLiteral("</a>");

  if (!context.IsEmpty()) {
    mBuffer.AppendLiteral("<br><span title=\"Context separation key\">");
    nsAutoCString escapedContext;
    nsAppendEscapedHTML(context, escapedContext);
    mBuffer.Append(escapedContext);
    mBuffer.AppendLiteral("</span>");
  }

  mBuffer.AppendLiteral("</td>\n");

  // Content length
  mBuffer.AppendLiteral("    <td>");
  mBuffer.AppendInt(aDataSize);
  mBuffer.AppendLiteral(" bytes</td>\n");

  // Length of alternative content
  mBuffer.AppendLiteral("    <td>");
  mBuffer.AppendInt(aAltDataSize);
  mBuffer.AppendLiteral(" bytes</td>\n");

  // Number of accesses
  mBuffer.AppendLiteral("    <td>");
  mBuffer.AppendInt(aFetchCount);
  mBuffer.AppendLiteral("</td>\n");

  // vars for reporting time
  char buf[255];

  // Last modified time
  mBuffer.AppendLiteral("    <td>");
  if (aLastModified) {
    PrintTimeString(buf, sizeof(buf), aLastModified);
    mBuffer.Append(buf);
  } else {
    mBuffer.AppendLiteral("No last modified time");
  }
  mBuffer.AppendLiteral("</td>\n");

  // Expires time
  mBuffer.AppendLiteral("    <td>");

  // Bug - 633747.
  // When expiration time is 0, we show 1970-01-01 01:00:00 which is confusing.
  // So we check if time is 0, then we show a message, "Expired Immediately"
  if (aExpirationTime == 0) {
    mBuffer.AppendLiteral("Expired Immediately");
  } else if (aExpirationTime < 0xFFFFFFFF) {
    PrintTimeString(buf, sizeof(buf), aExpirationTime);
    mBuffer.Append(buf);
  } else {
    mBuffer.AppendLiteral("No expiration time");
  }
  mBuffer.AppendLiteral("</td>\n");

  // Pinning
  mBuffer.AppendLiteral("    <td>");
  if (aPinned) {
    mBuffer.AppendLiteral("Pinned");
  } else {
    mBuffer.AppendLiteral("&nbsp;");
  }
  mBuffer.AppendLiteral("</td>\n");

  // Entry is done...
  mBuffer.AppendLiteral("  </tr>\n");

  return FlushBuffer();
}

NS_IMETHODIMP
nsAboutCache::Channel::OnCacheEntryVisitCompleted() {
  if (!mStream) {
    return NS_ERROR_FAILURE;
  }

  if (mEntriesHeaderAdded) {
    mBuffer.AppendLiteral("</table>\n");
  }

  // Kick another storage visiting (from a storage that allows us.)
  while (mStorageList.Length()) {
    nsresult rv = VisitNextStorage();
    if (NS_SUCCEEDED(rv)) {
      // Expecting new round of OnCache* calls.
      return NS_OK;
    }
  }

  // We are done!
  mBuffer.AppendLiteral(
      "</body>\n"
      "</html>\n");
  nsresult rv = FlushBuffer();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to flush buffer");
  }
  mStream->Close();

  return NS_OK;
}

nsresult nsAboutCache::Channel::FlushBuffer() {
  nsresult rv;

  uint32_t bytesWritten;
  rv = mStream->Write(mBuffer.get(), mBuffer.Length(), &bytesWritten);
  mBuffer.Truncate();

  if (NS_FAILED(rv)) {
    mCancel = true;
  }

  return rv;
}

NS_IMETHODIMP
nsAboutCache::GetURIFlags(nsIURI* aURI, uint32_t* result) {
  *result = nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT |
            nsIAboutModule::IS_SECURE_CHROME_UI;
  return NS_OK;
}

// static
nsresult nsAboutCache::Create(REFNSIID aIID, void** aResult) {
  RefPtr<nsAboutCache> about = new nsAboutCache();
  return about->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsAboutCache::GetChromeURI(nsIURI* aURI, nsIURI** chromeURI) {
  return NS_ERROR_ILLEGAL_VALUE;
}

////////////////////////////////////////////////////////////////////////////////

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_PackagedAppService_h
#define mozilla_net_PackagedAppService_h

#include "nsIPackagedAppService.h"
#include "nsILoadContextInfo.h"
#include "nsICacheStorage.h"

namespace mozilla {
namespace net {

class nsHttpResponseHead;

// This service is used to download packages from the web.
// Individual resources in the package are saved in the browser cache. It also
// provides an interface to asynchronously request resources from packages,
// which are either returned from the cache if they exist and are valid,
// or downloads the package.
// The package format is defined at:
//     https://w3ctag.github.io/packaging-on-the-web/#streamable-package-format
// Downloading the package is triggered by calling requestURI(aURI, aInfo, aCallback)
//     aURI is the subresource uri - http://domain.com/path/package!//resource.html
//     aInfo is a nsILoadContextInfo used to pick the cache jar the resource goes into
//     aCallback is the target of the async call to requestURI
// When requestURI is called, a CacheEntryChecker is created to verify if the
// resource is already in the cache. If it is, it passes it to the callback.
// Otherwise, it starts downloading the package. When the packaged resource has
// been downloaded, its cache entry gets passed to the callback.
class PackagedAppService final
  : public nsIPackagedAppService
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPACKAGEDAPPSERVICE

  PackagedAppService();

private:
  ~PackagedAppService();

  // This method is called if an entry wasn't found in the cache.
  // It checks to see if the package is currently being downloaded.
  // If so, then it simply adds the callback to that PackageAppDownloader
  // Else it begins downloading the new package and adds it to mDownloadingPackages
  // - aURI is the packaged resource's URL
  // - aCallback is the listener which gets called when the requested
  //   resource is available.
  // - aInfo is needed because cache entries are located in separate cache jars
  //   If a resource isn't found in the package, aCallback->OnCacheEntryAvailable
  //   will be called with a null entry and an error result as a status.
  nsresult OpenNewPackageInternal(nsIURI *aURI,
                                  nsICacheEntryOpenCallback *aCallback,
                                  nsILoadContextInfo *aInfo);

  // Called by PackageAppDownloader once the download has finished
  // (or encountered an error) to remove the package from mDownloadingPackages
  // Should be called on the main thread.
  nsresult NotifyPackageDownloaded(nsCString aKey);

  // This class is used to write data into the cache entry corresponding to the
  // packaged resource being downloaded.
  // The PackagedAppDownloader will hold a ref to a CacheEntryWriter that
  // corresponds to the entry that is currently being downloaded.
  class CacheEntryWriter final
    : public nsIStreamListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    // If successful, calling this static method will create a new
    // CacheEntryWriter and will create the cache entry associated to the
    // resource and open an output stream which we use for writing the resource's
    // content into the cache entry.
    static nsresult Create(nsIURI*, nsICacheStorage*, CacheEntryWriter**);

    nsCOMPtr<nsICacheEntry> mEntry;
  private:
    CacheEntryWriter() { }
    ~CacheEntryWriter() { }

    // Copy the security-info metadata from the channel to the cache entry
    // so packaged resources can be accessed over https.
    nsresult CopySecurityInfo(nsIChannel *aChannel);

    // Copy headers that apply to all resources in the package
    static nsresult CopyHeadersFromChannel(nsIChannel *aChannel,
                                           nsHttpResponseHead *aHead);

    // Static method used to write data into the cache entry
    // Called from OnDataAvailable
    static NS_METHOD ConsumeData(nsIInputStream *in, void *closure,
                                 const char *fromRawSegment, uint32_t toOffset,
                                 uint32_t count, uint32_t *writeCount);
    // We write the data we read from the network into this stream which goes
    // to the cache entry.
    nsCOMPtr<nsIOutputStream> mOutputStream;
  };

  // This class is used to download a packaged app. It acts as a listener
  // for the nsMultiMixedConv object that parses the package.
  // There is an OnStartRequest, OnDataAvailable*, OnStopRequest sequence called
  // for each resource
  // The PackagedAppService holds a hash-table of the PackagedAppDownloaders
  // that are in progress to coalesce same loads.
  // Once the downloading is completed, it should call
  // NotifyPackageDownloaded(packageURI), so the service releases the ref.
  class PackagedAppDownloader final
    : public nsIStreamListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    // Initializes mCacheStorage and saves aKey as mPackageKey which is later
    // used to remove this object from PackagedAppService::mDownloadingPackages
    // - aKey is a string which uniquely identifies this package within the
    //   packagedAppService
    nsresult Init(nsILoadContextInfo* aInfo, const nsCString &aKey);
    // Registers a callback which gets called when the given nsIURI is downloaded
    // aURI is the full URI of a subresource, composed of packageURI + !// + subresourcePath
    nsresult AddCallback(nsIURI *aURI, nsICacheEntryOpenCallback *aCallback);

  private:
    ~PackagedAppDownloader() { }

    // Calls all the callbacks registered for the given URI.
    // aURI is the full URI of a subresource, composed of packageURI + !// + subresourcePath
    // It passes the cache entry and the result when calling OnCacheEntryAvailable
    nsresult CallCallbacks(nsIURI *aURI, nsICacheEntry *aEntry, nsresult aResult);
    // Clears all the callbacks for this package
    // This would get called at the end of downloading the package and would
    // cause us to call OnCacheEntryAvailable with a null entry. This would be
    // equivalent to a 404 when loading from the net.
    nsresult ClearCallbacks(nsresult aResult);
    static PLDHashOperator ClearCallbacksEnumerator(const nsACString& key,
      nsAutoPtr<nsCOMArray<nsICacheEntryOpenCallback>>& callbackArray,
      void* arg);
    // Returns a URI with the subresource's full URI
    // The request must be QIable to nsIResponseHeadProvider since it looks
    // at the Content-Location header to compute the full path.
    static nsresult GetSubresourceURI(nsIRequest * aRequest, nsIURI **aResult);
    // Used to write data into the cache entry of the resource currently being
    // downloaded. It is kept alive until the downloader receives OnStopRequest
    nsRefPtr<CacheEntryWriter> mWriter;
    // Cached value of nsICacheStorage
    nsCOMPtr<nsICacheStorage> mCacheStorage;
    // A hastable containing all the consumers which requested a resource and need
    // to be notified once it is inserted into the cache.
    // The key is a subresource URI - http://example.com/package.pak!//res.html
    // Should only be used on the main thread.
    nsClassHashtable<nsCStringHashKey, nsCOMArray<nsICacheEntryOpenCallback>> mCallbacks;
    // The key with which this package is inserted in
    // PackagedAppService::mDownloadingPackages
    nsCString mPackageKey;
  };

  // This class is used to check if a packaged resource has already been
  // downloaded and saved into the cache.
  // It calls aCallback->OnCacheEntryAvailable if the resource exists in the
  // cache or PackagedAppService::OpenNewPackageInternal if it needs
  // to be downloaded
  class CacheEntryChecker final
    : public nsICacheEntryOpenCallback
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYOPENCALLBACK

    CacheEntryChecker(nsIURI *aURI, nsICacheEntryOpenCallback * aCallback,
                      nsILoadContextInfo *aInfo)
      : mURI(aURI)
      , mCallback(aCallback)
      , mLoadContextInfo(aInfo)
    {
    }
  private:
    ~CacheEntryChecker() { }

    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsICacheEntryOpenCallback> mCallback;
    nsCOMPtr<nsILoadContextInfo> mLoadContextInfo;
  };

  // A hashtable of packages that are currently being downloaded.
  // The key is a string formed by concatenating LoadContextInfo and package URI
  // Should only be used on the main thread.
  nsRefPtrHashtable<nsCStringHashKey, PackagedAppDownloader> mDownloadingPackages;
};


} // namespace net
} // namespace mozilla

#endif // mozilla_net_PackagedAppService_h
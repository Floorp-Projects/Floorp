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
#include "PackagedAppVerifier.h"
#include "nsIMultiPartChannel.h"
#include "PackagedAppVerifier.h"
#include "nsIPackagedAppChannelListener.h"

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
// Downloading the package is triggered by calling getResource()
class PackagedAppService final
  : public nsIPackagedAppService
{
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPACKAGEDAPPSERVICE

  PackagedAppService();

private:
  ~PackagedAppService();

  // Called by PackageAppDownloader once the download has finished
  // (or encountered an error) to remove the package from mDownloadingPackages
  // Should be called on the main thread.
  nsresult NotifyPackageDownloaded(nsCString aKey);

  // Extract the URI of a package from the given packaged resource URI.
  static nsresult GetPackageURI(nsIURI *aUri, nsIURI **aPackageURI);

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
    // resource.
    static nsresult Create(nsIURI*, nsICacheStorage*, CacheEntryWriter**);

    nsCOMPtr<nsICacheEntry> mEntry;

    // Called by PackagedAppDownloader to write data to the cache entry.
    NS_METHOD ConsumeData(const char *aBuf,
                          uint32_t aCount,
                          uint32_t *aWriteCount);

  private:
    CacheEntryWriter() { }
    ~CacheEntryWriter() { }

    // Copy the security-info metadata from the channel to the cache entry
    // so packaged resources can be accessed over https.
    nsresult CopySecurityInfo(nsIChannel *aChannel);

    // Copy headers that apply to all resources in the package
    static nsresult CopyHeadersFromChannel(nsIChannel *aChannel,
                                           nsHttpResponseHead *aHead);

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
    , public nsIPackagedAppVerifierListener
  {
  public:
    typedef PackagedAppVerifier::ResourceCacheInfo ResourceCacheInfo;

  private:
    enum EErrorType {
      ERROR_MANIFEST_VERIFIED_FAILED,
      ERROR_RESOURCE_VERIFIED_FAILED,
      ERROR_GET_INSTALLER_FAILED,
      ERROR_INSTALL_RESOURCE_FAILED,
    };

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIPACKAGEDAPPVERIFIERLISTENER

    // Initializes mCacheStorage and saves aKey as mPackageKey which is later
    // used to remove this object from PackagedAppService::mDownloadingPackages
    // - aKey is a string which uniquely identifies this package within the
    //   packagedAppService
    nsresult Init(nsILoadContextInfo* aInfo, const nsCString &aKey,
                                             const nsACString& aPackageOrigin);
    // Registers a callback which gets called when the given nsIURI is downloaded
    // aURI is the full URI of a subresource, composed of packageURI + !// + subresourcePath
    // aRequester is the outer channel who makes the request for aURI.
    nsresult AddCallback(nsIURI *aURI,
                         nsICacheEntryOpenCallback *aCallback,
                         nsIChannel* aRequester);

    // Remove the callback from the resource callback list.
    nsresult RemoveCallbacks(nsICacheEntryOpenCallback* aCallback);

    // Called by PackagedAppChannelListener to note the fact that the package
    // is coming from the cache, and no subresources are to be expected as only
    // package metadata is saved in the cache.
    void SetIsFromCache(bool aFromCache) { mIsFromCache = aFromCache; }

    // Notify the observers who are interested in knowing a signed packaged content
    // is about to load from either HTTP or cache..
    void NotifyOnStartSignedPackageRequest(const nsACString& PackageOrigin);

  private:
    ~PackagedAppDownloader() { }

    // Static method used to write data into the cache entry or discard
    // if there's no writer. Used as a writer function of
    // nsIInputStream::ReadSegments.
    static NS_METHOD ConsumeData(nsIInputStream *aStream,
                                 void *aClosure,
                                 const char *aFromRawSegment,
                                 uint32_t aToOffset,
                                 uint32_t aCount,
                                 uint32_t *aWriteCount);

    //---------------------------------------------------------------
    // For PackagedAppVerifierListener.
    //---------------------------------------------------------------
    virtual void OnManifestVerified(const ResourceCacheInfo* aInfo, bool aSuccess);
    virtual void OnResourceVerified(const ResourceCacheInfo* aInfo, bool aSuccess);

    // Handle all kinds of error during package downloading.
    void OnError(EErrorType aError);

    // Called when the last part is complete or the resource is from cache.
    void FinalizeDownload(nsresult aStatusCode);

    // Get the signature from the multipart channel.
    nsCString GetSignatureFromChannel(nsIMultiPartChannel* aChannel);

    // Start off a resource hash computation and feed the HTTP response header.
    nsresult BeginHashComputation(nsIURI* aURI, nsIRequest* aRequest);

    // Ensure a packaged app verifier is created.
    void EnsureVerifier(nsIRequest *aRequest);

    // Handle all tasks about app installation like permission and system message
    // registration.
    void InstallSignedPackagedApp(const ResourceCacheInfo* aInfo);

    // Calls all the callbacks registered for the given URI.
    // aURI is the full URI of a subresource, composed of packageURI + !// + subresourcePath
    // It passes the cache entry and the result when calling OnCacheEntryAvailable
    nsresult CallCallbacks(nsIURI *aURI, nsICacheEntry *aEntry, nsresult aResult);
    // Clears all the callbacks for this package
    // This would get called at the end of downloading the package and would
    // cause us to call OnCacheEntryAvailable with a null entry. This would be
    // equivalent to a 404 when loading from the net.
    nsresult ClearCallbacks(nsresult aResult);
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

    // Whether the package is from the cache
    bool mIsFromCache;

    // Deal with verification and delegate callbacks to the downloader.
    nsRefPtr<PackagedAppVerifier> mVerifier;

    // The outer channels which have issued the request to the downloader.
    nsCOMArray<nsIPackagedAppChannelListener> mRequesters;

    // The package origin without signed package origin identifier.
    // If you need the origin with the signity taken into account, use
    // PackagedAppVerifier::GetPackageOrigin().
    nsCString mPackageOrigin;

    //The app id of the package loaded from the LoadContextInfo
    uint32_t mAppId;

    // A flag to indicate if we are processing the first request.
    bool mProcessingFirstRequest;

    // A in-memory copy of the manifest content.
    nsCString mManifestContent;
  };

  // Intercepts OnStartRequest, OnDataAvailable*, OnStopRequest method calls
  // and forwards them to the listener.
  // The target is a `mListener` which converts the package to individual
  // resources and serves them to mDownloader.
  // This class is able to perform conditional actions based on whether the
  // underlying nsIHttpChannel is served from the cache.
  // As this object serves as a listener for the channel, it is kept alive until
  // calling OnStopRequest is completed.
  class PackagedAppChannelListener final
    : public nsIStreamListener
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER

    PackagedAppChannelListener(PackagedAppDownloader *aDownloader,
                               nsIStreamListener *aListener)
    : mDownloader(aDownloader)
    , mListener(aListener)
    {
    }
  private:
    ~PackagedAppChannelListener() { }

    nsRefPtr<PackagedAppDownloader> mDownloader;
    nsCOMPtr<nsIStreamListener> mListener; // nsMultiMixedConv
  };

  // A hashtable of packages that are currently being downloaded.
  // The key is a string formed by concatenating LoadContextInfo and package URI
  // Should only be used on the main thread.
  nsRefPtrHashtable<nsCStringHashKey, PackagedAppDownloader> mDownloadingPackages;
};


} // namespace net
} // namespace mozilla

#endif // mozilla_net_PackagedAppService_h
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_PackagedAppVerifier_h
#define mozilla_net_PackagedAppVerifier_h

#include "nsICacheEntry.h"
#include "nsIURI.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsICryptoHash.h"
#include "nsIPackagedAppVerifier.h"
#include "mozilla/LinkedList.h"
#include "nsIPackagedAppUtils.h"

namespace mozilla {
namespace net {

class PackagedAppVerifier final
  : public nsIPackagedAppVerifier
  , public nsIVerificationCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIPACKAGEDAPPVERIFIER
  NS_DECL_NSIVERIFICATIONCALLBACK

public:
  enum EState {
    // The initial state.
    STATE_UNKNOWN,

    // When we are notified to process the first resource, we will start to
    // verify the manifest and go to this state no matter the package has
    // signature or not.
    STATE_MANIFEST_VERIFYING,

    // Either the package has no signature or the manifest is verified
    // successfully will we be in this state.
    STATE_MANIFEST_VERIFIED_OK,

    // iff the package has signature but the manifest is not well signed.
    STATE_MANIFEST_VERIFIED_FAILED,

    // The manifest is well signed but the resource integrity check failed.
    STATE_RESOURCE_VERIFIED_FAILED,
  };

  // The only reason to inherit from nsISupports is it needs to be
  // passed as the context to PackagedAppVerifier::OnStopRequest.
  class ResourceCacheInfo : public nsISupports
                          , public mozilla::LinkedListElement<ResourceCacheInfo>
  {
  public:
    NS_DECL_ISUPPORTS

    ResourceCacheInfo(nsIURI* aURI,
                      nsICacheEntry* aCacheEntry,
                      nsresult aStatusCode,
                      bool aIsLastPart)
      : mURI(aURI)
      , mCacheEntry(aCacheEntry)
      , mStatusCode(aStatusCode)
      , mIsLastPart(aIsLastPart)
    {
    }

    ResourceCacheInfo(const ResourceCacheInfo& aCopyFrom)
      : mURI(aCopyFrom.mURI)
      , mCacheEntry(aCopyFrom.mCacheEntry)
      , mStatusCode(aCopyFrom.mStatusCode)
      , mIsLastPart(aCopyFrom.mIsLastPart)
    {
    }

    // A ResourceCacheInfo must have a URI. If mURI is null, this
    // resource is broken.
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsICacheEntry> mCacheEntry;
    nsresult mStatusCode;
    bool mIsLastPart;

  private:
    virtual ~ResourceCacheInfo() { }
  };

public:
  PackagedAppVerifier();

  PackagedAppVerifier(nsIPackagedAppVerifierListener* aListener,
                      const nsACString& aPackageOrigin,
                      const nsACString& aSignature,
                      nsICacheEntry* aPackageCacheEntry);

  // A internal used function to let the verifier know there's a broken
  // last part.
  void SetHasBrokenLastPart(nsresult aStatusCode);

  // Used to explicitly clear the listener to avoid circula reference.
  void ClearListener() { mListener = nullptr; }

  static const char* kSignedPakOriginMetadataKey;

private:
  virtual ~PackagedAppVerifier();

  // Called when a resource is already fully written in the cache. This resource
  // will be processed and is guaranteed to be called back in either:
  //
  // 1) PackagedAppVerifierListener::OnManifestVerified:
  //    ------------------------------------------------------------------------
  //    If the resource is the first one in the package, it will be called
  //    back in OnManifestVerified no matter this package has a signature or not.
  //
  // 2) PackagedAppVerifierListener::OnResourceVerified.
  //    ------------------------------------------------------------------------
  //    Otherwise, the resource will be called back here.
  //
  void ProcessResourceCache(const ResourceCacheInfo* aInfo);

  // Callback for nsIInputStream::ReadSegment() to read manifest
  static NS_METHOD WriteManifest(nsIInputStream* aStream,
                                void* aManifest,
                                const char* aFromRawSegment,
                                uint32_t aToOffset,
                                uint32_t aCount,
                                uint32_t* aWriteCount);

  // This two functions would call the actual verifier.
  void VerifyManifest(const ResourceCacheInfo* aInfo);
  void VerifyResource(const ResourceCacheInfo* aInfo);

  void OnManifestVerified(bool aSuccess);
  void OnResourceVerified(bool aSuccess);

  // To notify that either manifest or resource check is done.
  nsCOMPtr<nsIPackagedAppVerifierListener> mListener;

  // The internal verification state.
  EState mState;

  // Initialized as a normal origin. Will be updated once we verified the manifest.
  nsCString mPackageOrigin;

  // The signature of the package.
  nsCString mSignature;

  // The app manfiest of the package
  nsCString mManifest;

  // Whether we're processing the first resource, which is the manfiest
  bool mIsFirstResource;

  // Whether this package app is signed.
  bool mIsPackageSigned;

  // The package cache entry (e.g. http://foo.com/app.pak) used to store
  // any necessarry signed package information.
  nsCOMPtr<nsICacheEntry> mPackageCacheEntry;

  // The resource URI that we are computing its hash.
  nsCString mHashingResourceURI;

  // Used to compute resource's hash value.
  nsCOMPtr<nsICryptoHash> mHasher;

  // The last computed hash value for a resource. It will be set on every
  // |EndResourceHash| call.
  nsCString mLastComputedResourceHash;

  // This will help to verify manifests and resource integrity
  nsCOMPtr<nsIPackagedAppUtils> mPackagedAppUtils;

  // A list of pending resource that is downloaded but not verified yet.
  mozilla::LinkedList<ResourceCacheInfo> mPendingResourceCacheInfoList;

  // A place to store the computed hashes of each resource.
  nsClassHashtable<nsCStringHashKey, nsCString> mResourceHashStore;
}; // class PackagedAppVerifier

} // namespace net
} // namespace mozilla

#endif // mozilla_net_PackagedAppVerifier_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICacheStorage.h"
#include "nsICacheStorageService.h"
#include "../../cache2/CacheFileUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/DebugOnly.h"
#include "nsThreadUtils.h"
#include "PackagedAppVerifier.h"
#include "nsITimer.h"
#include "nsIPackagedAppVerifier.h"
#include "mozilla/Preferences.h"

static const short kResourceHashType = nsICryptoHash::SHA256;

// If it's true, all the verification will be skipped and the package will
// be treated signed.
static bool gDeveloperMode = false;

namespace mozilla {
namespace net {

///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(PackagedAppVerifier, nsIPackagedAppVerifier)

NS_IMPL_ISUPPORTS(PackagedAppVerifier::ResourceCacheInfo, nsISupports)

const char* PackagedAppVerifier::kSignedPakOriginMetadataKey = "signed-pak-origin";

PackagedAppVerifier::PackagedAppVerifier()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "PackagedAppVerifier::OnResourceVerified must be on main thread");

  Init(nullptr, EmptyCString(), EmptyCString(), nullptr);
}

PackagedAppVerifier::PackagedAppVerifier(nsIPackagedAppVerifierListener* aListener,
                                         const nsACString& aPackageOrigin,
                                         const nsACString& aSignature,
                                         nsICacheEntry* aPackageCacheEntry)
{
  Init(aListener, aPackageOrigin, aSignature, aPackageCacheEntry);
}

PackagedAppVerifier::~PackagedAppVerifier()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mPendingResourceCacheInfoList is not thread safe.");
  while (auto i = mPendingResourceCacheInfoList.popFirst()) {
    // This seems to be the only way that we can manually delete a
    // nsISupports instance with no warning.
    RefPtr<ResourceCacheInfo> deleter(i);
  }
}

NS_IMETHODIMP PackagedAppVerifier::Init(nsIPackagedAppVerifierListener* aListener,
                                        const nsACString& aPackageOrigin,
                                        const nsACString& aSignature,
                                        nsICacheEntry* aPackageCacheEntry)
{
  static bool onceThru = false;
  if (!onceThru) {
    Preferences::AddBoolVarCache(&gDeveloperMode,
                                 "network.http.packaged-apps-developer-mode", false);
    onceThru = true;
  }

  mListener = aListener;
  mState = STATE_UNKNOWN;
  mPackageOrigin = aPackageOrigin;
  mSignature = aSignature;
  mIsPackageSigned = false;
  mPackageCacheEntry = aPackageCacheEntry;

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStreamListener
//----------------------------------------------------------------------

// @param aRequest nullptr.
// @param aContext The URI of the resource. (nsIURI)
NS_IMETHODIMP
PackagedAppVerifier::OnStartRequest(nsIRequest *aRequest,
                                    nsISupports *aContext)
{
  if (!mHasher) {
    mHasher = do_CreateInstance("@mozilla.org/security/hash;1");
  }

  NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetAsciiSpec(mHashingResourceURI);

  return mHasher->Init(kResourceHashType);
}

// @param aRequest nullptr.
// @param aContext nullptr.
// @param aInputStream as-is.
// @param aOffset as-is.
// @param aCount as-is.
NS_IMETHODIMP
PackagedAppVerifier::OnDataAvailable(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsIInputStream *aInputStream,
                                     uint64_t aOffset,
                                     uint32_t aCount)
{
  MOZ_ASSERT(!mHashingResourceURI.IsEmpty(), "MUST call BeginResourceHash first.");
  NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);
  return mHasher->UpdateFromStream(aInputStream, aCount);
}

// @param aRequest nullptr.
// @param aContext The resource cache info.
// @param aStatusCode as-is,
NS_IMETHODIMP
PackagedAppVerifier::OnStopRequest(nsIRequest* aRequest,
                                    nsISupports* aContext,
                                    nsresult aStatusCode)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "mHashingResourceURI is not thread safe.");

  NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);

  nsAutoCString hash;
  nsresult rv = mHasher->Finish(true, hash);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Hash of %s is %s", mHashingResourceURI.get(), hash.get()));

  // Store the computated hash associated with the resource URI.
  mResourceHashStore.Put(mHashingResourceURI, new nsCString(hash));
  mHashingResourceURI = EmptyCString();

  // Get a internal copy and take over the life cycle handling
  // since the linked list we use only supports pointer-based element.
  ResourceCacheInfo* info
    = new ResourceCacheInfo(*(static_cast<ResourceCacheInfo*>(aContext)));

  ProcessResourceCache(info);

  return NS_OK;
}

void
PackagedAppVerifier::ProcessResourceCache(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "ProcessResourceCache must be on main thread");

  // Queue this info since we might process it asynchronously.
  mPendingResourceCacheInfoList.insertBack(const_cast<ResourceCacheInfo*>(aInfo));

  switch (mState) {
  case STATE_UNKNOWN:
    // The first resource has to be the manifest.
    VerifyManifest(aInfo);
    break;

  case STATE_MANIFEST_VERIFYING:
    // A resource is cached in the middle of manifest verification.
    // Verify it until the manifest is verified.
    break;

  case STATE_MANIFEST_VERIFIED_OK:
    VerifyResource(aInfo);
    break;

  case STATE_MANIFEST_VERIFIED_FAILED:
    LOG(("Resource not verified because manifest verification failed."));
    FireVerifiedEvent(false, false);
    break;

  default:
    MOZ_CRASH("Unexpected PackagedAppVerifier state."); // Shouldn't get here.
    break;
  }
}

void
PackagedAppVerifier::FireVerifiedEvent(bool aForManifest, bool aSuccess)
{
  nsCOMPtr<nsIRunnable> r;

  if (aForManifest) {
    r = NS_NewRunnableMethodWithArgs<bool>(this,
                                           &PackagedAppVerifier::OnManifestVerified,
                                           aSuccess);
  } else {
    r = NS_NewRunnableMethodWithArgs<bool>(this,
                                           &PackagedAppVerifier::OnResourceVerified,
                                           aSuccess);
  }

  NS_DispatchToMainThread(r);
}

void
PackagedAppVerifier::VerifyManifest(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Manifest verification must be on main thread");

  LOG(("Ready to verify manifest."));

  if (!aInfo->mURI) { // Broken last part.
    FireVerifiedEvent(false, false);
    mState = STATE_MANIFEST_VERIFIED_FAILED;
    return;
  }

  mState = STATE_MANIFEST_VERIFYING;

  if (gDeveloperMode) {
    LOG(("Developer mode! Bypass verification."));
    FireVerifiedEvent(true, true);
    return;
  }

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do verification."));
    FireVerifiedEvent(true, true);
    return;
  }

  // TODO: Implement manifest verification.
  LOG(("Manifest verification not implemented yet. See Bug 1178518."));
  FireVerifiedEvent(true, false);
}

void
PackagedAppVerifier::VerifyResource(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Resource verification must be on main thread");

  if (!aInfo->mURI) { // Broken last part.
    FireVerifiedEvent(false, false);
    return;
  }

  // Look up the resource hash that we computed and stored to
  // mResourceHashStore before.
  nsAutoCString uriAsAscii;
  aInfo->mURI->GetAsciiSpec(uriAsAscii);
  nsCString* resourceHash = mResourceHashStore.Get(uriAsAscii);

  if (!resourceHash) {
    LOG(("Hash value for %s is not computed. ERROR!", uriAsAscii.get()));
    MOZ_CRASH();
  }

  if (gDeveloperMode) {
    LOG(("Developer mode! Bypass integrity check."));
    FireVerifiedEvent(false, true);
    return;
  }

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do resource integrity check."));
    FireVerifiedEvent(false, true);
    return;
  }

  // TODO: Implement resource integrity check.
  LOG(("Resource integrity check not implemented yet. See Bug 1178518."));
  FireVerifiedEvent(false, false);
}

void
PackagedAppVerifier::OnManifestVerified(bool aSuccess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "OnManifestVerified must be on main thread.");

  LOG(("PackagedAppVerifier::OnManifestVerified: %d", aSuccess));

  // The listener could have been removed before we verify the resource.
  if (!mListener) {
    return;
  }

  // Only when the manifest verified and package has signature would we
  // regard this package is signed.
  mIsPackageSigned = aSuccess && !mSignature.IsEmpty();

  mState = aSuccess ? STATE_MANIFEST_VERIFIED_OK
                    : STATE_MANIFEST_VERIFIED_FAILED;

  // TODO: Update mPackageOrigin.

  // If the package is signed, add related info to the package cache.
  if (mIsPackageSigned && mPackageCacheEntry) {
    LOG(("This package is signed. Add this info to the cache channel."));
    if (mPackageCacheEntry) {
      mPackageCacheEntry->SetMetaDataElement(kSignedPakOriginMetadataKey,
                                             mPackageOrigin.get());
      mPackageCacheEntry = nullptr; // the cache entry is no longer needed.
    }
  }

  RefPtr<ResourceCacheInfo> info(mPendingResourceCacheInfoList.popFirst());
  MOZ_ASSERT(info);

  mListener->OnVerified(true, // aIsManifest.
                        info->mURI,
                        info->mCacheEntry,
                        info->mStatusCode,
                        info->mIsLastPart,
                        aSuccess);

  LOG(("Ready to verify resources that were cached during verification"));
  // Verify the resources which were cached during verification accordingly.
  for (auto i = mPendingResourceCacheInfoList.getFirst(); i; i = i->getNext()) {
    VerifyResource(i);
  }
}

void
PackagedAppVerifier::OnResourceVerified(bool aSuccess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "PackagedAppVerifier::OnResourceVerified must be on main thread");

  // The listener could have been removed before we verify the resource.
  if (!mListener) {
    return;
  }

  RefPtr<ResourceCacheInfo> info(mPendingResourceCacheInfoList.popFirst());
  MOZ_ASSERT(info);

  mListener->OnVerified(false, // aIsManifest.
                        info->mURI,
                        info->mCacheEntry,
                        info->mStatusCode,
                        info->mIsLastPart,
                        aSuccess);
}

void
PackagedAppVerifier::SetHasBrokenLastPart(nsresult aStatusCode)
{
  // Append a record with null URI as a broken last part.

  ResourceCacheInfo* info
    = new ResourceCacheInfo(nullptr, nullptr, aStatusCode, true);

  mPendingResourceCacheInfoList.insertBack(info);
}

//---------------------------------------------------------------
// nsIPackagedAppVerifier.
//---------------------------------------------------------------

NS_IMETHODIMP
PackagedAppVerifier::GetPackageOrigin(nsACString& aPackageOrigin)
{
  aPackageOrigin = mPackageOrigin;
  return NS_OK;
}

NS_IMETHODIMP
PackagedAppVerifier::GetIsPackageSigned(bool* aIsPackagedSigned)
{
  *aIsPackagedSigned = mIsPackageSigned;
  return NS_OK;
}

NS_IMETHODIMP
PackagedAppVerifier::CreateResourceCacheInfo(nsIURI* aUri,
                                             nsICacheEntry* aCacheEntry,
                                             nsresult aStatusCode,
                                             bool aIsLastPart,
                                             nsISupports** aReturn)
{
  nsCOMPtr<nsISupports> info =
    new ResourceCacheInfo(aUri, aCacheEntry, aStatusCode, aIsLastPart);

  info.forget(aReturn);

  return NS_OK;
}


} // namespace net
} // namespace mozilla

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
  NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);

  nsresult rv = mHasher->Finish(true, mLastComputedResourceHash);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Hash of %s is %s", mHashingResourceURI.get(),
                           mLastComputedResourceHash.get()));

  ProcessResourceCache(static_cast<ResourceCacheInfo*>(aContext));

  return NS_OK;
}

void
PackagedAppVerifier::ProcessResourceCache(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "ProcessResourceCache must be on main thread");

  switch (mState) {
  case STATE_UNKNOWN:
    // The first resource has to be the manifest.
    VerifyManifest(aInfo);
    break;

  case STATE_MANIFEST_VERIFIED_OK:
    VerifyResource(aInfo);
    break;

  case STATE_MANIFEST_VERIFIED_FAILED:
    OnResourceVerified(aInfo, false);
    break;

  default:
    MOZ_CRASH("Unexpected PackagedAppVerifier state."); // Shouldn't get here.
    break;
  }
}

void
PackagedAppVerifier::VerifyManifest(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Manifest verification must be on main thread");

  LOG(("Ready to verify manifest."));

  if (gDeveloperMode) {
    LOG(("Developer mode! Bypass verification."));
    OnManifestVerified(aInfo, true);
    return;
  }

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do verification."));
    OnManifestVerified(aInfo, true);
    return;
  }

  // TODO: Implement manifest verification.
  LOG(("Manifest verification not implemented yet. See Bug 1178518."));
  OnManifestVerified(aInfo, false);
}

void
PackagedAppVerifier::VerifyResource(const ResourceCacheInfo* aInfo)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Resource verification must be on main thread");

  LOG(("Checking the resource integrity. '%s'", mLastComputedResourceHash.get()));

  if (gDeveloperMode) {
    LOG(("Developer mode! Bypass integrity check."));
    OnResourceVerified(aInfo, true);
    return;
  }

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do resource integrity check."));
    OnResourceVerified(aInfo, true);
    return;
  }

  // TODO: Implement resource integrity check.
  LOG(("Resource integrity check not implemented yet. See Bug 1178518."));
  OnResourceVerified(aInfo, false);
}

void
PackagedAppVerifier::OnManifestVerified(const ResourceCacheInfo* aInfo, bool aSuccess)
{
  LOG(("PackagedAppVerifier::OnManifestVerified: %d", aSuccess));

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

  mListener->OnVerified(true, // aIsManifest.
                        aInfo->mURI,
                        aInfo->mCacheEntry,
                        aInfo->mStatusCode,
                        aInfo->mIsLastPart,
                        aSuccess);

  LOG(("PackagedAppVerifier::OnManifestVerified done"));
}

void
PackagedAppVerifier::OnResourceVerified(const ResourceCacheInfo* aInfo, bool aSuccess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread(),
                     "PackagedAppVerifier::OnResourceVerified must be on main thread");

  mListener->OnVerified(false, // aIsManifest.
                        aInfo->mURI,
                        aInfo->mCacheEntry,
                        aInfo->mStatusCode,
                        aInfo->mIsLastPart,
                        aSuccess);
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

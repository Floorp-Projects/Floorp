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
#include "nsIPackagedAppUtils.h"
#include "nsIInputStream.h"
#include "nsComponentManagerUtils.h"
#include "nsIURL.h"
#include "mozilla/BasePrincipal.h"
#include "HttpLog.h"

static const short kResourceHashType = nsICryptoHash::SHA256;

static bool gSignedAppEnabled = false;

namespace mozilla {
namespace net {

///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(PackagedAppVerifier, nsIPackagedAppVerifier, nsIVerificationCallback)

NS_IMPL_ISUPPORTS(PackagedAppVerifier::ResourceCacheInfo, nsISupports)

const char* PackagedAppVerifier::kSignedPakIdMetadataKey = "package-id";

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
    Preferences::AddBoolVarCache(&gSignedAppEnabled,
                                 "network.http.signed-packages.enabled", false);
    onceThru = true;
  }

  mListener = aListener;
  mState = STATE_UNKNOWN;
  mSignature = aSignature;
  mIsPackageSigned = false;
  mPackageCacheEntry = aPackageCacheEntry;
  mIsFirstResource = true;
  mManifest = EmptyCString();

  bool success = NeckoOriginAttributes().PopulateFromOrigin(aPackageOrigin, mPackageOrigin);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  mBypassVerification = (mPackageOrigin ==
      Preferences::GetCString("network.http.signed-packages.trusted-origin"));

  LOG(("mBypassVerification = %d\n", mBypassVerification));
  LOG(("mPackageOrigin = %s\n", mPackageOrigin.get()));

  nsresult rv;
  mPackagedAppUtils = do_CreateInstance(NS_PACKAGEDAPPUTILS_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    LOG(("create packaged app utils failed"));
    return rv;
  }

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
  if (mIsFirstResource) {
    // The first resource must be the manifest, hashes not needed
    return NS_OK;
  }

  if (!mHasher) {
    mHasher = do_CreateInstance("@mozilla.org/security/hash;1");
  }

  NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri = do_QueryInterface(aContext);
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  uri->GetAsciiSpec(mHashingResourceURI);

  return mHasher->Init(kResourceHashType);
}

nsresult
PackagedAppVerifier::WriteManifest(nsIInputStream* aStream,
                                   void* aManifest,
                                   const char* aFromRawSegment,
                                   uint32_t aToOffset,
                                   uint32_t aCount,
                                   uint32_t* aWriteCount)
{
  LOG(("WriteManifest: length %u", aCount));
  LOG(("%s", nsCString(aFromRawSegment, aCount).get()));
  nsCString* manifest = static_cast<nsCString*>(aManifest);
  manifest->AppendASCII(aFromRawSegment, aCount);
  *aWriteCount = aCount;
  return NS_OK;
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
  if (mIsFirstResource) {
    // The first resource must be the manifest, hash value not needed.
    // Instead, we read from the input stream and append to mManifest.
    uint32_t count;
    LOG(("ReadSegments: size = %u", aCount));
    nsresult rv = aInputStream->ReadSegments(WriteManifest, &mManifest, aCount, &count);
    MOZ_ASSERT(count == aCount, "Bytes read by ReadSegments don't match");
    return rv;
  }

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

  if (mIsFirstResource) {
    // The first resource must be the manifest, hash value not needed
    mIsFirstResource = false;
  } else {
    NS_ENSURE_TRUE(mHasher, NS_ERROR_FAILURE);

    nsAutoCString hash;
    nsresult rv = mHasher->Finish(true, hash);
    NS_ENSURE_SUCCESS(rv, rv);

    LOG(("Hash of %s is %s", mHashingResourceURI.get(), hash.get()));

    // Store the computated hash associated with the resource URI.
    mResourceHashStore.Put(mHashingResourceURI, new nsCString(hash));
    mHashingResourceURI = EmptyCString();
  }

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

NS_IMETHODIMP
PackagedAppVerifier::FireVerifiedEvent(bool aForManifest, bool aSuccess)
{
  LOG(("FireVerifiedEvent aForManifest=%d aSuccess=%d", aForManifest, aSuccess));
  nsCOMPtr<nsIRunnable> r;

  if (aForManifest) {
    r = NewRunnableMethod<bool>(this,
                                &PackagedAppVerifier::OnManifestVerified,
                                aSuccess);
  } else {
    r = NewRunnableMethod<bool>(this,
                                &PackagedAppVerifier::OnResourceVerified,
                                aSuccess);
  }

  NS_DispatchToMainThread(r);

  return NS_OK;
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

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do verification."));
    FireVerifiedEvent(true, true);
    return;
  }

  LOG(("Signature: length = %u\n%s", mSignature.Length(), mSignature.get()));
  LOG(("Manifest: length = %u\n%s", mManifest.Length(), mManifest.get()));

  bool useDeveloperRoot =
    !Preferences::GetCString("network.http.signed-packages.developer-root").IsEmpty();
  nsresult rv = mPackagedAppUtils->VerifyManifest(mSignature, mManifest,
                                                  this, useDeveloperRoot);
  if (NS_FAILED(rv)) {
    LOG(("VerifyManifest FAILED rv = %u", (unsigned)rv));
  }
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

  if (mBypassVerification) {
    LOG(("Origin is trusted. Bypass integrity check."));
    FireVerifiedEvent(false, true);
    return;
  }

  if (mSignature.IsEmpty()) {
    LOG(("No signature. No need to do resource integrity check."));
    FireVerifiedEvent(false, true);
    return;
  }

  nsAutoCString path;
  nsCOMPtr<nsIURL> url(do_QueryInterface(aInfo->mURI));
  if (url) {
    url->GetFilePath(path);
  }
  int32_t pos = path.Find("!//");
  if (pos == kNotFound) {
    FireVerifiedEvent(false, false);
    return;
  }
  // Only keep the part after "!//"
  path.Cut(0, pos + 3);

  mPackagedAppUtils->CheckIntegrity(path, *resourceHash, this);
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


  if (!aSuccess && mBypassVerification) {
    aSuccess = true;
    LOG(("Developer mode! Treat junk signature valid."));
  }

  if (aSuccess && !mSignature.IsEmpty()) {
    // Get the package location from the manifest
    nsAutoCString packageOrigin;
    mPackagedAppUtils->GetPackageOrigin(packageOrigin);
    if (packageOrigin != mPackageOrigin) {
      aSuccess = false;
      LOG(("moz-package-location doesn't match:\nFrom: %s\nManifest: %s\n", mPackageOrigin.get(), packageOrigin.get()));
    }
  }

  // Only when the manifest verified and package has signature would we
  // regard this package is signed.
  mIsPackageSigned = aSuccess && !mSignature.IsEmpty();

  mState = aSuccess ? STATE_MANIFEST_VERIFIED_OK
                    : STATE_MANIFEST_VERIFIED_FAILED;

  // Obtain the package identifier from manifest if the package is signed.
  if (mIsPackageSigned) {
    mPackagedAppUtils->GetPackageIdentifier(mPackageIdentifer);
    LOG(("PackageIdentifer is: %s", mPackageIdentifer.get()));
  }

  // If the signature verification failed, doom the package cache to
  // make its subresources unavailable in the subsequent requests.
  if (!aSuccess && mPackageCacheEntry) {
    mPackageCacheEntry->AsyncDoom(nullptr);
  }

  // If the package is signed, add related info to the package cache.
  if (mIsPackageSigned && mPackageCacheEntry) {
    LOG(("This package is signed. Add this info to the cache channel."));
    if (mPackageCacheEntry) {
      mPackageCacheEntry->SetMetaDataElement(kSignedPakIdMetadataKey,
                                             mPackageIdentifer.get());
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

bool
PackagedAppVerifier::WouldVerify() const
{
  return gSignedAppEnabled && !mSignature.IsEmpty();
}

//---------------------------------------------------------------
// nsIPackagedAppVerifier.
//---------------------------------------------------------------

NS_IMETHODIMP
PackagedAppVerifier::GetPackageIdentifier(nsACString& aPackageIdentifier)
{
  aPackageIdentifier = mPackageIdentifer;
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

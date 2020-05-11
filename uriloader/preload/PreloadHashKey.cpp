/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreloadHashKey.h"

#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"

namespace mozilla {

PreloadHashKey::PreloadHashKey(const PreloadHashKey* aKey)
    : nsURIHashKey(aKey->mKey) {
  *this = *aKey;
}

PreloadHashKey::PreloadHashKey(PreloadHashKey&& aToMove)
    : nsURIHashKey(std::move(aToMove)) {
  mAs = std::move(aToMove.mAs);
  mCORSMode = std::move(aToMove.mCORSMode);
  mReferrerPolicy = std::move(aToMove.mReferrerPolicy);

  switch (mAs) {
    case ResourceType::SCRIPT:
      break;
    case ResourceType::STYLE:
      break;
    case ResourceType::IMAGE:
      break;
    case ResourceType::FONT:
      break;
    case ResourceType::FETCH:
      break;
    case ResourceType::NONE:
      break;
  }
}

PreloadHashKey& PreloadHashKey::operator=(const PreloadHashKey& aOther) {
  MOZ_ASSERT(mAs == ResourceType::NONE || aOther.mAs == ResourceType::NONE,
             "Assigning more than once, only reset is allowed");

  nsURIHashKey::operator=(aOther);

  mAs = aOther.mAs;
  mCORSMode = aOther.mCORSMode;
  mReferrerPolicy = aOther.mReferrerPolicy;

  switch (mAs) {
    case ResourceType::SCRIPT:
      break;
    case ResourceType::STYLE:
      break;
    case ResourceType::IMAGE:
      break;
    case ResourceType::FONT:
      break;
    case ResourceType::FETCH:
      break;
    case ResourceType::NONE:
      break;
  }

  return *this;
}

bool PreloadHashKey::KeyEquals(KeyTypePointer aOther) const {
  if (mAs != aOther->mAs || mCORSMode != aOther->mCORSMode ||
      mReferrerPolicy != aOther->mReferrerPolicy) {
    return false;
  }

  if (!nsURIHashKey::KeyEquals(
          static_cast<const nsURIHashKey*>(aOther)->GetKey())) {
    return false;
  }

  switch (mAs) {
    case ResourceType::SCRIPT:
      break;
    case ResourceType::STYLE:
      break;
    case ResourceType::IMAGE:
      break;
    case ResourceType::FONT:
      break;
    case ResourceType::FETCH:
      break;
    case ResourceType::NONE:
      break;
  }

  return true;
}

// static
PLDHashNumber PreloadHashKey::HashKey(KeyTypePointer aKey) {
  PLDHashNumber hash = nsURIHashKey::HashKey(aKey->mKey);

  // Enough to use the common attributes
  hash = AddToHash(hash, static_cast<uint32_t>(aKey->mAs));
  hash = AddToHash(hash, static_cast<uint32_t>(aKey->mCORSMode));
  hash = AddToHash(hash, static_cast<uint32_t>(aKey->mReferrerPolicy));

  return hash;
}

}  // namespace mozilla

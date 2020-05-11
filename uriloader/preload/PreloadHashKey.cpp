/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreloadHashKey.h"

#include "mozilla/dom/Element.h"  // StringToCORSMode
#include "nsIPrincipal.h"
#include "nsIReferrerInfo.h"

namespace mozilla {

PreloadHashKey::PreloadHashKey(const nsIURI* aKey, ResourceType aAs)
    : nsURIHashKey(aKey), mAs(aAs) {}

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
      mScript = std::move(aToMove.mScript);
      break;
    case ResourceType::STYLE:
      mStyle = std::move(aToMove.mStyle);
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
      mScript = aOther.mScript;
      break;
    case ResourceType::STYLE:
      mStyle = aOther.mStyle;
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

// static
PreloadHashKey PreloadHashKey::CreateAsScript(
    nsIURI* aURI, const CORSMode& aCORSMode, const dom::ScriptKind& aScriptKind,
    const dom::ReferrerPolicy& aReferrerPolicy) {
  PreloadHashKey key(aURI, ResourceType::SCRIPT);
  key.mCORSMode = aCORSMode;
  key.mReferrerPolicy = aReferrerPolicy;

  key.mScript.mScriptKind = aScriptKind;

  return key;
}

// static
PreloadHashKey PreloadHashKey::CreateAsScript(
    nsIURI* aURI, const nsAString& aCrossOrigin, const nsAString& aType,
    const dom::ReferrerPolicy& aReferrerPolicy) {
  dom::ScriptKind scriptKind = dom::ScriptKind::eClassic;
  if (aType.LowerCaseEqualsASCII("module")) {
    scriptKind = dom::ScriptKind::eModule;
  }
  CORSMode crossOrigin = dom::Element::StringToCORSMode(aCrossOrigin);

  return CreateAsScript(aURI, crossOrigin, scriptKind, aReferrerPolicy);
}

// static
PreloadHashKey PreloadHashKey::CreateAsStyle(
    nsIURI* aURI, nsIPrincipal* aPrincipal, nsIReferrerInfo* aReferrerInfo,
    CORSMode aCORSMode, css::SheetParsingMode aParsingMode) {
  PreloadHashKey key(aURI, ResourceType::STYLE);
  key.mCORSMode = aCORSMode;

  key.mStyle.mPrincipal = aPrincipal;
  key.mStyle.mParsingMode = aParsingMode;
  key.mStyle.mReferrerInfo = aReferrerInfo;

  return key;
}

// static
PreloadHashKey PreloadHashKey::CreateAsStyle(
    css::SheetLoadData& aSheetLoadData) {
  return CreateAsStyle(aSheetLoadData.mURI, aSheetLoadData.mLoaderPrincipal,
                       aSheetLoadData.ReferrerInfo(),
                       aSheetLoadData.mSheet->GetCORSMode(),
                       aSheetLoadData.mSheet->ParsingMode());
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
      if (mScript.mScriptKind != aOther->mScript.mScriptKind) {
        return false;
      }
      break;
    case ResourceType::STYLE: {
      if (!mStyle.mPrincipal != !aOther->mStyle.mPrincipal) {
        // One or the other has a principal, but not both... not equal
        return false;
      }
      if (mStyle.mParsingMode != aOther->mStyle.mParsingMode) {
        return false;
      }
      bool eq;
      if (NS_FAILED(mStyle.mReferrerInfo->Equals(aOther->mStyle.mReferrerInfo,
                                                 &eq)) ||
          !eq) {
        return false;
      }
      if (mStyle.mPrincipal && (NS_FAILED(mStyle.mPrincipal->Equals(
                                    aOther->mStyle.mPrincipal, &eq)) ||
                                !eq)) {
        return false;
      }
      break;
    }
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

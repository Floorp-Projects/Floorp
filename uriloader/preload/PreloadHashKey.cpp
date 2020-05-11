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
  mPrincipal = std::move(aToMove.mPrincipal);

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
  mPrincipal = aOther.mPrincipal;

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
  key.mPrincipal = aPrincipal;

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

// static
PreloadHashKey PreloadHashKey::CreateAsImage(
    nsIURI* aURI, nsIPrincipal* aPrincipal, CORSMode aCORSMode,
    dom::ReferrerPolicy const& aReferrerPolicy) {
  PreloadHashKey key(aURI, ResourceType::IMAGE);
  key.mReferrerPolicy = aReferrerPolicy;
  key.mCORSMode = aCORSMode;
  key.mPrincipal = aPrincipal;

  return key;
}

// static
PreloadHashKey PreloadHashKey::CreateAsFetch(
    nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy) {
  PreloadHashKey key(aURI, ResourceType::FETCH);
  key.mReferrerPolicy = aReferrerPolicy;
  key.mCORSMode = aCORSMode;

  return key;
}

// static
PreloadHashKey PreloadHashKey::CreateAsFetch(
    nsIURI* aURI, const nsAString& aCrossOrigin,
    const dom::ReferrerPolicy& aReferrerPolicy) {
  return CreateAsFetch(aURI, dom::Element::StringToCORSMode(aCrossOrigin),
                       aReferrerPolicy);
}

PreloadHashKey PreloadHashKey::CreateAsFont(
    nsIURI* aURI, const CORSMode aCORSMode,
    const dom::ReferrerPolicy& aReferrerPolicy) {
  PreloadHashKey key(aURI, ResourceType::FONT);
  key.mReferrerPolicy = aReferrerPolicy;
  key.mCORSMode = aCORSMode;

  return key;
}

bool PreloadHashKey::KeyEquals(KeyTypePointer aOther) const {
  if (mAs != aOther->mAs || mCORSMode != aOther->mCORSMode ||
      mReferrerPolicy != aOther->mReferrerPolicy) {
    return false;
  }
  if (!mPrincipal != !aOther->mPrincipal) {
    // One or the other has a principal, but not both... not equal
    return false;
  }

  if (mPrincipal && !mPrincipal->Equals(aOther->mPrincipal)) {
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
      if (mStyle.mParsingMode != aOther->mStyle.mParsingMode) {
        return false;
      }
      bool eq;
      if (NS_FAILED(mStyle.mReferrerInfo->Equals(aOther->mStyle.mReferrerInfo,
                                                 &eq)) ||
          !eq) {
        return false;
      }
      break;
    }
    case ResourceType::IMAGE:
      // No more checks needed.  The image cache key consists of the document
      // (which we are scoped into), origin attributes (which we compare as part
      // of the principal check) and the URL.  imgLoader::ValidateEntry compares
      // CORS, referrer info and principal, which we do by default.
      break;
    case ResourceType::FONT:
      break;
    case ResourceType::FETCH:
      // No more checks needed.
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

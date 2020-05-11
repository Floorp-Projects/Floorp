/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloadHashKey_h__
#define PreloadHashKey_h__

#include "mozilla/CORSMode.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
#include "mozilla/dom/ScriptKind.h"
#include "nsURIHashKey.h"

class nsIPrincipal;
class nsIReferrerInfo;

namespace mozilla {

/**
 * This key is used for coalescing and lookup of preloading or regular
 * speculative loads.  It consists of:
 * - the resource type, which is the value of the 'as' attribute
 * - the URI of the resource
 * - set of attributes that is common to all resource types
 * - resource-type-specific attributes that we use to distinguish loads that has
 *   to be treated separately, some of these attributes may remain at their
 *   default values
 */
class PreloadHashKey : public nsURIHashKey {
 public:
  enum class ResourceType : uint8_t { NONE, SCRIPT, STYLE, IMAGE, FONT, FETCH };

  typedef PreloadHashKey* KeyType;
  typedef const PreloadHashKey* KeyTypePointer;

  PreloadHashKey() = default;
  PreloadHashKey(const nsIURI* aKey, ResourceType aAs);
  explicit PreloadHashKey(const PreloadHashKey* aKey);
  PreloadHashKey(PreloadHashKey&& aToMove);

  PreloadHashKey& operator=(const PreloadHashKey& aOther);

  // Construct key for "script"
  static PreloadHashKey CreateAsScript(
      nsIURI* aURI, const CORSMode& aCORSMode,
      const dom::ScriptKind& aScriptKind,
      const dom::ReferrerPolicy& aReferrerPolicy);
  static PreloadHashKey CreateAsScript(
      nsIURI* aURI, const nsAString& aCrossOrigin, const nsAString& aType,
      const dom::ReferrerPolicy& aReferrerPolicy);

  // Construct key for "style"
  static PreloadHashKey CreateAsStyle(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                      nsIReferrerInfo* aReferrerInfo,
                                      CORSMode aCORSMode,
                                      css::SheetParsingMode aParsingMode);
  static PreloadHashKey CreateAsStyle(css::SheetLoadData& aSheetLoadData);

  // Construct key for "image"
  static PreloadHashKey CreateAsImage(
      nsIURI* aURI, nsIPrincipal* aPrincipal, CORSMode aCORSMode,
      dom::ReferrerPolicy const& aReferrerPolicy);

  // Construct key for "fetch"
  static PreloadHashKey CreateAsFetch(
      nsIURI* aURI, const CORSMode aCORSMode,
      const dom::ReferrerPolicy& aReferrerPolicy);
  static PreloadHashKey CreateAsFetch(
      nsIURI* aURI, const nsAString& aCrossOrigin,
      const dom::ReferrerPolicy& aReferrerPolicy);

  // Construct key for "font"
  static PreloadHashKey CreateAsFont(
      nsIURI* aURI, const CORSMode aCORSMode,
      const dom::ReferrerPolicy& aReferrerPolicy);

  KeyType GetKey() const { return const_cast<PreloadHashKey*>(this); }
  KeyTypePointer GetKeyPointer() const { return this; }
  static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }

  bool KeyEquals(KeyTypePointer aOther) const;
  static PLDHashNumber HashKey(KeyTypePointer aKey);

#ifdef MOZILLA_INTERNAL_API
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    // Bug 1627752
    return 0;
  }
#endif

  enum { ALLOW_MEMMOVE = true };

 private:
  // Attributes common to all resource types
  ResourceType mAs = ResourceType::NONE;

  CORSMode mCORSMode = CORS_NONE;
  enum dom::ReferrerPolicy mReferrerPolicy = dom::ReferrerPolicy::_empty;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  struct {
    dom::ScriptKind mScriptKind = dom::ScriptKind::eClassic;
  } mScript;

  struct {
    nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
    css::SheetParsingMode mParsingMode = css::eAuthorSheetFeatures;
  } mStyle;
};

}  // namespace mozilla

#endif

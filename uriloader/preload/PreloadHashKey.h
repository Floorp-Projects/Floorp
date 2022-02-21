/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloadHashKey_h__
#define PreloadHashKey_h__

#include "mozilla/CORSMode.h"
#include "mozilla/css/SheetParsingMode.h"
#include "js/loader/ScriptKind.h"
#include "nsURIHashKey.h"

class nsIPrincipal;

namespace JS::loader {
enum class ScriptKind;
}

namespace mozilla {

namespace css {
class SheetLoadData;
}

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

  using KeyType = const PreloadHashKey&;
  using KeyTypePointer = const PreloadHashKey*;

  PreloadHashKey() = default;
  PreloadHashKey(const nsIURI* aKey, ResourceType aAs);
  explicit PreloadHashKey(const PreloadHashKey* aKey);
  PreloadHashKey(PreloadHashKey&& aToMove);

  PreloadHashKey& operator=(const PreloadHashKey& aOther);

  // Construct key for "script"
  static PreloadHashKey CreateAsScript(nsIURI* aURI, CORSMode aCORSMode,
                                       JS::loader::ScriptKind aScriptKind);
  static PreloadHashKey CreateAsScript(nsIURI* aURI,
                                       const nsAString& aCrossOrigin,
                                       const nsAString& aType);

  // Construct key for "style"
  static PreloadHashKey CreateAsStyle(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                      CORSMode aCORSMode,
                                      css::SheetParsingMode aParsingMode);
  static PreloadHashKey CreateAsStyle(css::SheetLoadData&);

  // Construct key for "image"
  static PreloadHashKey CreateAsImage(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                      CORSMode aCORSMode);

  // Construct key for "fetch"
  static PreloadHashKey CreateAsFetch(nsIURI* aURI, CORSMode aCORSMode);
  static PreloadHashKey CreateAsFetch(nsIURI* aURI,
                                      const nsAString& aCrossOrigin);

  // Construct key for "font"
  static PreloadHashKey CreateAsFont(nsIURI* aURI, CORSMode aCORSMode);

  KeyType GetKey() const { return *this; }
  KeyTypePointer GetKeyPointer() const { return this; }
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  bool KeyEquals(KeyTypePointer aOther) const;
  static PLDHashNumber HashKey(KeyTypePointer aKey);
  ResourceType As() const { return mAs; }

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
  nsCOMPtr<nsIPrincipal> mPrincipal;

  struct {
    JS::loader::ScriptKind mScriptKind = JS::loader::ScriptKind::eClassic;
  } mScript;

  struct {
    css::SheetParsingMode mParsingMode = css::eAuthorSheetFeatures;
  } mStyle;
};

}  // namespace mozilla

#endif

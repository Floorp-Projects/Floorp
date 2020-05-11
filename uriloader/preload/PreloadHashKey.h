/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PreloadHashKey_h__
#define PreloadHashKey_h__

#include "mozilla/CORSMode.h"
#include "mozilla/dom/ReferrerPolicyBinding.h"
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
  explicit PreloadHashKey(const PreloadHashKey* aKey);
  PreloadHashKey(PreloadHashKey&& aToMove);
  ~PreloadHashKey() = default;

  PreloadHashKey& operator=(const PreloadHashKey& aOther);

  // TODO
  // static CreateAsScript(...);
  // static CreateAsStyle(...);
  // static CreateAsImage(...);
  // static CreateAsFont(...);
  // static CreateAsFetch(...);

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
};

}  // namespace mozilla

#endif

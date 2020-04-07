/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CacheControlParser_h__
#define CacheControlParser_h__

#include "mozilla/Tokenizer.h"

namespace mozilla {
namespace net {

class CacheControlParser final : Tokenizer {
 public:
  explicit CacheControlParser(nsACString const& header);

  MOZ_MUST_USE bool MaxAge(uint32_t* seconds);
  MOZ_MUST_USE bool MaxStale(uint32_t* seconds);
  MOZ_MUST_USE bool MinFresh(uint32_t* seconds);
  MOZ_MUST_USE bool StaleWhileRevalidate(uint32_t* seconds);
  bool NoCache();
  bool NoStore();
  bool Public();
  bool Private();
  bool Immutable();

 private:
  void Directive();
  void IgnoreDirective();
  MOZ_MUST_USE bool SecondsValue(uint32_t* seconds, uint32_t defaultVal = 0);

  bool mMaxAgeSet;
  uint32_t mMaxAge;
  bool mMaxStaleSet;
  uint32_t mMaxStale;
  bool mMinFreshSet;
  uint32_t mMinFresh;
  bool mStaleWhileRevalidateSet;
  uint32_t mStaleWhileRevalidate;
  bool mNoCache;
  bool mNoStore;
  bool mPublic;
  bool mPrivate;
  bool mImmutable;
};

}  // namespace net
}  // namespace mozilla

#endif

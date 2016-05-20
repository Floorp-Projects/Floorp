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

class CacheControlParser final : Tokenizer
{
public:
  explicit CacheControlParser(nsACString const &header);

  bool MaxAge(uint32_t *seconds);
  bool MaxStale(uint32_t *seconds);
  bool MinFresh(uint32_t *seconds);
  bool NoCache();
  bool NoStore();

private:
  void Directive();
  void IgnoreDirective();
  bool SecondsValue(uint32_t *seconds, uint32_t defaultVal = 0);

  bool mMaxAgeSet;
  uint32_t mMaxAge;
  bool mMaxStaleSet;
  uint32_t mMaxStale;
  bool mMinFreshSet;
  uint32_t mMinFresh;
  bool mNoCache;
  bool mNoStore;
};

} // net
} // mozilla

#endif

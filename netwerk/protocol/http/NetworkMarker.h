/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set et cin ts=4 sw=2 sts=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkMarker_h__
#define NetworkMarker_h__

#include "mozilla/ProfilerMarkers.h"

namespace mozilla {
namespace net {

struct TimingStruct;
enum CacheDisposition : uint8_t;

enum class NetworkLoadType {
  LOAD_START,
  LOAD_STOP,
  LOAD_REDIRECT,
  LOAD_CANCEL
};

void profiler_add_network_marker(
    nsIURI* aURI, const nsACString& aRequestMethod, int32_t aPriority,
    uint64_t aChannelId, NetworkLoadType aType, mozilla::TimeStamp aStart,
    mozilla::TimeStamp aEnd, int64_t aCount,
    mozilla::net::CacheDisposition aCacheDisposition, uint64_t aInnerWindowID,
    const mozilla::net::TimingStruct* aTimings = nullptr,
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource = nullptr,
    const mozilla::Maybe<nsDependentCString>& aContentType = mozilla::Nothing(),
    nsIURI* aRedirectURI = nullptr, uint32_t aRedirectFlags = 0,
    uint64_t aRedirectChannelId = 0);

}  // namespace net
}  // namespace mozilla

#endif  // NetworkMarker_h__

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set expandtab ts=4 sw=2 sts=2 cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkMarker.h"

#include "HttpBaseChannel.h"
#include "nsIChannelEventSink.h"

namespace mozilla {
namespace net {

static constexpr net::TimingStruct scEmptyNetTimingStruct;

void profiler_add_network_marker(
    nsIURI* aURI, const nsACString& aRequestMethod, int32_t aPriority,
    uint64_t aChannelId, NetworkLoadType aType, mozilla::TimeStamp aStart,
    mozilla::TimeStamp aEnd, int64_t aCount,
    mozilla::net::CacheDisposition aCacheDisposition, uint64_t aInnerWindowID,
    const mozilla::net::TimingStruct* aTimings,
    UniquePtr<ProfileChunkedBuffer> aSource,
    const Maybe<nsDependentCString>& aContentType, nsIURI* aRedirectURI,
    uint32_t aRedirectFlags, uint64_t aRedirectChannelId) {
  if (!profiler_can_accept_markers()) {
    return;
  }

  nsAutoCStringN<2048> name;
  name.AppendASCII("Load ");
  // top 32 bits are process id of the load
  name.AppendInt(aChannelId & 0xFFFFFFFFu);

  // These can do allocations/frees/etc; avoid if not active
  nsAutoCStringN<2048> spec;
  if (aURI) {
    aURI->GetAsciiSpec(spec);
    name.AppendASCII(": ");
    name.Append(spec);
  }

  nsAutoCString redirect_spec;
  if (aRedirectURI) {
    aRedirectURI->GetAsciiSpec(redirect_spec);
  }

  struct NetworkMarker {
    static constexpr Span<const char> MarkerTypeName() {
      return MakeStringSpan("Network");
    }
    static void StreamJSONMarkerData(
        baseprofiler::SpliceableJSONWriter& aWriter, mozilla::TimeStamp aStart,
        mozilla::TimeStamp aEnd, int64_t aID, const ProfilerString8View& aURI,
        const ProfilerString8View& aRequestMethod, NetworkLoadType aType,
        int32_t aPri, int64_t aCount, net::CacheDisposition aCacheDisposition,
        const net::TimingStruct& aTimings,
        const ProfilerString8View& aRedirectURI,
        const ProfilerString8View& aContentType, uint32_t aRedirectFlags,
        int64_t aRedirectChannelId) {
      // This payload still streams a startTime and endTime property because it
      // made the migration to MarkerTiming on the front-end easier.
      aWriter.TimeProperty("startTime", aStart);
      aWriter.TimeProperty("endTime", aEnd);

      aWriter.IntProperty("id", aID);
      aWriter.StringProperty("status", GetNetworkState(aType));
      if (Span<const char> cacheString = GetCacheState(aCacheDisposition);
          !cacheString.IsEmpty()) {
        aWriter.StringProperty("cache", cacheString);
      }
      aWriter.IntProperty("pri", aPri);
      if (aCount > 0) {
        aWriter.IntProperty("count", aCount);
      }
      if (aURI.Length() != 0) {
        aWriter.StringProperty("URI", aURI);
      }
      if (aRedirectURI.Length() != 0) {
        aWriter.StringProperty("RedirectURI", aRedirectURI);
        aWriter.StringProperty("redirectType", getRedirectType(aRedirectFlags));
        aWriter.BoolProperty(
            "isHttpToHttpsRedirect",
            aRedirectFlags & nsIChannelEventSink::REDIRECT_STS_UPGRADE);

        if (aRedirectChannelId != 0) {
          aWriter.IntProperty("redirectId", aRedirectChannelId);
        }
      }

      aWriter.StringProperty("requestMethod", aRequestMethod);

      if (aContentType.Length() != 0) {
        aWriter.StringProperty("contentType", aContentType);
      } else {
        aWriter.NullProperty("contentType");
      }

      if (aType != NetworkLoadType::LOAD_START) {
        aWriter.TimeProperty("domainLookupStart", aTimings.domainLookupStart);
        aWriter.TimeProperty("domainLookupEnd", aTimings.domainLookupEnd);
        aWriter.TimeProperty("connectStart", aTimings.connectStart);
        aWriter.TimeProperty("tcpConnectEnd", aTimings.tcpConnectEnd);
        aWriter.TimeProperty("secureConnectionStart",
                             aTimings.secureConnectionStart);
        aWriter.TimeProperty("connectEnd", aTimings.connectEnd);
        aWriter.TimeProperty("requestStart", aTimings.requestStart);
        aWriter.TimeProperty("responseStart", aTimings.responseStart);
        aWriter.TimeProperty("responseEnd", aTimings.responseEnd);
      }
    }
    static MarkerSchema MarkerTypeDisplay() {
      return MarkerSchema::SpecialFrontendLocation{};
    }

   private:
    static Span<const char> GetNetworkState(NetworkLoadType aType) {
      switch (aType) {
        case NetworkLoadType::LOAD_START:
          return MakeStringSpan("STATUS_START");
        case NetworkLoadType::LOAD_STOP:
          return MakeStringSpan("STATUS_STOP");
        case NetworkLoadType::LOAD_REDIRECT:
          return MakeStringSpan("STATUS_REDIRECT");
        case NetworkLoadType::LOAD_CANCEL:
          return MakeStringSpan("STATUS_CANCEL");
        default:
          MOZ_ASSERT(false, "Unexpected NetworkLoadType enum value.");
          return MakeStringSpan("");
      }
    }

    static Span<const char> GetCacheState(
        net::CacheDisposition aCacheDisposition) {
      switch (aCacheDisposition) {
        case net::kCacheUnresolved:
          return MakeStringSpan("Unresolved");
        case net::kCacheHit:
          return MakeStringSpan("Hit");
        case net::kCacheHitViaReval:
          return MakeStringSpan("HitViaReval");
        case net::kCacheMissedViaReval:
          return MakeStringSpan("MissedViaReval");
        case net::kCacheMissed:
          return MakeStringSpan("Missed");
        case net::kCacheUnknown:
          return MakeStringSpan("");
        default:
          MOZ_ASSERT(false, "Unexpected CacheDisposition enum value.");
          return MakeStringSpan("");
      }
    }

    static Span<const char> getRedirectType(uint32_t aRedirectFlags) {
      MOZ_ASSERT(aRedirectFlags != 0, "aRedirectFlags should be non-zero");
      if (aRedirectFlags & nsIChannelEventSink::REDIRECT_TEMPORARY) {
        return MakeStringSpan("Temporary");
      }
      if (aRedirectFlags & nsIChannelEventSink::REDIRECT_PERMANENT) {
        return MakeStringSpan("Permanent");
      }
      if (aRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
        return MakeStringSpan("Internal");
      }
      MOZ_ASSERT(false, "Couldn't find a redirect type from aRedirectFlags");
      return MakeStringSpan("");
    }
  };

  profiler_add_marker(
      name, geckoprofiler::category::NETWORK,
      {MarkerTiming::Interval(aStart, aEnd),
       MarkerStack::TakeBacktrace(std::move(aSource)),
       MarkerInnerWindowId(aInnerWindowID)},
      NetworkMarker{}, aStart, aEnd, static_cast<int64_t>(aChannelId), spec,
      aRequestMethod, aType, aPriority, aCount, aCacheDisposition,
      aTimings ? *aTimings : scEmptyNetTimingStruct, redirect_spec,
      aContentType ? ProfilerString8View(*aContentType) : ProfilerString8View(),
      aRedirectFlags, aRedirectChannelId);
}

}  // namespace net
}  // namespace mozilla

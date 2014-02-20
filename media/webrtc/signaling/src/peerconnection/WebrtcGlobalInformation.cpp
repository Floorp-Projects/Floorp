/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGlobalInformation.h"

#include <deque>
#include <string>

#include "CSFLog.h"

#include "mozilla/dom/WebrtcGlobalInformationBinding.h"

#include "nsAutoPtr.h"
#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "mozilla/ErrorResult.h"
#include "mozilla/Vector.h"
#include "nsProxyRelease.h"

#include "rlogringbuffer.h"
#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"

using sipcc::PeerConnectionImpl;
using sipcc::PeerConnectionCtx;
using sipcc::RTCStatsQuery;

static const char* logTag = "WebrtcGlobalInformation";

namespace mozilla {
namespace dom {

typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;

static void OnStatsReport_m(
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> aStatsCallback,
  nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aQueryList);

  WebrtcGlobalStatisticsReport report;
  report.mReports.Construct();
  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    MOZ_ASSERT(*q);
    report.mReports.Value().AppendElement((*q)->report);
  }

  ErrorResult rv;
  aStatsCallback.get()->Call(report, rv);

  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing stats observer callback");
  }
}

static void GetAllStats_s(
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> aStatsCallback,
  nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(aQueryList);

  for (auto q = aQueryList->begin(); q != aQueryList->end(); ++q) {
    MOZ_ASSERT(*q);
    PeerConnectionImpl::ExecuteStatsQuery_s(*q);
  }

  NS_DispatchToMainThread(WrapRunnableNM(&OnStatsReport_m,
                                         aStatsCallback,
                                         aQueryList),
                          NS_DISPATCH_NORMAL);
}

static void OnGetLogging_m(
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> aLoggingCallback,
  const std::string& aPattern,
  nsAutoPtr<std::deque<std::string>> aLogList)
{
  ErrorResult rv;
  if (!aLogList->empty()) {
    Sequence<nsString> nsLogs;
    for (auto l = aLogList->begin(); l != aLogList->end(); ++l) {
      nsLogs.AppendElement(NS_ConvertUTF8toUTF16(l->c_str()));
    }
    aLoggingCallback.get()->Call(nsLogs, rv);
  }

  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing logging observer callback");
  }
}

static void GetLogging_s(
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> aLoggingCallback,
  const std::string& aPattern)
{
  RLogRingBuffer* logs = RLogRingBuffer::GetInstance();
  nsAutoPtr<std::deque<std::string>> result(new std::deque<std::string>);
  // Might not exist yet.
  if (logs) {
    logs->Filter(aPattern, 0, result);
  }
  NS_DispatchToMainThread(WrapRunnableNM(&OnGetLogging_m,
                                         aLoggingCallback,
                                         aPattern,
                                         result),
                          NS_DISPATCH_NORMAL);
}


void
WebrtcGlobalInformation::GetAllStats(
  const GlobalObject& aGlobal,
  WebrtcGlobalStatisticsCallback& aStatsCallback,
  ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!stsThread) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsAutoPtr<RTCStatsQueries> queries(new RTCStatsQueries);

  // If there is no PeerConnectionCtx, go through the same motions, since
  // the API consumer doesn't care why there are no PeerConnectionImpl.
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx *ctx = PeerConnectionCtx::GetInstance();
    MOZ_ASSERT(ctx);
    for (auto p = ctx->mPeerConnections.begin();
         p != ctx->mPeerConnections.end();
         ++p) {
      MOZ_ASSERT(p->second);

      if (!p->second->IsClosed()) {
        queries->append(nsAutoPtr<RTCStatsQuery>(new RTCStatsQuery(true)));
        p->second->BuildStatsQuery_m(nullptr, // all tracks
                                     queries->back());
      }
    }
  }

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalStatisticsCallback>(&aStatsCallback));

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetAllStats_s, callbackHandle, queries),
                     NS_DISPATCH_NORMAL);

  aRv = rv;
}

void
WebrtcGlobalInformation::GetLogging(
  const GlobalObject& aGlobal,
  const nsAString& aPattern,
  WebrtcGlobalLoggingCallback& aLoggingCallback,
  ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (!stsThread) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  std::string pattern(NS_ConvertUTF16toUTF8(aPattern).get());

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalLoggingCallback>(&aLoggingCallback));

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetLogging_s, callbackHandle, pattern),
                     NS_DISPATCH_NORMAL);

  if (NS_FAILED(rv)) {
    aLoggingCallback.Release();
  }

  aRv = rv;
}

} // namespace dom
} // namespace mozilla


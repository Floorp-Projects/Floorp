/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcGlobalInformation.h"
#include "mozilla/media/webrtc/WebrtcGlobal.h"
#include "WebrtcGlobalChild.h"
#include "WebrtcGlobalParent.h"

#include <deque>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <queue>
#include <type_traits>

#include "CSFLog.h"
#include "WebRtcLog.h"
#include "mozilla/dom/WebrtcGlobalInformationBinding.h"
#include "mozilla/dom/ContentChild.h"

#include "nsNetCID.h"               // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h"  // do_GetService
#include "mozilla/ErrorResult.h"
#include "mozilla/Vector.h"
#include "nsProxyRelease.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/RefPtr.h"

#include "runnable_utils.h"
#include "MediaTransportHandler.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"

static const char* wgiLogTag = "WebrtcGlobalInformation";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG wgiLogTag

namespace mozilla {
namespace dom {

typedef nsTArray<RTCStatsReportInternal> Stats;

template <class Request, typename Callback, typename Result,
          typename QueryParam>
class RequestManager {
 public:
  static Request* Create(Callback& aCallback, QueryParam& aParam) {
    mozilla::StaticMutexAutoLock lock(sMutex);

    int id = ++sLastRequestId;
    auto result =
        sRequests.insert(std::make_pair(id, Request(id, aCallback, aParam)));

    if (!result.second) {
      return nullptr;
    }

    return &result.first->second;
  }

  static void Delete(int aId) {
    mozilla::StaticMutexAutoLock lock(sMutex);
    sRequests.erase(aId);
  }

  static Request* Get(int aId) {
    mozilla::StaticMutexAutoLock lock(sMutex);
    auto r = sRequests.find(aId);

    if (r == sRequests.end()) {
      return nullptr;
    }

    return &r->second;
  }

  Result mResult;
  std::queue<RefPtr<WebrtcGlobalParent>> mContactList;
  const int mRequestId;

  RefPtr<WebrtcGlobalParent> GetNextParent() {
    while (!mContactList.empty()) {
      RefPtr<WebrtcGlobalParent> next = mContactList.front();
      mContactList.pop();
      if (next->IsActive()) {
        return next;
      }
    }

    return nullptr;
  }

  MOZ_CAN_RUN_SCRIPT
  void Complete() {
    IgnoredErrorResult rv;
    using RealCallbackType = std::remove_pointer_t<decltype(mCallback.get())>;
    RefPtr<RealCallbackType> callback(mCallback.get());
    callback->Call(mResult, rv);

    if (rv.Failed()) {
      CSFLogError(LOGTAG, "Error firing stats observer callback");
    }
  }

 protected:
  // The mutex is used to protect two related operations involving the sRequest
  // map and the sLastRequestId. For the map, it prevents more than one thread
  // from adding or deleting map entries at the same time. For id generation, it
  // creates an atomic allocation and increment.
  static mozilla::StaticMutex sMutex;
  static std::map<int, Request> sRequests;
  static int sLastRequestId;

  Callback mCallback;

  explicit RequestManager(int aId, Callback& aCallback)
      : mRequestId(aId), mCallback(aCallback) {}
  ~RequestManager() {}

 private:
  RequestManager() = delete;
  RequestManager& operator=(const RequestManager&) = delete;
};

template <class Request, typename Callback, typename Result,
          typename QueryParam>
mozilla::StaticMutex
    RequestManager<Request, Callback, Result, QueryParam>::sMutex;
template <class Request, typename Callback, typename Result,
          typename QueryParam>
std::map<int, Request>
    RequestManager<Request, Callback, Result, QueryParam>::sRequests;
template <class Request, typename Callback, typename Result,
          typename QueryParam>
int RequestManager<Request, Callback, Result, QueryParam>::sLastRequestId;

typedef nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback>
    StatsRequestCallback;

class StatsRequest
    : public RequestManager<StatsRequest, StatsRequestCallback,
                            WebrtcGlobalStatisticsReport, nsAString> {
 public:
  const nsString mPcIdFilter;
  explicit StatsRequest(int aId, StatsRequestCallback& aCallback,
                        nsAString& aFilter)
      : RequestManager(aId, aCallback), mPcIdFilter(aFilter) {}

 private:
  StatsRequest() = delete;
  StatsRequest& operator=(const StatsRequest&) = delete;
};

typedef nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> LogRequestCallback;

class LogRequest : public RequestManager<LogRequest, LogRequestCallback,
                                         Sequence<nsString>, const nsACString> {
 public:
  const nsCString mPattern;
  explicit LogRequest(int aId, LogRequestCallback& aCallback,
                      const nsACString& aPattern)
      : RequestManager(aId, aCallback), mPattern(aPattern) {}

 private:
  LogRequest() = delete;
  LogRequest& operator=(const LogRequest&) = delete;
};

class WebrtcContentParents {
 public:
  static WebrtcGlobalParent* Alloc();
  static void Dealloc(WebrtcGlobalParent* aParent);
  static bool Empty() { return sContentParents.empty(); }
  static const std::vector<RefPtr<WebrtcGlobalParent>>& GetAll() {
    return sContentParents;
  }

 private:
  static std::vector<RefPtr<WebrtcGlobalParent>> sContentParents;
  WebrtcContentParents() = delete;
  WebrtcContentParents(const WebrtcContentParents&) = delete;
  WebrtcContentParents& operator=(const WebrtcContentParents&) = delete;
};

std::vector<RefPtr<WebrtcGlobalParent>> WebrtcContentParents::sContentParents;

WebrtcGlobalParent* WebrtcContentParents::Alloc() {
  RefPtr<WebrtcGlobalParent> cp = new WebrtcGlobalParent;
  sContentParents.push_back(cp);
  return cp.get();
}

void WebrtcContentParents::Dealloc(WebrtcGlobalParent* aParent) {
  if (aParent) {
    aParent->mShutdown = true;
    auto cp =
        std::find(sContentParents.begin(), sContentParents.end(), aParent);
    if (cp != sContentParents.end()) {
      sContentParents.erase(cp);
    }
  }
}

static PeerConnectionCtx* GetPeerConnectionCtx() {
  if (PeerConnectionCtx::isActive()) {
    MOZ_ASSERT(PeerConnectionCtx::GetInstance());
    return PeerConnectionCtx::GetInstance();
  }
  return nullptr;
}

MOZ_CAN_RUN_SCRIPT
static void OnStatsReport_m(
    WebrtcGlobalChild* aThisChild, const int aRequestId,
    nsTArray<UniquePtr<dom::RTCStatsReportInternal>>&& aReports) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aThisChild) {
    Stats stats;

    // Copy stats generated for the currently active PeerConnections
    for (auto& report : aReports) {
      if (report) {
        stats.AppendElement(*report);
      }
    }
    // Reports saved for closed/destroyed PeerConnections
    auto ctx = PeerConnectionCtx::GetInstance();
    if (ctx) {
      for (auto& pc : ctx->mStatsForClosedPeerConnections) {
        stats.AppendElement(pc);
      }
    }

    Unused << aThisChild->SendGetStatsResult(aRequestId, stats);
    return;
  }

  // This is the last stats report to be collected. (Must be the gecko process).
  MOZ_ASSERT(XRE_IsParentProcess());

  StatsRequest* request = StatsRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(LOGTAG, "Bad RequestId");
    return;
  }

  for (auto& report : aReports) {
    if (report) {
      request->mResult.mReports.AppendElement(*report, fallible);
    }
  }

  // Reports saved for closed/destroyed PeerConnections
  auto ctx = PeerConnectionCtx::GetInstance();
  if (ctx) {
    for (auto&& pc : ctx->mStatsForClosedPeerConnections) {
      request->mResult.mReports.AppendElement(pc, fallible);
    }
  }

  request->Complete();
  StatsRequest::Delete(aRequestId);
}

MOZ_CAN_RUN_SCRIPT
static void OnGetLogging_m(WebrtcGlobalChild* aThisChild, const int aRequestId,
                           Sequence<nsString>&& aLogList) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aLogList.IsEmpty()) {
    aLogList.AppendElement(NS_LITERAL_STRING("+++++++ END ++++++++"), fallible);
  }

  if (aThisChild) {
    // Add this log to the collection of logs and call into
    // the next content process.
    Unused << aThisChild->SendGetLogResult(aRequestId, aLogList);
    return;
  }

  // This is the last log to be collected. (Must be the gecko process).
  MOZ_ASSERT(XRE_IsParentProcess());

  LogRequest* request = LogRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(LOGTAG, "Bad RequestId");
    return;
  }

  request->mResult.AppendElements(std::move(aLogList), fallible);
  request->Complete();
  LogRequest::Delete(aRequestId);
}

static void RunStatsQuery(
    const std::map<const std::string, PeerConnectionImpl*>& aPeerConnections,
    const nsAString& aPcIdFilter, WebrtcGlobalChild* aThisChild,
    const int aRequestId) {
  nsTArray<RefPtr<RTCStatsReportPromise>> promises;

  for (auto& idAndPc : aPeerConnections) {
    MOZ_ASSERT(idAndPc.second);
    PeerConnectionImpl& pc = *idAndPc.second;
    if (aPcIdFilter.IsEmpty() ||
        aPcIdFilter.EqualsASCII(pc.GetIdAsAscii().c_str())) {
      if (pc.HasMedia()) {
        promises.AppendElement(
            pc.GetStats(nullptr, true)
                ->Then(
                    GetMainThreadSerialEventTarget(), __func__,
                    [=](UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
                      return RTCStatsReportPromise::CreateAndResolve(
                          std::move(aReport), __func__);
                    },
                    [=](nsresult aError) {
                      // Ignore errors! Just resolve with a nullptr.
                      return RTCStatsReportPromise::CreateAndResolve(
                          UniquePtr<dom::RTCStatsReportInternal>(), __func__);
                    }));
      }
    }
  }

  RTCStatsReportPromise::All(GetMainThreadSerialEventTarget(), promises)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we're going to run that
          // function async anyway.
          [aThisChild, aRequestId](
              nsTArray<UniquePtr<dom::RTCStatsReportInternal>>&& aReports)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                OnStatsReport_m(aThisChild, aRequestId, std::move(aReports));
              },
          [=](nsresult) { MOZ_CRASH(); });
}

void ClearClosedStats() {
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    ctx->mStatsForClosedPeerConnections.Clear();
  }
}

void WebrtcGlobalInformation::ClearAllStats(const GlobalObject& aGlobal) {
  if (!NS_IsMainThread()) {
    return;
  }

  // Chrome-only API
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!WebrtcContentParents::Empty()) {
    // Pass on the request to any content process based PeerConnections.
    for (auto& cp : WebrtcContentParents::GetAll()) {
      Unused << cp->SendClearStatsRequest();
    }
  }

  // Flush the history for the chrome process
  ClearClosedStats();
}

void WebrtcGlobalInformation::GetAllStats(
    const GlobalObject& aGlobal, WebrtcGlobalStatisticsCallback& aStatsCallback,
    const Optional<nsAString>& pcIdFilter, ErrorResult& aRv) {
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  // CallbackObject does not support threadsafe refcounting, and must be
  // used and destroyed on main.
  StatsRequestCallback callbackHandle(
      new nsMainThreadPtrHolder<WebrtcGlobalStatisticsCallback>(
          "WebrtcGlobalStatisticsCallback", &aStatsCallback));

  nsString filter;
  if (pcIdFilter.WasPassed()) {
    filter = pcIdFilter.Value();
  }

  auto* request = StatsRequest::Create(callbackHandle, filter);

  if (!request) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!WebrtcContentParents::Empty()) {
    // Pass on the request to any content based PeerConnections.
    for (auto& cp : WebrtcContentParents::GetAll()) {
      request->mContactList.push(cp);
    }

    auto next = request->GetNextParent();
    if (next) {
      aRv = next->SendGetStatsRequest(request->mRequestId, request->mPcIdFilter)
                ? NS_OK
                : NS_ERROR_FAILURE;
      return;
    }
  }
  // No content resident PeerConnectionCtx instances.
  // Check this process.
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    RunStatsQuery(ctx->mGetPeerConnections(), filter, nullptr,
                  request->mRequestId);
  } else {
    // Just send back an empty report.
    request->Complete();
    StatsRequest::Delete(request->mRequestId);
  }

  aRv = NS_OK;
}

MOZ_CAN_RUN_SCRIPT
static nsresult RunLogQuery(const nsCString& aPattern,
                            WebrtcGlobalChild* aThisChild,
                            const int aRequestId) {
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  if (!ctx) {
    // This process has never created a PeerConnection, so no ICE logging.
    OnGetLogging_m(aThisChild, aRequestId, Sequence<nsString>());
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsISerialEventTarget> stsThread =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!stsThread) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<MediaTransportHandler> transportHandler = ctx->GetTransportHandler();

  InvokeAsync(stsThread, __func__,
              [transportHandler, aPattern]() {
                return transportHandler->GetIceLog(aPattern);
              })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we're going to run that
          // function async anyway.
          [aRequestId, aThisChild](Sequence<nsString>&& aLogLines)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                OnGetLogging_m(aThisChild, aRequestId, std::move(aLogLines));
              },
          // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we're going to run that
          // function async anyway.
          [aRequestId, aThisChild](nsresult aError)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                OnGetLogging_m(aThisChild, aRequestId, Sequence<nsString>());
              });

  return NS_OK;
}

static nsresult RunLogClear() {
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  if (!ctx) {
    // This process has never created a PeerConnection, so no ICE logging.
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsISerialEventTarget> stsThread =
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!stsThread) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<MediaTransportHandler> transportHandler = ctx->GetTransportHandler();

  return RUN_ON_THREAD(
      stsThread,
      WrapRunnable(transportHandler, &MediaTransportHandler::ClearIceLog),
      NS_DISPATCH_NORMAL);
}

void WebrtcGlobalInformation::ClearLogging(const GlobalObject& aGlobal) {
  if (!NS_IsMainThread()) {
    return;
  }

  // Chrome-only API
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!WebrtcContentParents::Empty()) {
    // Clear content process signaling logs
    for (auto& cp : WebrtcContentParents::GetAll()) {
      Unused << cp->SendClearLogRequest();
    }
  }

  // Clear chrome process signaling logs
  Unused << RunLogClear();
}

void WebrtcGlobalInformation::GetLogging(
    const GlobalObject& aGlobal, const nsAString& aPattern,
    WebrtcGlobalLoggingCallback& aLoggingCallback, ErrorResult& aRv) {
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  LogRequestCallback callbackHandle(
      new nsMainThreadPtrHolder<WebrtcGlobalLoggingCallback>(
          "WebrtcGlobalLoggingCallback", &aLoggingCallback));

  nsAutoCString pattern;
  CopyUTF16toUTF8(aPattern, pattern);

  LogRequest* request = LogRequest::Create(callbackHandle, pattern);

  if (!request) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!WebrtcContentParents::Empty()) {
    // Pass on the request to any content based PeerConnections.
    for (auto& cp : WebrtcContentParents::GetAll()) {
      request->mContactList.push(cp);
    }

    auto next = request->GetNextParent();
    if (next) {
      aRv = next->SendGetLogRequest(request->mRequestId, request->mPattern)
                ? NS_OK
                : NS_ERROR_FAILURE;
      return;
    }
  }

  nsresult rv = RunLogQuery(request->mPattern, nullptr, request->mRequestId);

  if (NS_FAILED(rv)) {
    LogRequest::Delete(request->mRequestId);
  }

  aRv = rv;
}

static int32_t sLastSetLevel = 0;
static bool sLastAECDebug = false;
static Maybe<nsCString> sAecDebugLogDir;

void WebrtcGlobalInformation::SetDebugLevel(const GlobalObject& aGlobal,
                                            int32_t aLevel) {
  if (aLevel) {
    StartWebRtcLog(mozilla::LogLevel(aLevel));
  } else {
    StopWebRtcLog();
  }
  sLastSetLevel = aLevel;

  for (auto& cp : WebrtcContentParents::GetAll()) {
    Unused << cp->SendSetDebugMode(aLevel);
  }
}

int32_t WebrtcGlobalInformation::DebugLevel(const GlobalObject& aGlobal) {
  return sLastSetLevel;
}

void WebrtcGlobalInformation::SetAecDebug(const GlobalObject& aGlobal,
                                          bool aEnable) {
  if (aEnable) {
    sAecDebugLogDir.emplace(StartAecLog());
  } else {
    StopAecLog();
  }

  sLastAECDebug = aEnable;

  for (auto& cp : WebrtcContentParents::GetAll()) {
    Unused << cp->SendSetAecLogging(aEnable);
  }
}

bool WebrtcGlobalInformation::AecDebug(const GlobalObject& aGlobal) {
  return sLastAECDebug;
}

void WebrtcGlobalInformation::GetAecDebugLogDir(const GlobalObject& aGlobal,
                                                nsAString& aDir) {
  aDir = NS_ConvertASCIItoUTF16(sAecDebugLogDir.valueOr(EmptyCString()));
}

mozilla::ipc::IPCResult WebrtcGlobalParent::RecvGetStatsResult(
    const int& aRequestId, nsTArray<RTCStatsReportInternal>&& Stats) {
  MOZ_ASSERT(NS_IsMainThread());

  StatsRequest* request = StatsRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(LOGTAG, "Bad RequestId");
    return IPC_FAIL_NO_REASON(this);
  }

  for (auto& s : Stats) {
    request->mResult.mReports.AppendElement(s, fallible);
  }

  auto next = request->GetNextParent();
  if (next) {
    // There are more content instances to query.
    if (!next->SendGetStatsRequest(request->mRequestId, request->mPcIdFilter)) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  // Content queries complete, run chrome instance query if applicable
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    RunStatsQuery(ctx->mGetPeerConnections(), request->mPcIdFilter, nullptr,
                  aRequestId);
  } else {
    // No instance in the process, return the collections as is
    request->Complete();
    StatsRequest::Delete(aRequestId);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalParent::RecvGetLogResult(
    const int& aRequestId, const WebrtcGlobalLog& aLog) {
  MOZ_ASSERT(NS_IsMainThread());

  LogRequest* request = LogRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(LOGTAG, "Bad RequestId");
    return IPC_FAIL_NO_REASON(this);
  }
  request->mResult.AppendElements(aLog, fallible);

  auto next = request->GetNextParent();
  if (next) {
    // There are more content instances to query.
    if (!next->SendGetLogRequest(request->mRequestId, request->mPattern)) {
      return IPC_FAIL_NO_REASON(this);
    }
    return IPC_OK();
  }

  // Content queries complete, run chrome instance query if applicable
  nsresult rv = RunLogQuery(request->mPattern, nullptr, aRequestId);

  if (NS_FAILED(rv)) {
    // Unable to get gecko process log. Return what has been collected.
    CSFLogError(LOGTAG, "Unable to extract chrome process log");
    request->Complete();
    LogRequest::Delete(aRequestId);
  }

  return IPC_OK();
}

WebrtcGlobalParent* WebrtcGlobalParent::Alloc() {
  return WebrtcContentParents::Alloc();
}

bool WebrtcGlobalParent::Dealloc(WebrtcGlobalParent* aActor) {
  WebrtcContentParents::Dealloc(aActor);
  return true;
}

void WebrtcGlobalParent::ActorDestroy(ActorDestroyReason aWhy) {
  mShutdown = true;
}

mozilla::ipc::IPCResult WebrtcGlobalParent::Recv__delete__() {
  return IPC_OK();
}

MOZ_IMPLICIT WebrtcGlobalParent::WebrtcGlobalParent() : mShutdown(false) {
  MOZ_COUNT_CTOR(WebrtcGlobalParent);
}

MOZ_IMPLICIT WebrtcGlobalParent::~WebrtcGlobalParent() {
  MOZ_COUNT_DTOR(WebrtcGlobalParent);
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvGetStatsRequest(
    const int& aRequestId, const nsString& aPcIdFilter) {
  if (mShutdown) {
    return IPC_OK();
  }

  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    RunStatsQuery(ctx->mGetPeerConnections(), aPcIdFilter, this, aRequestId);
    return IPC_OK();
  }

  nsTArray<RTCStatsReportInternal> empty_stats;
  SendGetStatsResult(aRequestId, empty_stats);

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvClearStatsRequest() {
  if (mShutdown) {
    return IPC_OK();
  }

  ClearClosedStats();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvGetLogRequest(
    const int& aRequestId, const nsCString& aPattern) {
  if (mShutdown) {
    return IPC_OK();
  }

  nsresult rv = RunLogQuery(aPattern, this, aRequestId);

  if (NS_FAILED(rv)) {
    Sequence<nsString> empty_log;
    SendGetLogResult(aRequestId, empty_log);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvClearLogRequest() {
  if (mShutdown) {
    return IPC_OK();
  }

  RunLogClear();
  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvSetAecLogging(
    const bool& aEnable) {
  if (!mShutdown) {
    if (aEnable) {
      StartAecLog();
    } else {
      StopAecLog();
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcGlobalChild::RecvSetDebugMode(const int& aLevel) {
  if (!mShutdown) {
    if (aLevel) {
      StartWebRtcLog(mozilla::LogLevel(aLevel));
    } else {
      StopWebRtcLog();
    }
  }
  return IPC_OK();
}

WebrtcGlobalChild* WebrtcGlobalChild::Create() {
  WebrtcGlobalChild* child = static_cast<WebrtcGlobalChild*>(
      ContentChild::GetSingleton()->SendPWebrtcGlobalConstructor());
  return child;
}

void WebrtcGlobalChild::ActorDestroy(ActorDestroyReason aWhy) {
  mShutdown = true;
}

MOZ_IMPLICIT WebrtcGlobalChild::WebrtcGlobalChild() : mShutdown(false) {
  MOZ_COUNT_CTOR(WebrtcGlobalChild);
}

MOZ_IMPLICIT WebrtcGlobalChild::~WebrtcGlobalChild() {
  MOZ_COUNT_DTOR(WebrtcGlobalChild);
}

static void StoreLongTermICEStatisticsImpl_m(RTCStatsReportInternal* report) {
  using namespace Telemetry;

  report->mClosed = true;

  for (const auto& outboundRtpStats : report->mOutboundRtpStreamStats) {
    bool isVideo = (outboundRtpStats.mId.Value().Find("video") != -1);
    if (!isVideo) {
      continue;
    }
    if (outboundRtpStats.mBitrateMean.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_AVG_PER_CALL_KBPS,
                 uint32_t(outboundRtpStats.mBitrateMean.Value() / 1000));
    }
    if (outboundRtpStats.mBitrateStdDev.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                 uint32_t(outboundRtpStats.mBitrateStdDev.Value() / 1000));
    }
    if (outboundRtpStats.mFramerateMean.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_AVG_PER_CALL,
                 uint32_t(outboundRtpStats.mFramerateMean.Value()));
    }
    if (outboundRtpStats.mFramerateStdDev.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                 uint32_t(outboundRtpStats.mFramerateStdDev.Value() * 10));
    }
    if (outboundRtpStats.mDroppedFrames.WasPassed() &&
        report->mCallDurationMs.WasPassed()) {
      double mins = report->mCallDurationMs.Value() / (1000 * 60);
      if (mins > 0) {
        Accumulate(
            WEBRTC_VIDEO_ENCODER_DROPPED_FRAMES_PER_CALL_FPM,
            uint32_t(double(outboundRtpStats.mDroppedFrames.Value()) / mins));
      }
    }
  }

  for (const auto& inboundRtpStats : report->mInboundRtpStreamStats) {
    bool isVideo = (inboundRtpStats.mId.Value().Find("video") != -1);
    if (!isVideo) {
      continue;
    }
    if (inboundRtpStats.mBitrateMean.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_AVG_PER_CALL_KBPS,
                 uint32_t(inboundRtpStats.mBitrateMean.Value() / 1000));
    }
    if (inboundRtpStats.mBitrateStdDev.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                 uint32_t(inboundRtpStats.mBitrateStdDev.Value() / 1000));
    }
    if (inboundRtpStats.mFramerateMean.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_AVG_PER_CALL,
                 uint32_t(inboundRtpStats.mFramerateMean.Value()));
    }
    if (inboundRtpStats.mFramerateStdDev.WasPassed()) {
      Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                 uint32_t(inboundRtpStats.mFramerateStdDev.Value() * 10));
    }
    if (inboundRtpStats.mDiscardedPackets.WasPassed() &&
        report->mCallDurationMs.WasPassed()) {
      double mins = report->mCallDurationMs.Value() / (1000 * 60);
      if (mins > 0) {
        Accumulate(
            WEBRTC_VIDEO_DECODER_DISCARDED_PACKETS_PER_CALL_PPM,
            uint32_t(double(inboundRtpStats.mDiscardedPackets.Value()) / mins));
      }
    }
  }

  // Finally, store the stats

  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  if (ctx) {
    ctx->mStatsForClosedPeerConnections.AppendElement(*report, fallible);
  }
}

void WebrtcGlobalInformation::StoreLongTermICEStatistics(
    PeerConnectionImpl& aPc) {
  if (aPc.IceConnectionState() == RTCIceConnectionState::New) {
    // ICE has not started; we won't have any remote candidates, so recording
    // statistics on gathered candidates is pointless.
    return;
  }

  aPc.GetStats(nullptr, true)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [=](UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
            StoreLongTermICEStatisticsImpl_m(aReport.get());
          },
          [=](nsresult aError) {});
}

}  // namespace dom
}  // namespace mozilla

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

#include "CSFLog.h"
#include "WebRtcLog.h"
#include "mozilla/dom/WebrtcGlobalInformationBinding.h"
#include "mozilla/dom/ContentChild.h"

#include "nsAutoPtr.h"
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

typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;
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
    using RealCallbackType =
        typename RemovePointer<decltype(mCallback.get())>::Type;
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
      : RequestManager(aId, aCallback), mPcIdFilter(aFilter) {
    mResult.mReports.Construct();
  }

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
static void OnStatsReport_m(WebrtcGlobalChild* aThisChild, const int aRequestId,
                            nsTArray<UniquePtr<RTCStatsQuery>>&& aQueryList) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aThisChild) {
    Stats stats;

    // Copy stats generated for the currently active PeerConnections
    for (auto& query : aQueryList) {
      if (query) {
        stats.AppendElement(*query->report);
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

  for (auto& query : aQueryList) {
    if (query) {
      request->mResult.mReports.Value().AppendElement(*(query->report),
                                                      fallible);
    }
  }

  // Reports saved for closed/destroyed PeerConnections
  auto ctx = PeerConnectionCtx::GetInstance();
  if (ctx) {
    for (auto&& pc : ctx->mStatsForClosedPeerConnections) {
      request->mResult.mReports.Value().AppendElement(pc, fallible);
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
  nsTArray<RefPtr<RTCStatsQueryPromise>> promises;

  for (auto& idAndPc : aPeerConnections) {
    MOZ_ASSERT(idAndPc.second);
    PeerConnectionImpl& pc = *idAndPc.second;
    if (aPcIdFilter.IsEmpty() ||
        aPcIdFilter.EqualsASCII(pc.GetIdAsAscii().c_str())) {
      if (pc.HasMedia()) {
        promises.AppendElement(
            pc.GetStats(nullptr, true, false)
                ->Then(
                    GetMainThreadSerialEventTarget(), __func__,
                    [=](UniquePtr<RTCStatsQuery>&& aQuery) {
                      return RTCStatsQueryPromise::CreateAndResolve(
                          std::move(aQuery), __func__);
                    },
                    [=](nsresult aError) {
                      // Ignore errors! Just resolve with a nullptr.
                      return RTCStatsQueryPromise::CreateAndResolve(
                          UniquePtr<RTCStatsQuery>(), __func__);
                    }));
      }
    }
  }

  RTCStatsQueryPromise::All(GetMainThreadSerialEventTarget(), promises)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          // MOZ_CAN_RUN_SCRIPT_BOUNDARY because we're going to run that
          // function async anyway.
          [aThisChild,
           aRequestId](nsTArray<UniquePtr<RTCStatsQuery>>&& aQueries)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY {
                OnStatsReport_m(aThisChild, aRequestId, std::move(aQueries));
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
  nsCOMPtr<nsIEventTarget> stsThread =
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
    request->mResult.mReports.Value().AppendElement(s, fallible);
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

struct StreamResult {
  StreamResult() : candidateTypeBitpattern(0), streamSucceeded(false) {}
  uint32_t candidateTypeBitpattern;
  bool streamSucceeded;
};

static uint32_t GetCandidateIpAndTransportMask(
    const RTCIceCandidateStats* cand) {
  enum {
    CANDIDATE_BITMASK_UDP = 1,
    CANDIDATE_BITMASK_TCP = 1 << 1,
    CANDIDATE_BITMASK_IPV6 = 1 << 2,
  };

  uint32_t res = 0;

  nsAutoCString transport;
  // prefer relay transport for local relay candidates
  if (cand->mRelayProtocol.WasPassed()) {
    transport.Assign(NS_ConvertUTF16toUTF8(cand->mRelayProtocol.Value()));
  } else {
    transport.Assign(NS_ConvertUTF16toUTF8(cand->mProtocol.Value()));
  }
  if (transport == kNrIceTransportUdp) {
    res |= CANDIDATE_BITMASK_UDP;
  } else if (transport == kNrIceTransportTcp) {
    res |= CANDIDATE_BITMASK_TCP;
  }

  if (cand->mAddress.Value().FindChar(':') != -1) {
    res |= CANDIDATE_BITMASK_IPV6;
  }

  return res;
};

static void StoreLongTermICEStatisticsImpl_m(nsresult result,
                                             RTCStatsQuery* query) {
  using namespace Telemetry;

  if (NS_FAILED(result) || !query->report->mIceCandidateStats.WasPassed()) {
    return;
  }

  query->report->mClosed.Construct(true);

  // TODO(bcampen@mozilla.com): Do we need to watch out for cases where the
  // components within a stream didn't have the same types of relayed
  // candidates? I have a feeling that late trickle could cause this, but right
  // now we don't have enough information to detect it (we would need to know
  // the ICE component id for each candidate pair and candidate)

  std::map<std::string, StreamResult> streamResults;

  // Build list of streams, and whether or not they failed.
  for (size_t i = 0; i < query->report->mIceCandidatePairStats.Value().Length();
       ++i) {
    const RTCIceCandidatePairStats& pair =
        query->report->mIceCandidatePairStats.Value()[i];

    if (!pair.mState.WasPassed() || !pair.mTransportId.WasPassed()) {
      MOZ_CRASH();
      continue;
    }

    // Note: we use NrIceMediaStream's name for the
    // RTCIceCandidatePairStats tranportId
    std::string streamId(
        NS_ConvertUTF16toUTF8(pair.mTransportId.Value()).get());

    streamResults[streamId].streamSucceeded |=
        pair.mState.Value() == RTCStatsIceCandidatePairState::Succeeded;
  }

  for (size_t i = 0; i < query->report->mIceCandidateStats.Value().Length();
       ++i) {
    const RTCIceCandidateStats& cand =
        query->report->mIceCandidateStats.Value()[i];

    if (!cand.mType.WasPassed() || !cand.mCandidateType.WasPassed() ||
        !cand.mProtocol.WasPassed() || !cand.mAddress.WasPassed() ||
        !cand.mTransportId.WasPassed()) {
      // Crash on debug, ignore this candidate otherwise.
      MOZ_CRASH();
      continue;
    }

    /* The bitmask after examaning a candidate should look like this:
     * REMOTE_GATHERED_HOST_UDP = 1,
     * REMOTE_GATHERED_HOST_TCP = 1 << 1,
     * REMOTE_GATHERED_HOST_IPV6 = 1 << 2,
     * REMOTE_GATHERED_SERVER_REFLEXIVE_UDP = 1 << 3,
     * REMOTE_GATHERED_SERVER_REFLEXIVE_TCP = 1 << 4,
     * REMOTE_GATHERED_SERVER_REFLEXIVE_IPV6 = 1 << 5,
     * REMOTE_GATHERED_TURN_UDP = 1 << 6,
     * REMOTE_GATHERED_TURN_TCP = 1 << 7, // dummy place holder
     * REMOTE_GATHERED_TURN_IPV6 = 1 << 8,
     * REMOTE_GATHERED_PEER_REFLEXIVE_UDP = 1 << 9,
     * REMOTE_GATHERED_PEER_REFLEXIVE_TCP = 1 << 10,
     * REMOTE_GATHERED_PEER_REFLEXIVE_IPV6 = 1 << 11,
     * LOCAL_GATHERED_HOST_UDP = 1 << 16,
     * LOCAL_GATHERED_HOST_TCP = 1 << 17,
     * LOCAL_GATHERED_HOST_IPV6 = 1 << 18,
     * LOCAL_GATHERED_SERVER_REFLEXIVE_UDP = 1 << 19,
     * LOCAL_GATHERED_SERVER_REFLEXIVE_TCP = 1 << 20,
     * LOCAL_GATHERED_SERVER_REFLEXIVE_IPV6 = 1 << 21,
     * LOCAL_GATHERED_TURN_UDP = 1 << 22,
     * LOCAL_GATHERED_TURN_TCP = 1 << 23,
     * LOCAL_GATHERED_TURN_IPV6 = 1 << 24,
     * LOCAL_GATHERED_PEERREFLEXIVE_UDP = 1 << 25,
     * LOCAL_GATHERED_PEERREFLEXIVE_TCP = 1 << 26,
     * LOCAL_GATHERED_PEERREFLEXIVE_IPV6 = 1 << 27,
     *
     * This results in following shift values
     */
    static const uint32_t kLocalShift = 16;
    static const uint32_t kSrflxShift = 3;
    static const uint32_t kRelayShift = 6;
    static const uint32_t kPrflxShift = 9;

    uint32_t candBitmask = GetCandidateIpAndTransportMask(&cand);

    // Note: shift values need to result in the above enum table
    if (cand.mType.Value() == RTCStatsType::Local_candidate) {
      candBitmask <<= kLocalShift;
    }

    if (cand.mCandidateType.Value() == RTCIceCandidateType::Srflx) {
      candBitmask <<= kSrflxShift;
    } else if (cand.mCandidateType.Value() == RTCIceCandidateType::Relay) {
      candBitmask <<= kRelayShift;
    } else if (cand.mCandidateType.Value() == RTCIceCandidateType::Prflx) {
      candBitmask <<= kPrflxShift;
    }

    // Note: this is not a "transport", this is really a stream ID. This is just
    // the way the stats API is standardized right now.
    // Very confusing.
    std::string streamId(
        NS_ConvertUTF16toUTF8(cand.mTransportId.Value()).get());

    streamResults[streamId].candidateTypeBitpattern |= candBitmask;
  }

  for (auto& streamResult : streamResults) {
    Telemetry::RecordWebrtcIceCandidates(
        streamResult.second.candidateTypeBitpattern,
        streamResult.second.streamSucceeded);
  }

  // Beyond ICE, accumulate telemetry for various PER_CALL settings here.

  if (query->report->mOutboundRtpStreamStats.WasPassed()) {
    auto& array = query->report->mOutboundRtpStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo) {
        continue;
      }
      if (s.mBitrateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_AVG_PER_CALL_KBPS,
                   uint32_t(s.mBitrateMean.Value() / 1000));
      }
      if (s.mBitrateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                   uint32_t(s.mBitrateStdDev.Value() / 1000));
      }
      if (s.mFramerateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_AVG_PER_CALL,
                   uint32_t(s.mFramerateMean.Value()));
      }
      if (s.mFramerateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_ENCODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                   uint32_t(s.mFramerateStdDev.Value() * 10));
      }
      if (s.mDroppedFrames.WasPassed() && !query->iceStartTime.IsNull()) {
        double mins = (TimeStamp::Now() - query->iceStartTime).ToSeconds() / 60;
        if (mins > 0) {
          Accumulate(WEBRTC_VIDEO_ENCODER_DROPPED_FRAMES_PER_CALL_FPM,
                     uint32_t(double(s.mDroppedFrames.Value()) / mins));
        }
      }
    }
  }

  if (query->report->mInboundRtpStreamStats.WasPassed()) {
    auto& array = query->report->mInboundRtpStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo) {
        continue;
      }
      if (s.mBitrateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_AVG_PER_CALL_KBPS,
                   uint32_t(s.mBitrateMean.Value() / 1000));
      }
      if (s.mBitrateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_BITRATE_STD_DEV_PER_CALL_KBPS,
                   uint32_t(s.mBitrateStdDev.Value() / 1000));
      }
      if (s.mFramerateMean.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_AVG_PER_CALL,
                   uint32_t(s.mFramerateMean.Value()));
      }
      if (s.mFramerateStdDev.WasPassed()) {
        Accumulate(WEBRTC_VIDEO_DECODER_FRAMERATE_10X_STD_DEV_PER_CALL,
                   uint32_t(s.mFramerateStdDev.Value() * 10));
      }
      if (s.mDiscardedPackets.WasPassed() && !query->iceStartTime.IsNull()) {
        double mins = (TimeStamp::Now() - query->iceStartTime).ToSeconds() / 60;
        if (mins > 0) {
          Accumulate(WEBRTC_VIDEO_DECODER_DISCARDED_PACKETS_PER_CALL_PPM,
                     uint32_t(double(s.mDiscardedPackets.Value()) / mins));
        }
      }
    }
  }

  // Finally, store the stats

  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  if (ctx) {
    ctx->mStatsForClosedPeerConnections.AppendElement(*query->report, fallible);
  }
}

void WebrtcGlobalInformation::StoreLongTermICEStatistics(
    PeerConnectionImpl& aPc) {
  Telemetry::Accumulate(Telemetry::WEBRTC_ICE_FINAL_CONNECTION_STATE,
                        static_cast<uint32_t>(aPc.IceConnectionState()));

  if (aPc.IceConnectionState() == PCImplIceConnectionState::New) {
    // ICE has not started; we won't have any remote candidates, so recording
    // statistics on gathered candidates is pointless.
    return;
  }

  aPc.GetStats(nullptr, true, false)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [=](UniquePtr<RTCStatsQuery>&& aQuery) {
            StoreLongTermICEStatisticsImpl_m(NS_OK, aQuery.get());
          },
          [=](nsresult aError) {
            StoreLongTermICEStatisticsImpl_m(aError, nullptr);
          });
}

}  // namespace dom
}  // namespace mozilla

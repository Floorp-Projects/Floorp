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
#include "nsNetCID.h" // NS_SOCKETTRANSPORTSERVICE_CONTRACTID
#include "nsServiceManagerUtils.h" // do_GetService
#include "mozilla/ErrorResult.h"
#include "mozilla/Vector.h"
#include "nsProxyRelease.h"
#include "mozilla/Telemetry.h"
#include "mozilla/unused.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/RefPtr.h"

#include "rlogringbuffer.h"
#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "webrtc/system_wrappers/interface/trace.h"

static const char* logTag = "WebrtcGlobalInformation";

namespace mozilla {
namespace dom {

typedef Vector<nsAutoPtr<RTCStatsQuery>> RTCStatsQueries;
typedef nsTArray<RTCStatsReportInternal> Stats;

template<class Request, typename Callback,
         typename Result, typename QueryParam>
class RequestManager
{
public:

  static Request* Create(Callback& aCallback, QueryParam& aParam)
  {
    mozilla::StaticMutexAutoLock lock(sMutex);

    int id = ++sLastRequestId;
    auto result = sRequests.insert(
      std::make_pair(id, Request(id, aCallback, aParam)));

    if (!result.second) {
      return nullptr;
    }

    return &result.first->second;
  }

  static void Delete(int aId)
  {
    mozilla::StaticMutexAutoLock lock(sMutex);
    sRequests.erase(aId);
  }

  static Request* Get(int aId)
  {
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

  RefPtr<WebrtcGlobalParent> GetNextParent()
  {
    while (!mContactList.empty()) {
      RefPtr<WebrtcGlobalParent> next = mContactList.front();
      mContactList.pop();
      if (next->IsActive()) {
        return next;
      }
    }

    return nullptr;
  }

  void Complete()
  {
    ErrorResult rv;
    mCallback.get()->Call(mResult, rv);

    if (rv.Failed()) {
      CSFLogError(logTag, "Error firing stats observer callback");
    }
  }

protected:
  // The mutex is used to protect two related operations involving the sRequest map
  // and the sLastRequestId. For the map, it prevents more than one thread from
  // adding or deleting map entries at the same time. For id generation,
  // it creates an atomic allocation and increment.
  static mozilla::StaticMutex sMutex;
  static std::map<int, Request> sRequests;
  static int sLastRequestId;

  Callback mCallback;

  explicit RequestManager(int aId, Callback& aCallback)
    : mRequestId(aId)
    , mCallback(aCallback)
  {}
  ~RequestManager() {}
private:

  RequestManager() = delete;
  RequestManager& operator=(const RequestManager&) = delete;
};

template<class Request, typename Callback,
         typename Result, typename QueryParam>
mozilla::StaticMutex RequestManager<Request, Callback, Result, QueryParam>::sMutex;
template<class Request, typename Callback,
         typename Result, typename QueryParam>
std::map<int, Request> RequestManager<Request, Callback, Result, QueryParam>::sRequests;
template<class Request, typename Callback,
         typename Result, typename QueryParam>
int RequestManager<Request, Callback, Result, QueryParam>::sLastRequestId;

typedef nsMainThreadPtrHandle<WebrtcGlobalStatisticsCallback> StatsRequestCallback;

class StatsRequest
  : public RequestManager<StatsRequest,
                          StatsRequestCallback,
                          WebrtcGlobalStatisticsReport,
                          nsAString>
{
public:
  const nsString mPcIdFilter;
  explicit StatsRequest(int aId, StatsRequestCallback& aCallback, nsAString& aFilter)
    : RequestManager(aId, aCallback)
    , mPcIdFilter(aFilter)
  {
    mResult.mReports.Construct();
  }

private:
  StatsRequest() = delete;
  StatsRequest& operator=(const StatsRequest&) = delete;
};

typedef nsMainThreadPtrHandle<WebrtcGlobalLoggingCallback> LogRequestCallback;

class LogRequest
  : public RequestManager<LogRequest,
                          LogRequestCallback,
                          Sequence<nsString>,
                          const nsACString>
{
public:
  const nsCString mPattern;
  explicit LogRequest(int aId, LogRequestCallback& aCallback, const nsACString& aPattern)
    : RequestManager(aId, aCallback)
    , mPattern(aPattern)
  {}

private:
  LogRequest() = delete;
  LogRequest& operator=(const LogRequest&) = delete;
};

class WebrtcContentParents
{
public:
  static WebrtcGlobalParent* Alloc();
  static void Dealloc(WebrtcGlobalParent* aParent);
  static bool Empty()
  {
    return sContentParents.empty();
  }
  static const std::vector<RefPtr<WebrtcGlobalParent>>& GetAll()
  {
    return sContentParents;
  }
private:
  static std::vector<RefPtr<WebrtcGlobalParent>> sContentParents;
  WebrtcContentParents() = delete;
  WebrtcContentParents(const WebrtcContentParents&) = delete;
  WebrtcContentParents& operator=(const WebrtcContentParents&) = delete;
};

std::vector<RefPtr<WebrtcGlobalParent>> WebrtcContentParents::sContentParents;

WebrtcGlobalParent* WebrtcContentParents::Alloc()
{
  RefPtr<WebrtcGlobalParent> cp = new WebrtcGlobalParent;
  sContentParents.push_back(cp);
  return cp.get();
}

void WebrtcContentParents::Dealloc(WebrtcGlobalParent* aParent)
{
  if (aParent) {
    aParent->mShutdown = true;
    auto cp = std::find(sContentParents.begin(), sContentParents.end(), aParent);
    if (cp != sContentParents.end()) {
      sContentParents.erase(cp);
    }
  }
}

static PeerConnectionCtx* GetPeerConnectionCtx()
{
  if(PeerConnectionCtx::isActive()) {
    MOZ_ASSERT(PeerConnectionCtx::GetInstance());
    return PeerConnectionCtx::GetInstance();
  }
  return nullptr;
}

static void
OnStatsReport_m(WebrtcGlobalChild* aThisChild,
                const int aRequestId,
                nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aQueryList);

  if (aThisChild) {
    Stats stats;

    // Copy stats generated for the currently active PeerConnections
    for (auto&& query : *aQueryList) {
      stats.AppendElement(*(query->report));
    }
    // Reports saved for closed/destroyed PeerConnections
    auto ctx = PeerConnectionCtx::GetInstance();
    if (ctx) {
      for (auto&& pc : ctx->mStatsForClosedPeerConnections) {
        stats.AppendElement(pc);
      }
    }

    unused << aThisChild->SendGetStatsResult(aRequestId, stats);
    return;
  }

  // This is the last stats report to be collected. (Must be the gecko process).
  MOZ_ASSERT(XRE_IsParentProcess());

  StatsRequest* request = StatsRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(logTag, "Bad RequestId");
    return;
  }

  for (auto&& query : *aQueryList) {
    request->mResult.mReports.Value().AppendElement(*(query->report), fallible);
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

static void
GetAllStats_s(WebrtcGlobalChild* aThisChild,
              const int aRequestId,
              nsAutoPtr<RTCStatsQueries> aQueryList)
{
  MOZ_ASSERT(aQueryList);
  // The call to PeerConnetionImpl must happen on the from a runnable
  // dispatched on the STS thread.

  // Get stats from active connections.
  for (auto&& query : *aQueryList) {
    PeerConnectionImpl::ExecuteStatsQuery_s(query);
  }

  // After the RTCStatsQueries have been filled in, control must return
  // to the main thread before their eventual destruction.
  NS_DispatchToMainThread(WrapRunnableNM(&OnStatsReport_m,
                                         aThisChild,
                                         aRequestId,
                                         aQueryList),
                          NS_DISPATCH_NORMAL);
}

static void OnGetLogging_m(WebrtcGlobalChild* aThisChild,
                           const int aRequestId,
                           nsAutoPtr<std::deque<std::string>> aLogList)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aThisChild) {
    // Add this log to the collection of logs and call into
    // the next content process.
    Sequence<nsString> nsLogs;

    if (!aLogList->empty()) {
      for (auto& line : *aLogList) {
        nsLogs.AppendElement(NS_ConvertUTF8toUTF16(line.c_str()), fallible);
      }
      nsLogs.AppendElement(NS_LITERAL_STRING("+++++++ END ++++++++"), fallible);
    }

    unused << aThisChild->SendGetLogResult(aRequestId, nsLogs);
    return;
  }

  // This is the last log to be collected. (Must be the gecko process).
  MOZ_ASSERT(XRE_IsParentProcess());

  LogRequest* request = LogRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(logTag, "Bad RequestId");
    return;
  }

  if (!aLogList->empty()) {
    for (auto& line : *aLogList) {
      request->mResult.AppendElement(NS_ConvertUTF8toUTF16(line.c_str()),
                                     fallible);
    }
    request->mResult.AppendElement(NS_LITERAL_STRING("+++++++ END ++++++++"),
                                   fallible);
  }

  request->Complete();
  LogRequest::Delete(aRequestId);
}

static void GetLogging_s(WebrtcGlobalChild* aThisChild,
                         const int aRequestId,
                         const std::string& aPattern)
{
  // Request log while not on the main thread.
  RLogRingBuffer* logs = RLogRingBuffer::GetInstance();
  nsAutoPtr<std::deque<std::string>> result(new std::deque<std::string>);
  // Might not exist yet.
  if (logs) {
    logs->Filter(aPattern, 0, result);
  }
  // Return to main thread to complete processing.
  NS_DispatchToMainThread(WrapRunnableNM(&OnGetLogging_m,
                                         aThisChild,
                                         aRequestId,
                                         result),
                          NS_DISPATCH_NORMAL);
}

static nsresult
BuildStatsQueryList(
  const std::map<const std::string, PeerConnectionImpl *>& aPeerConnections,
  const nsAString& aPcIdFilter,
  RTCStatsQueries* queries)
{
  nsresult rv;

  for (auto&& pc : aPeerConnections) {
    MOZ_ASSERT(pc.second);
    if (aPcIdFilter.IsEmpty() ||
        aPcIdFilter.EqualsASCII(pc.second->GetIdAsAscii().c_str())) {
      if (pc.second->HasMedia()) {
        queries->append(nsAutoPtr<RTCStatsQuery>(new RTCStatsQuery(true)));
        rv = pc.second->BuildStatsQuery_m(nullptr, queries->back()); // all tracks
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        MOZ_ASSERT(queries->back()->report);
      }
    }
  }

  return NS_OK;
}

static nsresult
RunStatsQuery(
  const std::map<const std::string, PeerConnectionImpl *>& aPeerConnections,
  const nsAString& aPcIdFilter,
  WebrtcGlobalChild* aThisChild,
  const int aRequestId)
{
  nsAutoPtr<RTCStatsQueries> queries(new RTCStatsQueries);
  nsresult rv = BuildStatsQueryList(aPeerConnections, aPcIdFilter, queries);

  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  } else if (!stsThread) {
    return NS_ERROR_FAILURE;
  }

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetAllStats_s,
                                    aThisChild,
                                    aRequestId,
                                    queries),
                     NS_DISPATCH_NORMAL);
  return rv;
}

void
WebrtcGlobalInformation::GetAllStats(
  const GlobalObject& aGlobal,
  WebrtcGlobalStatisticsCallback& aStatsCallback,
  const Optional<nsAString>& pcIdFilter,
  ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    aRv.Throw(NS_ERROR_NOT_SAME_THREAD);
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  StatsRequestCallback callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalStatisticsCallback>(&aStatsCallback));

  nsString filter;
  if (pcIdFilter.WasPassed()) {
    filter = pcIdFilter.Value();
  }

  StatsRequest* request = StatsRequest::Create(callbackHandle, filter);

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
      aRv = next->SendGetStatsRequest(request->mRequestId, request->mPcIdFilter) ?
                NS_OK : NS_ERROR_FAILURE;
      return;
    }
  }
  // No content resident PeerConnectionCtx instances.
  // Check this process.
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();
  nsresult rv;

  if (ctx) {
    rv = RunStatsQuery(ctx->mGetPeerConnections(),
                       filter, nullptr, request->mRequestId);

    if (NS_FAILED(rv)) {
      StatsRequest::Delete(request->mRequestId);
    }
  } else {
    // Just send back an empty report.
    rv = NS_OK;
    request->Complete();
    StatsRequest::Delete(request->mRequestId);
  }

  aRv = rv;
  return;
}

static nsresult
RunLogQuery(const nsCString& aPattern,
            WebrtcGlobalChild* aThisChild,
            const int aRequestId)
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  } else if (!stsThread) {
    return NS_ERROR_FAILURE;
  }

  rv = RUN_ON_THREAD(stsThread,
                     WrapRunnableNM(&GetLogging_s,
                                    aThisChild,
                                    aRequestId,
                                    aPattern.get()),
                     NS_DISPATCH_NORMAL);
  return rv;
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

  MOZ_ASSERT(XRE_IsParentProcess());

  // CallbackObject does not support threadsafe refcounting, and must be
  // destroyed on main.
  LogRequestCallback callbackHandle(
    new nsMainThreadPtrHolder<WebrtcGlobalLoggingCallback>(&aLoggingCallback));

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
      aRv = next->SendGetLogRequest(request->mRequestId, request->mPattern) ?
                NS_OK : NS_ERROR_FAILURE;
      return;
    }
  }

  nsresult rv = RunLogQuery(request->mPattern, nullptr, request->mRequestId);

  if (NS_FAILED(rv)) {
    LogRequest::Delete(request->mRequestId);
  }

  aRv = rv;
  return;
}

static int32_t sLastSetLevel = 0;
static bool sLastAECDebug = false;

void
WebrtcGlobalInformation::SetDebugLevel(const GlobalObject& aGlobal, int32_t aLevel)
{
  StartWebRtcLog(webrtc::TraceLevel(aLevel));
  sLastSetLevel = aLevel;

  for (auto& cp : WebrtcContentParents::GetAll()){
    unused << cp->SendSetDebugMode(aLevel);
  }
}

int32_t
WebrtcGlobalInformation::DebugLevel(const GlobalObject& aGlobal)
{
  return sLastSetLevel;
}

void
WebrtcGlobalInformation::SetAecDebug(const GlobalObject& aGlobal, bool aEnable)
{
  StartWebRtcLog(sLastSetLevel); // to make it read the aec path
  webrtc::Trace::set_aec_debug(aEnable);
  sLastAECDebug = aEnable;

  for (auto& cp : WebrtcContentParents::GetAll()){
    unused << cp->SendSetAecLogging(aEnable);
  }
}

bool
WebrtcGlobalInformation::AecDebug(const GlobalObject& aGlobal)
{
  return sLastAECDebug;
}

bool
WebrtcGlobalParent::RecvGetStatsResult(const int& aRequestId,
                                       nsTArray<RTCStatsReportInternal>&& Stats)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = NS_OK;

  StatsRequest* request = StatsRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(logTag, "Bad RequestId");
    return false;
  }

  for (auto&& s : Stats) {
    request->mResult.mReports.Value().AppendElement(s, fallible);
  }

  auto next = request->GetNextParent();
  if (next) {
    // There are more content instances to query.
    return next->SendGetStatsRequest(request->mRequestId, request->mPcIdFilter);
  }

  // Content queries complete, run chrome instance query if applicable
  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    rv = RunStatsQuery(ctx->mGetPeerConnections(),
                       request->mPcIdFilter, nullptr, aRequestId);
  } else {
    // No instance in the process, return the collections as is
    request->Complete();
    StatsRequest::Delete(aRequestId);
  }

  return NS_SUCCEEDED(rv);
}

bool
WebrtcGlobalParent::RecvGetLogResult(const int& aRequestId,
                                     const WebrtcGlobalLog& aLog)
{
  MOZ_ASSERT(NS_IsMainThread());

  LogRequest* request = LogRequest::Get(aRequestId);

  if (!request) {
    CSFLogError(logTag, "Bad RequestId");
    return false;
  }
  request->mResult.AppendElements(aLog, fallible);

  auto next = request->GetNextParent();
  if (next) {
    // There are more content instances to query.
    return next->SendGetLogRequest(request->mRequestId, request->mPattern);
  }

  // Content queries complete, run chrome instance query if applicable
  nsresult rv = RunLogQuery(request->mPattern, nullptr, aRequestId);

  if (NS_FAILED(rv)) {
    //Unable to get gecko process log. Return what has been collected.
    CSFLogError(logTag, "Unable to extract chrome process log");
    request->Complete();
    LogRequest::Delete(aRequestId);
  }

  return true;
}

WebrtcGlobalParent*
WebrtcGlobalParent::Alloc()
{
  return WebrtcContentParents::Alloc();
}

bool
WebrtcGlobalParent::Dealloc(WebrtcGlobalParent * aActor)
{
  WebrtcContentParents::Dealloc(aActor);
  return true;
}

void
WebrtcGlobalParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mShutdown = true;
  return;
}

bool
WebrtcGlobalParent::Recv__delete__()
{
  return true;
}

MOZ_IMPLICIT WebrtcGlobalParent::WebrtcGlobalParent()
  : mShutdown(false)
{
  MOZ_COUNT_CTOR(WebrtcGlobalParent);
}

MOZ_IMPLICIT WebrtcGlobalParent::~WebrtcGlobalParent()
{
  MOZ_COUNT_DTOR(WebrtcGlobalParent);
}

bool
WebrtcGlobalChild::RecvGetStatsRequest(const int& aRequestId,
                                       const nsString& aPcIdFilter)
{
  if (mShutdown) {
    return true;
  }

  PeerConnectionCtx* ctx = GetPeerConnectionCtx();

  if (ctx) {
    nsresult rv = RunStatsQuery(ctx->mGetPeerConnections(),
                                aPcIdFilter, this, aRequestId);
    return NS_SUCCEEDED(rv);
  }

  nsTArray<RTCStatsReportInternal> empty_stats;
  SendGetStatsResult(aRequestId, empty_stats);

  return true;
}

bool
WebrtcGlobalChild::RecvGetLogRequest(const int& aRequestId,
                                     const nsCString& aPattern)
{
  if (mShutdown) {
    return true;
  }

  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsThread =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_SUCCEEDED(rv) && stsThread) {
    rv = RUN_ON_THREAD(stsThread,
                       WrapRunnableNM(&GetLogging_s, this, aRequestId, aPattern.get()),
                       NS_DISPATCH_NORMAL);

    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

  Sequence<nsString> empty_log;
  SendGetLogResult(aRequestId, empty_log);

  return true;
}

bool
WebrtcGlobalChild::RecvSetAecLogging(const bool& aEnable)
{
  if (!mShutdown) {
    StartWebRtcLog(sLastSetLevel); // to make it read the aec path
    webrtc::Trace::set_aec_debug(aEnable);
  }
  return true;
}

bool
WebrtcGlobalChild::RecvSetDebugMode(const int& aLevel)
{
  if (!mShutdown) {
    StartWebRtcLog(webrtc::TraceLevel(aLevel));
  }
  return true;
}

WebrtcGlobalChild*
WebrtcGlobalChild::Create()
{
  WebrtcGlobalChild* child =
    static_cast<WebrtcGlobalChild*>(
      ContentChild::GetSingleton()->SendPWebrtcGlobalConstructor());
  return child;
}

void
WebrtcGlobalChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mShutdown = true;
}

MOZ_IMPLICIT WebrtcGlobalChild::WebrtcGlobalChild()
  : mShutdown(false)
{
  MOZ_COUNT_CTOR(WebrtcGlobalChild);
}

MOZ_IMPLICIT WebrtcGlobalChild::~WebrtcGlobalChild()
{
  MOZ_COUNT_DTOR(WebrtcGlobalChild);
}

struct StreamResult {
  StreamResult() : candidateTypeBitpattern(0), streamSucceeded(false) {}
  uint8_t candidateTypeBitpattern;
  bool streamSucceeded;
};

static void StoreLongTermICEStatisticsImpl_m(
    nsresult result,
    nsAutoPtr<RTCStatsQuery> query) {

  using namespace Telemetry;

  if (NS_FAILED(result) ||
      !query->error.empty() ||
      !query->report->mIceCandidateStats.WasPassed()) {
    return;
  }

  query->report->mClosed.Construct(true);

  // First, store stuff in telemetry
  enum {
    REMOTE_GATHERED_SERVER_REFLEXIVE = 1,
    REMOTE_GATHERED_TURN = 1 << 1,
    LOCAL_GATHERED_SERVER_REFLEXIVE = 1 << 2,
    LOCAL_GATHERED_TURN_UDP = 1 << 3,
    LOCAL_GATHERED_TURN_TCP = 1 << 4,
    LOCAL_GATHERED_TURN_TLS = 1 << 5,
    LOCAL_GATHERED_TURN_HTTPS = 1 << 6,
  };

  // TODO(bcampen@mozilla.com): Do we need to watch out for cases where the
  // components within a stream didn't have the same types of relayed
  // candidates? I have a feeling that late trickle could cause this, but right
  // now we don't have enough information to detect it (we would need to know
  // the ICE component id for each candidate pair and candidate)

  std::map<std::string, StreamResult> streamResults;

  // Build list of streams, and whether or not they failed.
  for (size_t i = 0;
       i < query->report->mIceCandidatePairStats.Value().Length();
       ++i) {
    const RTCIceCandidatePairStats &pair =
      query->report->mIceCandidatePairStats.Value()[i];

    if (!pair.mState.WasPassed() || !pair.mComponentId.WasPassed()) {
      MOZ_CRASH();
      continue;
    }

    // Note: this is not a "component" in the ICE definition, this is really a
    // stream ID. This is just the way the stats API is standardized right now.
    // Very confusing.
    std::string streamId(
      NS_ConvertUTF16toUTF8(pair.mComponentId.Value()).get());

    streamResults[streamId].streamSucceeded |=
      pair.mState.Value() == RTCStatsIceCandidatePairState::Succeeded;
  }

  for (size_t i = 0;
       i < query->report->mIceCandidateStats.Value().Length();
       ++i) {
    const RTCIceCandidateStats &cand =
      query->report->mIceCandidateStats.Value()[i];

    if (!cand.mType.WasPassed() ||
        !cand.mCandidateType.WasPassed() ||
        !cand.mComponentId.WasPassed()) {
      // Crash on debug, ignore this candidate otherwise.
      MOZ_CRASH();
      continue;
    }

    // Note: this is not a "component" in the ICE definition, this is really a
    // stream ID. This is just the way the stats API is standardized right now
    // Very confusing.
    std::string streamId(
      NS_ConvertUTF16toUTF8(cand.mComponentId.Value()).get());

    if (cand.mCandidateType.Value() == RTCStatsIceCandidateType::Relayed) {
      if (cand.mType.Value() == RTCStatsType::Localcandidate) {
        NS_ConvertUTF16toUTF8 transport(cand.mMozLocalTransport.Value());
        if (transport == kNrIceTransportUdp) {
          streamResults[streamId].candidateTypeBitpattern |=
            LOCAL_GATHERED_TURN_UDP;
        } else if (transport == kNrIceTransportTcp) {
          streamResults[streamId].candidateTypeBitpattern |=
            LOCAL_GATHERED_TURN_TCP;
        }
      } else {
        streamResults[streamId].candidateTypeBitpattern |= REMOTE_GATHERED_TURN;
      }
    } else if (cand.mCandidateType.Value() ==
               RTCStatsIceCandidateType::Serverreflexive) {
      if (cand.mType.Value() == RTCStatsType::Localcandidate) {
        streamResults[streamId].candidateTypeBitpattern |=
          LOCAL_GATHERED_SERVER_REFLEXIVE;
      } else {
        streamResults[streamId].candidateTypeBitpattern |=
          REMOTE_GATHERED_SERVER_REFLEXIVE;
      }
    }
  }

  for (auto i = streamResults.begin(); i != streamResults.end(); ++i) {
    if (i->second.streamSucceeded) {
      Telemetry::Accumulate(Telemetry::WEBRTC_CANDIDATE_TYPES_GIVEN_SUCCESS,
                            i->second.candidateTypeBitpattern);
    } else {
      Telemetry::Accumulate(Telemetry::WEBRTC_CANDIDATE_TYPES_GIVEN_FAILURE,
                            i->second.candidateTypeBitpattern);
    }
  }

  // Beyond ICE, accumulate telemetry for various PER_CALL settings here.

  if (query->report->mOutboundRTPStreamStats.WasPassed()) {
    auto& array = query->report->mOutboundRTPStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo || s.mIsRemote) {
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

  if (query->report->mInboundRTPStreamStats.WasPassed()) {
    auto& array = query->report->mInboundRTPStreamStats.Value();
    for (decltype(array.Length()) i = 0; i < array.Length(); i++) {
      auto& s = array[i];
      bool isVideo = (s.mId.Value().Find("video") != -1);
      if (!isVideo || s.mIsRemote) {
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

  PeerConnectionCtx *ctx = GetPeerConnectionCtx();
  if (ctx) {
    ctx->mStatsForClosedPeerConnections.AppendElement(*query->report, fallible);
  }
}

static void GetStatsForLongTermStorage_s(
    nsAutoPtr<RTCStatsQuery> query) {

  MOZ_ASSERT(query);

  nsresult rv = PeerConnectionImpl::ExecuteStatsQuery_s(query.get());

  // Check whether packets were dropped due to rate limiting during
  // this call. (These calls must be made on STS)
  unsigned char rate_limit_bit_pattern = 0;
  if (!mozilla::nr_socket_short_term_violation_time().IsNull() &&
      !query->iceStartTime.IsNull() &&
      mozilla::nr_socket_short_term_violation_time() >= query->iceStartTime) {
    rate_limit_bit_pattern |= 1;
  }
  if (!mozilla::nr_socket_long_term_violation_time().IsNull() &&
      !query->iceStartTime.IsNull() &&
      mozilla::nr_socket_long_term_violation_time() >= query->iceStartTime) {
    rate_limit_bit_pattern |= 2;
  }

  if (query->failed) {
    Telemetry::Accumulate(
        Telemetry::WEBRTC_STUN_RATE_LIMIT_EXCEEDED_BY_TYPE_GIVEN_FAILURE,
        rate_limit_bit_pattern);
  } else {
    Telemetry::Accumulate(
        Telemetry::WEBRTC_STUN_RATE_LIMIT_EXCEEDED_BY_TYPE_GIVEN_SUCCESS,
        rate_limit_bit_pattern);
  }

  // Even if Telemetry::Accumulate is threadsafe, we still need to send the
  // query back to main, since that is where it must be destroyed.
  NS_DispatchToMainThread(
      WrapRunnableNM(
          &StoreLongTermICEStatisticsImpl_m,
          rv,
          query),
      NS_DISPATCH_NORMAL);
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

  nsAutoPtr<RTCStatsQuery> query(new RTCStatsQuery(true));

  nsresult rv = aPc.BuildStatsQuery_m(nullptr, query.get());

  NS_ENSURE_SUCCESS_VOID(rv);

  RUN_ON_THREAD(aPc.GetSTSThread(),
                WrapRunnableNM(&GetStatsForLongTermStorage_s,
                               query),
                NS_DISPATCH_NORMAL);
}

} // namespace dom
} // namespace mozilla

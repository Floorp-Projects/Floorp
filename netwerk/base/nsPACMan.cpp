/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPACMan.h"

#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIAuthPrompt.h"
#include "nsIDHCPClient.h"
#include "nsIHttpChannel.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPromptFactory.h"
#include "nsIProtocolProxyService.h"
#include "nsISystemProxySettings.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

//-----------------------------------------------------------------------------

namespace mozilla {
namespace net {

LazyLogModule gProxyLog("proxy");

#undef LOG
#define LOG(args) MOZ_LOG(gProxyLog, LogLevel::Debug, args)
#define MOZ_WPAD_URL "http://wpad/wpad.dat"
#define MOZ_DHCP_WPAD_OPTION 252

// The PAC thread does evaluations of both PAC files and
// nsISystemProxySettings because they can both block the calling thread and we
// don't want that on the main thread

// Check to see if the underlying request was not an error page in the case of
// a HTTP request.  For other types of channels, just return true.
static bool
HttpRequestSucceeded(nsIStreamLoader *loader)
{
  nsCOMPtr<nsIRequest> request;
  loader->GetRequest(getter_AddRefs(request));

  bool result = true;  // default to assuming success

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(request);
  if (httpChannel) {
    // failsafe
    Unused << httpChannel->GetRequestSucceeded(&result);
  }

  return result;
}

// Read preference setting of extra JavaScript context heap size.
// PrefService tends to be run on main thread, where ProxyAutoConfig runs on
// ProxyResolution thread, so it's read here and passed to ProxyAutoConfig.
static uint32_t
GetExtraJSContextHeapSize()
{
  MOZ_ASSERT(NS_IsMainThread());

  static int32_t extraSize = -1;

  if (extraSize < 0) {
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    int32_t value;

    if (prefs && NS_SUCCEEDED(prefs->GetIntPref(
        "network.proxy.autoconfig_extra_jscontext_heap_size", &value))) {
      LOG(("autoconfig_extra_jscontext_heap_size: %d\n", value));

      extraSize = value;
    }
  }

  return extraSize < 0 ? 0 : extraSize;
}

// Read network proxy type from preference
// Used to verify that the preference is WPAD in nsPACMan::ConfigureWPAD
nsresult
GetNetworkProxyTypeFromPref(int32_t* type)
{
  *type = 0;
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if (!prefs) {
    LOG(("Failed to get a preference service object"));
    return NS_ERROR_FACTORY_NOT_REGISTERED;
  }
  nsresult rv = prefs->GetIntPref("network.proxy.type", type);
  if (!NS_SUCCEEDED(rv)) {
    LOG(("Failed to retrieve network.proxy.type from prefs"));
    return rv;
  }
  LOG(("network.proxy.type pref retrieved: %d\n", *type));
  return NS_OK;
}

//-----------------------------------------------------------------------------

// The ExecuteCallback runnable is triggered by
// nsPACManCallback::OnQueryComplete on the Main thread when its completion is
// discovered on the pac thread

class ExecuteCallback final : public Runnable
{
public:
  ExecuteCallback(nsPACManCallback* aCallback, nsresult status)
    : Runnable("net::ExecuteCallback")
    , mCallback(aCallback)
    , mStatus(status)
  {
  }

  void SetPACString(const nsACString &pacString)
  {
    mPACString = pacString;
  }

  void SetPACURL(const nsACString &pacURL)
  {
    mPACURL = pacURL;
  }

  NS_IMETHOD Run() override
  {
    mCallback->OnQueryComplete(mStatus, mPACString, mPACURL);
    mCallback = nullptr;
    return NS_OK;
  }

private:
  RefPtr<nsPACManCallback> mCallback;
  nsresult                   mStatus;
  nsCString                  mPACString;
  nsCString                  mPACURL;
};

//-----------------------------------------------------------------------------

// The PAC thread must be deleted from the main thread, this class
// acts as a proxy to do that, as the PACMan is reference counted
// and might be destroyed on either thread

class ShutdownThread final : public Runnable
{
public:
  explicit ShutdownThread(nsIThread* thread)
    : Runnable("net::ShutdownThread")
    , mThread(thread)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
    mThread->Shutdown();
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

// Dispatch this to wait until the PAC thread shuts down.

class WaitForThreadShutdown final : public Runnable
{
public:
  explicit WaitForThreadShutdown(nsPACMan* aPACMan)
    : Runnable("net::WaitForThreadShutdown")
    , mPACMan(aPACMan)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
    if (mPACMan->mPACThread) {
      mPACMan->mPACThread->Shutdown();
      mPACMan->mPACThread = nullptr;
    }
    return NS_OK;
  }

private:
  RefPtr<nsPACMan> mPACMan;
};

//-----------------------------------------------------------------------------

// PACLoadComplete allows the PAC thread to tell the main thread that
// the javascript PAC file has been installed (perhaps unsuccessfully)
// and that there is no reason to queue executions anymore

class PACLoadComplete final : public Runnable
{
public:
  explicit PACLoadComplete(nsPACMan* aPACMan)
    : Runnable("net::PACLoadComplete")
    , mPACMan(aPACMan)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
    mPACMan->mLoader = nullptr;
    mPACMan->PostProcessPendingQ();
    return NS_OK;
  }

private:
  RefPtr<nsPACMan> mPACMan;
};


//-----------------------------------------------------------------------------

// ConfigureWPADComplete allows the PAC thread to tell the main thread that
// the URL for the PAC file has been found
class ConfigureWPADComplete final : public Runnable
{
public:
  ConfigureWPADComplete(nsPACMan *aPACMan, const nsACString &aPACURISpec)
    : Runnable("net::ConfigureWPADComplete"),
     mPACMan(aPACMan),
     mPACURISpec(aPACURISpec)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
    mPACMan->AssignPACURISpec(mPACURISpec);
    mPACMan->ContinueLoadingAfterPACUriKnown();
    return NS_OK;
  }

private:
  RefPtr<nsPACMan> mPACMan;
  nsCString mPACURISpec;
};

//-----------------------------------------------------------------------------

// ExecutePACThreadAction is used to proxy actions from the main
// thread onto the PAC thread. There are 4 options: process the queue,
// cancel the queue, query DHCP for the PAC option
// and setup the javascript context with a new PAC file

class ExecutePACThreadAction final : public Runnable
{
public:
  // by default we just process the queue
  explicit ExecutePACThreadAction(nsPACMan* aPACMan)
    : Runnable("net::ExecutePACThreadAction")
    , mPACMan(aPACMan)
    , mCancel(false)
    , mCancelStatus(NS_OK)
    , mSetupPAC(false)
    , mExtraHeapSize(0)
    , mConfigureWPAD(false)
  { }

  void CancelQueue (nsresult status)
  {
    mCancel = true;
    mCancelStatus = status;
  }

  void SetupPAC (const char *text,
                 uint32_t datalen,
                 const nsACString &pacURI,
                 uint32_t extraHeapSize)
  {
    mSetupPAC = true;
    mSetupPACData.Assign(text, datalen);
    mSetupPACURI = pacURI;
    mExtraHeapSize = extraHeapSize;
  }

  void ConfigureWPAD()
  {
    mConfigureWPAD = true;
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");
    if (mCancel) {
      mPACMan->CancelPendingQ(mCancelStatus);
      mCancel = false;
      return NS_OK;
    }

    if (mSetupPAC) {
      mSetupPAC = false;

      nsCOMPtr<nsIEventTarget> target = mPACMan->GetNeckoTarget();
      mPACMan->mPAC.Init(mSetupPACURI,
                         mSetupPACData,
                         mPACMan->mIncludePath,
                         mExtraHeapSize,
                         target);

      RefPtr<PACLoadComplete> runnable = new PACLoadComplete(mPACMan);
      mPACMan->Dispatch(runnable.forget());
      return NS_OK;
    }

    if (mConfigureWPAD) {
      nsAutoCString spec;
      mConfigureWPAD = false;
      mPACMan->ConfigureWPAD(spec);
      RefPtr<ConfigureWPADComplete> runnable = new ConfigureWPADComplete(mPACMan, spec);
      mPACMan->Dispatch(runnable.forget());
      return NS_OK;
    }

    mPACMan->ProcessPendingQ();
    return NS_OK;
  }

private:
  RefPtr<nsPACMan> mPACMan;

  bool      mCancel;
  nsresult  mCancelStatus;

  bool                 mSetupPAC;
  uint32_t             mExtraHeapSize;
  nsCString            mSetupPACData;
  nsCString            mSetupPACURI;
  bool                 mConfigureWPAD;
};

//-----------------------------------------------------------------------------

PendingPACQuery::PendingPACQuery(nsPACMan* pacMan,
                                 nsIURI* uri,
                                 nsPACManCallback* callback,
                                 bool mainThreadResponse)
  : Runnable("net::PendingPACQuery")
  , mPort(0)
  , mPACMan(pacMan)
  , mCallback(callback)
  , mOnMainThreadOnly(mainThreadResponse)
{
  uri->GetAsciiSpec(mSpec);
  uri->GetAsciiHost(mHost);
  uri->GetScheme(mScheme);
  uri->GetPort(&mPort);
}

void
PendingPACQuery::Complete(nsresult status, const nsACString &pacString)
{
  if (!mCallback)
    return;
  RefPtr<ExecuteCallback> runnable = new ExecuteCallback(mCallback, status);
  runnable->SetPACString(pacString);
  if (mOnMainThreadOnly)
    mPACMan->Dispatch(runnable.forget());
  else
    runnable->Run();
}

void
PendingPACQuery::UseAlternatePACFile(const nsACString &pacURL)
{
  if (!mCallback)
    return;

  RefPtr<ExecuteCallback> runnable = new ExecuteCallback(mCallback, NS_OK);
  runnable->SetPACURL(pacURL);
  if (mOnMainThreadOnly)
    mPACMan->Dispatch(runnable.forget());
  else
    runnable->Run();
}

NS_IMETHODIMP
PendingPACQuery::Run()
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");
  mPACMan->PostQuery(this);
  return NS_OK;
}

//-----------------------------------------------------------------------------

static bool sThreadLocalSetup = false;
static uint32_t sThreadLocalIndex = 0xdeadbeef; // out of range

static const char *kPACIncludePath =
  "network.proxy.autoconfig_url.include_path";

nsPACMan::nsPACMan(nsIEventTarget *mainThreadEventTarget)
  : NeckoTargetHolder(mainThreadEventTarget)
  , mLoadPending(false)
  , mShutdown(false)
  , mLoadFailureCount(0)
  , mInProgress(false)
  , mAutoDetect(false)
  , mWPADOverDHCPEnabled(false)
  , mProxyConfigType(0)
{
  MOZ_ASSERT(NS_IsMainThread(), "pacman must be created on main thread");
  if (!sThreadLocalSetup){
    sThreadLocalSetup = true;
    PR_NewThreadPrivateIndex(&sThreadLocalIndex, nullptr);
  }
  mPAC.SetThreadLocalIndex(sThreadLocalIndex);
  mIncludePath = Preferences::GetBool(kPACIncludePath, false);
}

nsPACMan::~nsPACMan()
{
  MOZ_ASSERT(mShutdown, "Shutdown must be called before dtor.");

  if (mPACThread) {
    if (NS_IsMainThread()) {
      mPACThread->Shutdown();
      mPACThread = nullptr;
    }
    else {
      RefPtr<ShutdownThread> runnable = new ShutdownThread(mPACThread);
      Dispatch(runnable.forget());
    }
  }

  NS_ASSERTION(mLoader == nullptr, "pac man not shutdown properly");
  NS_ASSERTION(mPendingQ.isEmpty(), "pac man not shutdown properly");
}

void
nsPACMan::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "pacman must be shutdown on main thread");
  if (mShutdown) {
    return;
  }
  mShutdown = true;
  CancelExistingLoad();

  MOZ_ASSERT(mPACThread, "mPAC requires mPACThread to shutdown");
  PostCancelPendingQ(NS_ERROR_ABORT);

  RefPtr<WaitForThreadShutdown> runnable = new WaitForThreadShutdown(this);
  Dispatch(runnable.forget());
}

nsresult
nsPACMan::AsyncGetProxyForURI(nsIURI *uri,
                              nsPACManCallback *callback,
                              bool mainThreadResponse)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (mShutdown)
    return NS_ERROR_NOT_AVAILABLE;

  // Maybe Reload PAC
  if (!mPACURISpec.IsEmpty() && !mScheduledReload.IsNull() &&
      TimeStamp::Now() > mScheduledReload) {
    LOG(("nsPACMan::AsyncGetProxyForURI reload as scheduled\n"));

    LoadPACFromURI(mAutoDetect? EmptyCString(): mPACURISpec);
  }

  RefPtr<PendingPACQuery> query =
    new PendingPACQuery(this, uri, callback, mainThreadResponse);

  if (IsPACURI(uri)) {
    // deal with this directly instead of queueing it
    query->Complete(NS_OK, EmptyCString());
    return NS_OK;
  }

  return mPACThread->Dispatch(query, nsIEventTarget::DISPATCH_NORMAL);
}

nsresult
nsPACMan::PostQuery(PendingPACQuery *query)
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");

  if (mShutdown) {
    query->Complete(NS_ERROR_NOT_AVAILABLE, EmptyCString());
    return NS_OK;
  }

  // add a reference to the query while it is in the pending list
  RefPtr<PendingPACQuery> addref(query);
  mPendingQ.insertBack(addref.forget().take());
  ProcessPendingQ();
  return NS_OK;
}

nsresult
nsPACMan::LoadPACFromURI(const nsACString &aSpec)
{
  NS_ENSURE_STATE(!mShutdown);

  nsCOMPtr<nsIStreamLoader> loader =
      do_CreateInstance(NS_STREAMLOADER_CONTRACTID);
  NS_ENSURE_STATE(loader);

  LOG(("nsPACMan::LoadPACFromURI aSpec: %s\n", aSpec.BeginReading()));
  // Since we might get called from nsProtocolProxyService::Init, we need to
  // post an event back to the main thread before we try to use the IO service.
  //
  // But, we need to flag ourselves as loading, so that we queue up any PAC
  // queries the enter between now and when we actually load the PAC file.

  if (!mLoadPending) {
    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod("nsPACMan::StartLoading", this, &nsPACMan::StartLoading);
    nsresult rv = NS_IsMainThread()
      ? Dispatch(runnable.forget())
      : GetCurrentThreadEventTarget()->Dispatch(runnable.forget());
    if (NS_FAILED(rv))
      return rv;
    mLoadPending = true;
  }

  CancelExistingLoad();

  mLoader = loader;
  mPACURIRedirectSpec.Truncate();
  mNormalPACURISpec.Truncate(); // set at load time
  mLoadFailureCount = 0;  // reset
  mAutoDetect = aSpec.IsEmpty();
  mPACURISpec.Assign(aSpec);

  // reset to Null
  mScheduledReload = TimeStamp();
  return NS_OK;
}

nsresult
nsPACMan::GetPACFromDHCP(nsACString &aSpec)
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");
  if (!mDHCPClient) {
    LOG(("nsPACMan::GetPACFromDHCP DHCP option %d query failed because there is no DHCP client available\n", MOZ_DHCP_WPAD_OPTION));
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult rv;
  rv = mDHCPClient->GetOption(MOZ_DHCP_WPAD_OPTION, aSpec);
  if (NS_FAILED(rv)) {
    LOG(("nsPACMan::GetPACFromDHCP DHCP option %d query failed with result %d\n", MOZ_DHCP_WPAD_OPTION, (uint32_t)rv));
  } else {
    LOG(("nsPACMan::GetPACFromDHCP DHCP option %d query succeeded, finding PAC URL %s\n", MOZ_DHCP_WPAD_OPTION, aSpec.BeginReading()));
  }
  return rv;
}

nsresult
nsPACMan::ConfigureWPAD(nsACString &aSpec)
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");

  MOZ_RELEASE_ASSERT(mProxyConfigType == nsIProtocolProxyService::PROXYCONFIG_WPAD,
            "WPAD is being executed when not selected by user");

  aSpec.Truncate();
  if (mWPADOverDHCPEnabled) {
    GetPACFromDHCP(aSpec);
  }

  if (aSpec.IsEmpty()) {
    // We diverge from the WPAD spec here in that we don't walk the
    // hosts's FQDN, stripping components until we hit a TLD.  Doing so
    // is dangerous in the face of an incomplete list of TLDs, and TLDs
    // get added over time.  We could consider doing only a single
    // substitution of the first component, if that proves to help
    // compatibility.
    aSpec.AssignLiteral(MOZ_WPAD_URL);
  }
  return NS_OK;
}

void
nsPACMan::AssignPACURISpec(const nsACString &aSpec)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  mPACURISpec.Assign(aSpec);
}

void
nsPACMan::StartLoading()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  mLoadPending = false;

  // CancelExistingLoad was called...
  if (!mLoader) {
    PostCancelPendingQ(NS_ERROR_ABORT);
    return;
  }

  if (mAutoDetect) {
    GetNetworkProxyTypeFromPref(&mProxyConfigType);
    RefPtr<ExecutePACThreadAction> wpadConfigurer =
      new ExecutePACThreadAction(this);
    wpadConfigurer->ConfigureWPAD();
    if (mPACThread) {
      mPACThread->Dispatch(wpadConfigurer, nsIEventTarget::DISPATCH_NORMAL);
    }
  } else {
    ContinueLoadingAfterPACUriKnown();
  }
}

void
nsPACMan::ContinueLoadingAfterPACUriKnown()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");

  // CancelExistingLoad was called...
  if (!mLoader) {
    PostCancelPendingQ(NS_ERROR_ABORT);
    return;
  }
  if (NS_SUCCEEDED(mLoader->Init(this, nullptr))) {
    // Always hit the origin server when loading PAC.
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    if (ios) {
      nsCOMPtr<nsIChannel> channel;
      nsCOMPtr<nsIURI> pacURI;
      NS_NewURI(getter_AddRefs(pacURI), mPACURISpec);

      // NOTE: This results in GetProxyForURI being called
      if (pacURI) {
        nsresult rv = pacURI->GetSpec(mNormalPACURISpec);
        MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
        NS_NewChannel(getter_AddRefs(channel),
                      pacURI,
                      nsContentUtils::GetSystemPrincipal(),
                      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                      nsIContentPolicy::TYPE_OTHER,
                      nullptr, // PerformanceStorage,
                      nullptr, // aLoadGroup
                      nullptr, // aCallbacks
                      nsIRequest::LOAD_NORMAL,
                      ios);
      }
      else {
        LOG(("nsPACMan::StartLoading Failed pacspec uri conversion %s\n",
             mPACURISpec.get()));
      }

      if (channel) {
        channel->SetLoadFlags(nsIRequest::LOAD_BYPASS_CACHE);
        channel->SetNotificationCallbacks(this);
        if (NS_SUCCEEDED(channel->AsyncOpen2(mLoader)))
          return;
      }
    }
  }

  CancelExistingLoad();
  PostCancelPendingQ(NS_ERROR_UNEXPECTED);
}


void
nsPACMan::OnLoadFailure()
{
  int32_t minInterval = 5;    // 5 seconds
  int32_t maxInterval = 300;  // 5 minutes

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    prefs->GetIntPref("network.proxy.autoconfig_retry_interval_min",
                      &minInterval);
    prefs->GetIntPref("network.proxy.autoconfig_retry_interval_max",
                      &maxInterval);
  }

  int32_t interval = minInterval << mLoadFailureCount++;  // seconds
  if (!interval || interval > maxInterval)
    interval = maxInterval;

  mScheduledReload = TimeStamp::Now() + TimeDuration::FromSeconds(interval);

  LOG(("OnLoadFailure: retry in %d seconds (%d fails)\n",
       interval, mLoadFailureCount));

  // while we wait for the retry queued members should try direct
  // even if that means fast failure.
  PostCancelPendingQ(NS_ERROR_NOT_AVAILABLE);
}

void
nsPACMan::CancelExistingLoad()
{
  if (mLoader) {
    nsCOMPtr<nsIRequest> request;
    mLoader->GetRequest(getter_AddRefs(request));
    if (request)
      request->Cancel(NS_ERROR_ABORT);
    mLoader = nullptr;
  }
}

void
nsPACMan::PostProcessPendingQ()
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  RefPtr<ExecutePACThreadAction> pending =
    new ExecutePACThreadAction(this);
  if (mPACThread)
    mPACThread->Dispatch(pending, nsIEventTarget::DISPATCH_NORMAL);
}

void
nsPACMan::PostCancelPendingQ(nsresult status)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  RefPtr<ExecutePACThreadAction> pending =
    new ExecutePACThreadAction(this);
  pending->CancelQueue(status);
  if (mPACThread)
    mPACThread->Dispatch(pending, nsIEventTarget::DISPATCH_NORMAL);
}

void
nsPACMan::CancelPendingQ(nsresult status)
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");
  RefPtr<PendingPACQuery> query;

  while (!mPendingQ.isEmpty()) {
    query = dont_AddRef(mPendingQ.popLast());
    query->Complete(status, EmptyCString());
  }

  if (mShutdown)
    mPAC.Shutdown();
}

void
nsPACMan::ProcessPendingQ()
{
  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");
  while (ProcessPending());

  if (mShutdown) {
    mPAC.Shutdown();
  } else {
    // do GC while the thread has nothing pending
    mPAC.GC();
  }
}

// returns true if progress was made by shortening the queue
bool
nsPACMan::ProcessPending()
{
  if (mPendingQ.isEmpty())
    return false;

  // queue during normal load, but if we are retrying a failed load then
  // fast fail the queries
  if (mInProgress || (IsLoading() && !mLoadFailureCount))
    return false;

  RefPtr<PendingPACQuery> query(dont_AddRef(mPendingQ.popFirst()));

  if (mShutdown || IsLoading()) {
    query->Complete(NS_ERROR_NOT_AVAILABLE, EmptyCString());
    return true;
  }

  nsAutoCString pacString;
  bool completed = false;
  mInProgress = true;
  nsAutoCString PACURI;

  // first we need to consider the system proxy changing the pac url
  if (mSystemProxySettings &&
      NS_SUCCEEDED(mSystemProxySettings->GetPACURI(PACURI)) &&
      !PACURI.IsEmpty() &&
      !PACURI.Equals(mPACURISpec)) {
    query->UseAlternatePACFile(PACURI);
    LOG(("Use PAC from system settings: %s\n", PACURI.get()));
    completed = true;
  }

  // now try the system proxy settings for this particular url if
  // PAC was not specified
  if (!completed && mSystemProxySettings && PACURI.IsEmpty() &&
      NS_SUCCEEDED(mSystemProxySettings->
                   GetProxyForURI(query->mSpec, query->mScheme,
                                  query->mHost, query->mPort,
                                  pacString))) {
    LOG(("Use proxy from system settings: %s\n", pacString.get()));
    query->Complete(NS_OK, pacString);
    completed = true;
  }

  // the systemproxysettings didn't complete the resolution. try via PAC
  if (!completed) {
    nsresult status = mPAC.GetProxyForURI(query->mSpec, query->mHost,
                                          pacString);
    LOG(("Use proxy from PAC: %s\n", pacString.get()));
    query->Complete(status, pacString);
  }

  mInProgress = false;
  return true;
}

NS_IMPL_ISUPPORTS(nsPACMan, nsIStreamLoaderObserver,
                  nsIInterfaceRequestor, nsIChannelEventSink)

NS_IMETHODIMP
nsPACMan::OnStreamComplete(nsIStreamLoader *loader,
                           nsISupports *context,
                           nsresult status,
                           uint32_t dataLen,
                           const uint8_t *data)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");
  if (mLoader != loader) {
    // If this happens, then it means that LoadPACFromURI was called more
    // than once before the initial call completed.  In this case, status
    // should be NS_ERROR_ABORT, and if so, then we know that we can and
    // should delay any processing.
    LOG(("OnStreamComplete: called more than once\n"));
    if (status == NS_ERROR_ABORT)
      return NS_OK;
  }

  LOG(("OnStreamComplete: entry\n"));

  if (NS_SUCCEEDED(status) && HttpRequestSucceeded(loader)) {
    // Get the URI spec used to load this PAC script.
    nsAutoCString pacURI;
    {
      nsCOMPtr<nsIRequest> request;
      loader->GetRequest(getter_AddRefs(request));
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri)
          uri->GetAsciiSpec(pacURI);
      }
    }

    // We assume that the PAC text is ASCII (or ISO-Latin-1).  We've had this
    // assumption forever, and some real-world PAC scripts actually have some
    // non-ASCII text in comment blocks (see bug 296163).
    const char *text = (const char *) data;

    // we have succeeded in loading the pac file using a bunch of interfaces that
    // are main thread only, unfortunately we have to initialize the instance of
    // the PAC evaluator (NS_PROXYAUTOCONFIG_CONTRACTID) on the pac thread, because
    // that is where it will be used.

    RefPtr<ExecutePACThreadAction> pending =
      new ExecutePACThreadAction(this);
    pending->SetupPAC(text, dataLen, pacURI, GetExtraJSContextHeapSize());
    if (mPACThread)
      mPACThread->Dispatch(pending, nsIEventTarget::DISPATCH_NORMAL);

    LOG(("OnStreamComplete: process the PAC contents\n"));

    // Even if the PAC file could not be parsed, we did succeed in loading the
    // data for it.
    mLoadFailureCount = 0;
  } else {
    // We were unable to load the PAC file (presumably because of a network
    // failure).  Try again a little later.
    LOG(("OnStreamComplete: unable to load PAC, retry later\n"));
    OnLoadFailure();
  }

  if (NS_SUCCEEDED(status))
    PostProcessPendingQ();
  else
    PostCancelPendingQ(status);

  return NS_OK;
}

NS_IMETHODIMP
nsPACMan::GetInterface(const nsIID &iid, void **result)
{
  // In case loading the PAC file requires authentication.
  if (iid.Equals(NS_GET_IID(nsIAuthPrompt))) {
    nsCOMPtr<nsIPromptFactory> promptFac = do_GetService("@mozilla.org/prompter;1");
    NS_ENSURE_TRUE(promptFac, NS_ERROR_FAILURE);
    return promptFac->GetPrompt(nullptr, iid, reinterpret_cast<void**>(result));
  }

  // In case loading the PAC file results in a redirect.
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *result = static_cast<nsIChannelEventSink *>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsPACMan::AsyncOnChannelRedirect(nsIChannel *oldChannel, nsIChannel *newChannel,
                                 uint32_t flags,
                                 nsIAsyncVerifyRedirectCallback *callback)
{
  MOZ_ASSERT(NS_IsMainThread(), "wrong thread");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> pacURI;
  if (NS_FAILED((rv = newChannel->GetURI(getter_AddRefs(pacURI)))))
      return rv;

  rv = pacURI->GetSpec(mPACURIRedirectSpec);
  if (NS_FAILED(rv))
      return rv;

  LOG(("nsPACMan redirect from original %s to redirected %s\n",
       mPACURISpec.get(), mPACURIRedirectSpec.get()));

  // do not update mPACURISpec - that needs to stay as the
  // configured URI so that we can determine when the config changes.
  // However do track the most recent URI in the redirect change
  // as mPACURIRedirectSpec so that URI can be allowed to bypass
  // the proxy and actually fetch the pac file.

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

nsresult
nsPACMan::Init(nsISystemProxySettings *systemProxySettings)
{
  mSystemProxySettings = systemProxySettings;
  mDHCPClient = do_GetService(NS_DHCPCLIENT_CONTRACTID);


  nsresult rv =
    NS_NewNamedThread("ProxyResolution", getter_AddRefs(mPACThread));

  return rv;
}

} // namespace net
} // namespace mozilla

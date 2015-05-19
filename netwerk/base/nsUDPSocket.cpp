/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/Endian.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/Telemetry.h"

#include "nsSocketTransport2.h"
#include "nsUDPSocket.h"
#include "nsProxyRelease.h"
#include "nsAutoPtr.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "prnetdb.h"
#include "prio.h"
#include "nsNetAddr.h"
#include "nsNetSegmentUtils.h"
#include "NetworkActivityMonitor.h"
#include "nsStreamUtils.h"
#include "nsIPipe.h"
#include "prerror.h"
#include "nsThreadUtils.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"

#ifdef MOZ_WIDGET_GONK
#include "NetStatistics.h"
#endif

using namespace mozilla::net;
using namespace mozilla;

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static const uint32_t UDP_PACKET_CHUNK_SIZE = 1400;

//-----------------------------------------------------------------------------

typedef void (nsUDPSocket:: *nsUDPSocketFunc)(void);

static nsresult
PostEvent(nsUDPSocket *s, nsUDPSocketFunc func)
{
  nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(s, func);

  if (!gSocketTransportService)
    return NS_ERROR_FAILURE;

  return gSocketTransportService->Dispatch(ev, NS_DISPATCH_NORMAL);
}

static nsresult
ResolveHost(const nsACString &host, nsIDNSListener *listener)
{
  nsresult rv;

  nsCOMPtr<nsIDNSService> dns =
      do_GetService("@mozilla.org/network/dns-service;1", &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICancelable> tmpOutstanding;
  return dns->AsyncResolve(host, 0, listener, nullptr,
                           getter_AddRefs(tmpOutstanding));

}

//-----------------------------------------------------------------------------

class SetSocketOptionRunnable : public nsRunnable
{
public:
  SetSocketOptionRunnable(nsUDPSocket* aSocket, const PRSocketOptionData& aOpt)
    : mSocket(aSocket)
    , mOpt(aOpt)
  {}

  NS_IMETHOD Run()
  {
    return mSocket->SetSocketOption(mOpt);
  }

private:
  nsRefPtr<nsUDPSocket> mSocket;
  PRSocketOptionData    mOpt;
};

//-----------------------------------------------------------------------------
// nsUDPSocketCloseThread
//-----------------------------------------------------------------------------

// A helper class carrying call to PR_Close on nsUDPSocket's FD to a separate
// thread.  This may be a workaround for shutdown blocks that are caused by
// serial calls to close on UDP sockets.
// It starts an NSPR thread for each socket and also adds itself as an observer
// to "xpcom-shutdown-threads" notification where we join the thread (if not
// already done). During worktime of the thread the class is also
// self-referenced, since observer service might throw the reference away
// sooner than the thread is actually done.
class nsUDPSocketCloseThread : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static bool Close(PRFileDesc *aFd);

private:
  explicit nsUDPSocketCloseThread(PRFileDesc *aFd);
  virtual ~nsUDPSocketCloseThread() { }

  bool Begin();
  void ThreadFunc();
  void AddObserver();
  void JoinAndRemove();

  static void ThreadFunc(void *aClosure)
    { static_cast<nsUDPSocketCloseThread*>(aClosure)->ThreadFunc(); }

  // Socket to close.
  PRFileDesc *mFd;
  PRThread *mThread;

  // Self reference, added before we create the thread, dropped
  // after the thread is done (from the thread).  No races, since
  // mSelf is assigned bofore the thread func is executed and
  // released only on the thread func.
  nsRefPtr<nsUDPSocketCloseThread> mSelf;

  // Telemetry probes.
  TimeStamp mBeforeClose;
  TimeStamp mAfterClose;

  // Active threads (roughly) counter, modified only on the main thread
  // and used only for telemetry reports.
  static uint32_t sActiveThreadsCount;

  // Switches to true on "xpcom-shutdown-threads" notification and since
  // then it makes the code fallback to a direct call to PR_Close().
  static bool sPastShutdown;
};

uint32_t nsUDPSocketCloseThread::sActiveThreadsCount = 0;
bool nsUDPSocketCloseThread::sPastShutdown = false;

NS_IMPL_ISUPPORTS(nsUDPSocketCloseThread, nsIObserver);

bool
nsUDPSocketCloseThread::Close(PRFileDesc *aFd)
{
  if (sPastShutdown) {
    return false;
  }

  nsRefPtr<nsUDPSocketCloseThread> t = new nsUDPSocketCloseThread(aFd);
  return t->Begin();
}

nsUDPSocketCloseThread::nsUDPSocketCloseThread(PRFileDesc *aFd)
  : mFd(aFd)
  , mThread(nullptr)
{
}

bool
nsUDPSocketCloseThread::Begin()
{
  // Observer service must be worked with on the main thread only.
  // This method is called usually on the socket thread.
  // Register us before the thread starts.  It may happen the thread is
  // done and posts removal event sooner then we would post this event
  // after the thread creation.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(
    this, &nsUDPSocketCloseThread::AddObserver);
  if (event) {
    NS_DispatchToMainThread(event);
  }

  // Keep us self-referenced during lifetime of the thread.
  // Released after the thread is done.
  mSelf = this;
  mThread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 4 * 4096);
  if (!mThread) {
    // This doesn't join since there is no thread, just removes
    // this class as an observer.
    JoinAndRemove();
    mSelf = nullptr;
    return false;
  }

  return true;
}

void
nsUDPSocketCloseThread::AddObserver()
{
  MOZ_ASSERT(NS_IsMainThread());

  ++sActiveThreadsCount;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown-threads", false);
  }
}

void
nsUDPSocketCloseThread::JoinAndRemove()
{
  // Posted from the particular (this) UDP close socket when it's done
  // or from "xpcom-shutdown-threads" notification.
  MOZ_ASSERT(NS_IsMainThread());

  if (mThread) {
    PR_JoinThread(mThread);
    mThread = nullptr;

    Telemetry::Accumulate(Telemetry::UDP_SOCKET_PARALLEL_CLOSE_COUNT, sActiveThreadsCount);
    Telemetry::AccumulateTimeDelta(Telemetry::UDP_SOCKET_CLOSE_TIME, mBeforeClose, mAfterClose);

    MOZ_ASSERT(sActiveThreadsCount > 0);
    --sActiveThreadsCount;
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown-threads");
  }
}

NS_IMETHODIMP
nsUDPSocketCloseThread::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, "xpcom-shutdown-threads")) {
    sPastShutdown = true;
    JoinAndRemove();
    return NS_OK;
  }

  MOZ_CRASH("Unexpected observer topic");
  return NS_OK;
}

void
nsUDPSocketCloseThread::ThreadFunc()
{
  PR_SetCurrentThreadName("UDP socket close");

  mBeforeClose = TimeStamp::Now();

  PR_Close(mFd);
  mFd = nullptr;

  mAfterClose = TimeStamp::Now();

  // Join and remove the observer on the main thread.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(
    this, &nsUDPSocketCloseThread::JoinAndRemove);
  if (event) {
    NS_DispatchToMainThread(event);
  }

  // Thread's done, release the self-reference.
  mSelf = nullptr;
}

//-----------------------------------------------------------------------------
// nsUDPOutputStream impl
//-----------------------------------------------------------------------------
NS_IMPL_ISUPPORTS(nsUDPOutputStream, nsIOutputStream)

nsUDPOutputStream::nsUDPOutputStream(nsUDPSocket* aSocket,
                                     PRFileDesc* aFD,
                                     PRNetAddr& aPrClientAddr)
  : mSocket(aSocket)
  , mFD(aFD)
  , mPrClientAddr(aPrClientAddr)
  , mIsClosed(false)
{
}

nsUDPOutputStream::~nsUDPOutputStream()
{
}

/* void close (); */
NS_IMETHODIMP nsUDPOutputStream::Close()
{
  if (mIsClosed)
    return NS_BASE_STREAM_CLOSED;

  mIsClosed = true;
  return NS_OK;
}

/* void flush (); */
NS_IMETHODIMP nsUDPOutputStream::Flush()
{
  return NS_OK;
}

/* unsigned long write (in string aBuf, in unsigned long aCount); */
NS_IMETHODIMP nsUDPOutputStream::Write(const char * aBuf, uint32_t aCount, uint32_t *_retval)
{
  if (mIsClosed)
    return NS_BASE_STREAM_CLOSED;

  *_retval = 0;
  int32_t count = PR_SendTo(mFD, aBuf, aCount, 0, &mPrClientAddr, PR_INTERVAL_NO_WAIT);
  if (count < 0) {
    PRErrorCode code = PR_GetError();
    return ErrorAccordingToNSPR(code);
  }

  *_retval = count;

  mSocket->AddOutputBytes(count);

  return NS_OK;
}

/* unsigned long writeFrom (in nsIInputStream aFromStream, in unsigned long aCount); */
NS_IMETHODIMP nsUDPOutputStream::WriteFrom(nsIInputStream *aFromStream, uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] unsigned long writeSegments (in nsReadSegmentFun aReader, in voidPtr aClosure, in unsigned long aCount); */
NS_IMETHODIMP nsUDPOutputStream::WriteSegments(nsReadSegmentFun aReader, void *aClosure, uint32_t aCount, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isNonBlocking (); */
NS_IMETHODIMP nsUDPOutputStream::IsNonBlocking(bool *_retval)
{
  *_retval = true;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsUDPMessage impl
//-----------------------------------------------------------------------------
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsUDPMessage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsUDPMessage)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsUDPMessage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsUDPMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIUDPMessage)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsUDPMessage)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsobj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsUDPMessage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsUDPMessage)
  tmp->mJsobj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsUDPMessage::nsUDPMessage(NetAddr* aAddr,
                           nsIOutputStream* aOutputStream,
                           FallibleTArray<uint8_t>& aData)
  : mOutputStream(aOutputStream)
{
  memcpy(&mAddr, aAddr, sizeof(NetAddr));
  aData.SwapElements(mData);
}

nsUDPMessage::~nsUDPMessage()
{
  mozilla::DropJSObjects(this);
}

/* readonly attribute nsINetAddr from; */
NS_IMETHODIMP
nsUDPMessage::GetFromAddr(nsINetAddr * *aFromAddr)
{
  NS_ENSURE_ARG_POINTER(aFromAddr);

  nsCOMPtr<nsINetAddr> result = new nsNetAddr(&mAddr);
  result.forget(aFromAddr);

  return NS_OK;
}

/* readonly attribute ACString data; */
NS_IMETHODIMP
nsUDPMessage::GetData(nsACString & aData)
{
  aData.Assign(reinterpret_cast<const char*>(mData.Elements()), mData.Length());
  return NS_OK;
}

/* readonly attribute nsIOutputStream outputStream; */
NS_IMETHODIMP
nsUDPMessage::GetOutputStream(nsIOutputStream * *aOutputStream)
{
  NS_ENSURE_ARG_POINTER(aOutputStream);
  NS_IF_ADDREF(*aOutputStream = mOutputStream);
  return NS_OK;
}

/* readonly attribute jsval rawData; */
NS_IMETHODIMP
nsUDPMessage::GetRawData(JSContext* cx,
                         JS::MutableHandleValue aRawData)
{
  if(!mJsobj){
    mJsobj = mozilla::dom::Uint8Array::Create(cx, nullptr, mData.Length(), mData.Elements());
    mozilla::HoldJSObjects(this);
  }
  aRawData.setObject(*mJsobj);
  return NS_OK;
}

/* [noscript, notxpcom, nostdcall] Uint8ArrayRef getDataAsTArray(); */
FallibleTArray<uint8_t>&
nsUDPMessage::GetDataAsTArray()
{
  return mData;
}

//-----------------------------------------------------------------------------
// nsUDPSocket
//-----------------------------------------------------------------------------

nsUDPSocket::nsUDPSocket()
  : mLock("nsUDPSocket.mLock")
  , mFD(nullptr)
  , mAppId(NECKO_UNKNOWN_APP_ID)
  , mIsInBrowserElement(false)
  , mAttached(false)
  , mByteReadCount(0)
  , mByteWriteCount(0)
{
  mAddr.raw.family = PR_AF_UNSPEC;
  // we want to be able to access the STS directly, and it may not have been
  // constructed yet.  the STS constructor sets gSocketTransportService.
  if (!gSocketTransportService)
  {
    // This call can fail if we're offline, for example.
    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(kSocketTransportServiceCID);
  }

  mSts = gSocketTransportService;
  MOZ_COUNT_CTOR(nsUDPSocket);
}

nsUDPSocket::~nsUDPSocket()
{
  if (mFD) {
    if (!nsUDPSocketCloseThread::Close(mFD)) {
      PR_Close(mFD);
    }
    mFD = nullptr;
  }

  MOZ_COUNT_DTOR(nsUDPSocket);
}

void
nsUDPSocket::AddOutputBytes(uint64_t aBytes)
{
  mByteWriteCount += aBytes;
  SaveNetworkStats(false);
}

void
nsUDPSocket::OnMsgClose()
{
  SOCKET_LOG(("nsUDPSocket::OnMsgClose [this=%p]\n", this));

  if (NS_FAILED(mCondition))
    return;

  // tear down socket.  this signals the STS to detach our socket handler.
  mCondition = NS_BINDING_ABORTED;

  // if we are attached, then socket transport service will call our
  // OnSocketDetached method automatically. Otherwise, we have to call it
  // (and thus close the socket) manually.
  if (!mAttached)
    OnSocketDetached(mFD);
}

void
nsUDPSocket::OnMsgAttach()
{
  SOCKET_LOG(("nsUDPSocket::OnMsgAttach [this=%p]\n", this));

  if (NS_FAILED(mCondition))
    return;

  mCondition = TryAttach();

  // if we hit an error while trying to attach then bail...
  if (NS_FAILED(mCondition))
  {
    NS_ASSERTION(!mAttached, "should not be attached already");
    OnSocketDetached(mFD);
  }
}

nsresult
nsUDPSocket::TryAttach()
{
  nsresult rv;

  if (!gSocketTransportService)
    return NS_ERROR_FAILURE;

  //
  // find out if it is going to be ok to attach another socket to the STS.
  // if not then we have to wait for the STS to tell us that it is ok.
  // the notification is asynchronous, which means that when we could be
  // in a race to call AttachSocket once notified.  for this reason, when
  // we get notified, we just re-enter this function.  as a result, we are
  // sure to ask again before calling AttachSocket.  in this way we deal
  // with the race condition.  though it isn't the most elegant solution,
  // it is far simpler than trying to build a system that would guarantee
  // FIFO ordering (which wouldn't even be that valuable IMO).  see bug
  // 194402 for more info.
  //
  if (!gSocketTransportService->CanAttachSocket())
  {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &nsUDPSocket::OnMsgAttach);

    nsresult rv = gSocketTransportService->NotifyWhenCanAttachSocket(event);
    if (NS_FAILED(rv))
      return rv;
  }

  //
  // ok, we can now attach our socket to the STS for polling
  //
  rv = gSocketTransportService->AttachSocket(mFD, this);
  if (NS_FAILED(rv))
    return rv;

  mAttached = true;

  //
  // now, configure our poll flags for listening...
  //
  mPollFlags = (PR_POLL_READ | PR_POLL_EXCEPT);
  return NS_OK;
}

namespace {
//-----------------------------------------------------------------------------
// UDPMessageProxy
//-----------------------------------------------------------------------------
class UDPMessageProxy final : public nsIUDPMessage
{
public:
  UDPMessageProxy(NetAddr* aAddr,
                  nsIOutputStream* aOutputStream,
                  FallibleTArray<uint8_t>& aData)
  : mOutputStream(aOutputStream)
  {
    memcpy(&mAddr, aAddr, sizeof(NetAddr));
    aData.SwapElements(mData);
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPMESSAGE

private:
  ~UDPMessageProxy() {}

  NetAddr mAddr;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  FallibleTArray<uint8_t> mData;
};

NS_IMPL_ISUPPORTS(UDPMessageProxy, nsIUDPMessage)

/* readonly attribute nsINetAddr from; */
NS_IMETHODIMP
UDPMessageProxy::GetFromAddr(nsINetAddr * *aFromAddr)
{
  NS_ENSURE_ARG_POINTER(aFromAddr);

  nsCOMPtr<nsINetAddr> result = new nsNetAddr(&mAddr);
  result.forget(aFromAddr);

  return NS_OK;
}

/* readonly attribute ACString data; */
NS_IMETHODIMP
UDPMessageProxy::GetData(nsACString & aData)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript, notxpcom, nostdcall] Uint8TArrayRef getDataAsTArray(); */
FallibleTArray<uint8_t>&
UDPMessageProxy::GetDataAsTArray()
{
  return mData;
}

/* readonly attribute jsval rawData; */
NS_IMETHODIMP
UDPMessageProxy::GetRawData(JSContext* cx,
                            JS::MutableHandleValue aRawData)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIOutputStream outputStream; */
NS_IMETHODIMP
UDPMessageProxy::GetOutputStream(nsIOutputStream * *aOutputStream)
{
  NS_ENSURE_ARG_POINTER(aOutputStream);
  NS_IF_ADDREF(*aOutputStream = mOutputStream);
  return NS_OK;
}

} //anonymous namespace

//-----------------------------------------------------------------------------
// nsUDPSocket::nsASocketHandler
//-----------------------------------------------------------------------------

void
nsUDPSocket::OnSocketReady(PRFileDesc *fd, int16_t outFlags)
{
  NS_ASSERTION(NS_SUCCEEDED(mCondition), "oops");
  NS_ASSERTION(mFD == fd, "wrong file descriptor");
  NS_ASSERTION(outFlags != -1, "unexpected timeout condition reached");

  if (outFlags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL))
  {
    NS_WARNING("error polling on listening socket");
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }

  PRNetAddr prClientAddr;
  uint32_t count;
  // Bug 1165423 - using 8k here because the packet could be larger
  // than the MTU with fragmentation
  char buff[8 * 1024];
  count = PR_RecvFrom(mFD, buff, sizeof(buff), 0, &prClientAddr, PR_INTERVAL_NO_WAIT);

  if (count < 1) {
    NS_WARNING("error of recvfrom on UDP socket");
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }
  mByteReadCount += count;
  SaveNetworkStats(false);

  FallibleTArray<uint8_t> data;
  if(!data.AppendElements(buff, count)){
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }

  nsCOMPtr<nsIAsyncInputStream> pipeIn;
  nsCOMPtr<nsIAsyncOutputStream> pipeOut;

  uint32_t segsize = UDP_PACKET_CHUNK_SIZE;
  uint32_t segcount = 0;
  net_ResolveSegmentParams(segsize, segcount);
  nsresult rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut),
                true, true, segsize, segcount);

  if (NS_FAILED(rv)) {
    return;
  }

  nsRefPtr<nsUDPOutputStream> os = new nsUDPOutputStream(this, mFD, prClientAddr);
  rv = NS_AsyncCopy(pipeIn, os, mSts,
                    NS_ASYNCCOPY_VIA_READSEGMENTS, UDP_PACKET_CHUNK_SIZE);

  if (NS_FAILED(rv)) {
    return;
  }

  NetAddr netAddr;
  PRNetAddrToNetAddr(&prClientAddr, &netAddr);
  nsCOMPtr<nsIUDPMessage> message = new UDPMessageProxy(&netAddr, pipeOut, data);
  mListener->OnPacketReceived(this, message);
}

void
nsUDPSocket::OnSocketDetached(PRFileDesc *fd)
{
  // force a failure condition if none set; maybe the STS is shutting down :-/
  if (NS_SUCCEEDED(mCondition))
    mCondition = NS_ERROR_ABORT;

  if (mFD)
  {
    NS_ASSERTION(mFD == fd, "wrong file descriptor");
    if (!nsUDPSocketCloseThread::Close(mFD)) {
      PR_Close(mFD);
    }
    mFD = nullptr;
  }
  SaveNetworkStats(true);

  if (mListener)
  {
    // need to atomically clear mListener.  see our Close() method.
    nsCOMPtr<nsIUDPSocketListener> listener;
    {
      MutexAutoLock lock(mLock);
      mListener.swap(listener);
    }

    if (listener) {
      listener->OnStopListening(this, mCondition);
      NS_ProxyRelease(mListenerTarget, listener);
    }
  }
}

void
nsUDPSocket::IsLocal(bool *aIsLocal)
{
  // If bound to loopback, this UDP socket only accepts local connections.
  *aIsLocal = mAddr.raw.family == nsINetAddr::FAMILY_LOCAL;
}

//-----------------------------------------------------------------------------
// nsSocket::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsUDPSocket, nsIUDPSocket)


//-----------------------------------------------------------------------------
// nsSocket::nsISocket
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsUDPSocket::Init(int32_t aPort, bool aLoopbackOnly, nsIPrincipal *aPrincipal,
                  bool aAddressReuse, uint8_t aOptionalArgc)
{
  NetAddr addr;

  if (aPort < 0)
    aPort = 0;

  addr.raw.family = AF_INET;
  addr.inet.port = htons(aPort);

  if (aLoopbackOnly)
    addr.inet.ip = htonl(INADDR_LOOPBACK);
  else
    addr.inet.ip = htonl(INADDR_ANY);

  return InitWithAddress(&addr, aPrincipal, aAddressReuse, aOptionalArgc);
}

NS_IMETHODIMP
nsUDPSocket::InitWithAddress(const NetAddr *aAddr, nsIPrincipal *aPrincipal,
                             bool aAddressReuse, uint8_t aOptionalArgc)
{
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);

  bool addressReuse = (aOptionalArgc == 1) ? aAddressReuse : true;

  //
  // configure listening socket...
  //

  mFD = PR_OpenUDPSocket(aAddr->raw.family);
  if (!mFD)
  {
    NS_WARNING("unable to create UDP socket");
    return NS_ERROR_FAILURE;
  }

  if (aPrincipal) {
    nsresult rv = aPrincipal->GetAppId(&mAppId);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = aPrincipal->GetIsInBrowserElement(&mIsInBrowserElement);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

#ifdef MOZ_WIDGET_GONK
  if (mAppId != NECKO_UNKNOWN_APP_ID) {
    nsCOMPtr<nsINetworkInterface> activeNetwork;
    GetActiveNetworkInterface(activeNetwork);
    mActiveNetwork =
      new nsMainThreadPtrHolder<nsINetworkInterface>(activeNetwork);
  }
#endif

  uint16_t port;
  if (NS_FAILED(net::GetPort(aAddr, &port))) {
    NS_WARNING("invalid bind address");
    goto fail;
  }

  PRSocketOptionData opt;

  // Linux kernel will sometimes hand out a used port if we bind
  // to port 0 with SO_REUSEADDR
  if (port) {
    opt.option = PR_SockOpt_Reuseaddr;
    opt.value.reuse_addr = addressReuse;
    PR_SetSocketOption(mFD, &opt);
  }

  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = true;
  PR_SetSocketOption(mFD, &opt);

  PRNetAddr addr;
  PR_InitializeNetAddr(PR_IpAddrAny, 0, &addr);
  NetAddrToPRNetAddr(aAddr, &addr);

  if (PR_Bind(mFD, &addr) != PR_SUCCESS)
  {
    NS_WARNING("failed to bind socket");
    goto fail;
  }

  // get the resulting socket address, which may be different than what
  // we passed to bind.
  if (PR_GetSockName(mFD, &addr) != PR_SUCCESS)
  {
    NS_WARNING("cannot get socket name");
    goto fail;
  }

  PRNetAddrToNetAddr(&addr, &mAddr);

  // create proxy via NetworkActivityMonitor
  NetworkActivityMonitor::AttachIOLayer(mFD);

  // wait until AsyncListen is called before polling the socket for
  // client connections.
  return NS_OK;

fail:
  Close();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsUDPSocket::Close()
{
  {
    MutexAutoLock lock(mLock);
    // we want to proxy the close operation to the socket thread if a listener
    // has been set.  otherwise, we should just close the socket here...
    if (!mListener)
    {
      if (mFD)
      {
        // Here we want to go directly with closing the socket since some tests
        // expects this happen synchronously.
        PR_Close(mFD);
        mFD = nullptr;
      }
      SaveNetworkStats(true);
      return NS_OK;
    }
  }
  return PostEvent(this, &nsUDPSocket::OnMsgClose);
}

NS_IMETHODIMP
nsUDPSocket::GetPort(int32_t *aResult)
{
  // no need to enter the lock here
  uint16_t result;
  nsresult rv = net::GetPort(&mAddr, &result);
  *aResult = static_cast<int32_t>(result);
  return rv;
}

NS_IMETHODIMP
nsUDPSocket::GetLocalAddr(nsINetAddr * *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsCOMPtr<nsINetAddr> result = new nsNetAddr(&mAddr);
  result.forget(aResult);

  return NS_OK;
}

void
nsUDPSocket::SaveNetworkStats(bool aEnforce)
{
#ifdef MOZ_WIDGET_GONK
  if (!mActiveNetwork || mAppId == NECKO_UNKNOWN_APP_ID) {
    return;
  }

  if (mByteReadCount == 0 && mByteWriteCount == 0) {
    return;
  }

  uint64_t total = mByteReadCount + mByteWriteCount;
  if (aEnforce || total > NETWORK_STATS_THRESHOLD) {
    // Create the event to save the network statistics.
    // the event is then dispathed to the main thread.
    nsRefPtr<nsRunnable> event =
      new SaveNetworkStatsEvent(mAppId, mIsInBrowserElement, mActiveNetwork,
                                mByteReadCount, mByteWriteCount, false);
    NS_DispatchToMainThread(event);

    // Reset the counters after saving.
    mByteReadCount = 0;
    mByteWriteCount = 0;
  }
#endif
}

NS_IMETHODIMP
nsUDPSocket::GetAddress(NetAddr *aResult)
{
  // no need to enter the lock here
  memcpy(aResult, &mAddr, sizeof(mAddr));
  return NS_OK;
}

namespace {
//-----------------------------------------------------------------------------
// SocketListenerProxy
//-----------------------------------------------------------------------------
class SocketListenerProxy final : public nsIUDPSocketListener
{
  ~SocketListenerProxy() {}

public:
  explicit SocketListenerProxy(nsIUDPSocketListener* aListener)
    : mListener(new nsMainThreadPtrHolder<nsIUDPSocketListener>(aListener))
    , mTargetThread(do_GetCurrentThread())
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIUDPSOCKETLISTENER

  class OnPacketReceivedRunnable : public nsRunnable
  {
  public:
    OnPacketReceivedRunnable(const nsMainThreadPtrHandle<nsIUDPSocketListener>& aListener,
                             nsIUDPSocket* aSocket,
                             nsIUDPMessage* aMessage)
      : mListener(aListener)
      , mSocket(aSocket)
      , mMessage(aMessage)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUDPSocketListener> mListener;
    nsCOMPtr<nsIUDPSocket> mSocket;
    nsCOMPtr<nsIUDPMessage> mMessage;
  };

  class OnStopListeningRunnable : public nsRunnable
  {
  public:
    OnStopListeningRunnable(const nsMainThreadPtrHandle<nsIUDPSocketListener>& aListener,
                            nsIUDPSocket* aSocket,
                            nsresult aStatus)
      : mListener(aListener)
      , mSocket(aSocket)
      , mStatus(aStatus)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUDPSocketListener> mListener;
    nsCOMPtr<nsIUDPSocket> mSocket;
    nsresult mStatus;
  };

private:
  nsMainThreadPtrHandle<nsIUDPSocketListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
};

NS_IMPL_ISUPPORTS(SocketListenerProxy,
                  nsIUDPSocketListener)

NS_IMETHODIMP
SocketListenerProxy::OnPacketReceived(nsIUDPSocket* aSocket,
                                      nsIUDPMessage* aMessage)
{
  nsRefPtr<OnPacketReceivedRunnable> r =
    new OnPacketReceivedRunnable(mListener, aSocket, aMessage);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
SocketListenerProxy::OnStopListening(nsIUDPSocket* aSocket,
                                     nsresult aStatus)
{
  nsRefPtr<OnStopListeningRunnable> r =
    new OnStopListeningRunnable(mListener, aSocket, aStatus);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
SocketListenerProxy::OnPacketReceivedRunnable::Run()
{
  NetAddr netAddr;
  nsCOMPtr<nsINetAddr> nsAddr;
  mMessage->GetFromAddr(getter_AddRefs(nsAddr));
  nsAddr->GetNetAddr(&netAddr);

  nsCOMPtr<nsIOutputStream> outputStream;
  mMessage->GetOutputStream(getter_AddRefs(outputStream));

  FallibleTArray<uint8_t>& data = mMessage->GetDataAsTArray();

  nsCOMPtr<nsIUDPMessage> message = new nsUDPMessage(&netAddr,
                                                     outputStream,
                                                     data);
  mListener->OnPacketReceived(mSocket, message);
  return NS_OK;
}

NS_IMETHODIMP
SocketListenerProxy::OnStopListeningRunnable::Run()
{
  mListener->OnStopListening(mSocket, mStatus);
  return NS_OK;
}

class PendingSend : public nsIDNSListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  PendingSend(nsUDPSocket *aSocket, uint16_t aPort,
              FallibleTArray<uint8_t> &aData)
      : mSocket(aSocket)
      , mPort(aPort)
  {
    mData.SwapElements(aData);
  }

private:
  virtual ~PendingSend() {}

  nsRefPtr<nsUDPSocket> mSocket;
  uint16_t mPort;
  FallibleTArray<uint8_t> mData;
};

NS_IMPL_ISUPPORTS(PendingSend, nsIDNSListener)

NS_IMETHODIMP
PendingSend::OnLookupComplete(nsICancelable *request,
                              nsIDNSRecord  *rec,
                              nsresult       status)
{
  if (NS_FAILED(status)) {
    NS_WARNING("Failed to send UDP packet due to DNS lookup failure");
    return NS_OK;
  }

  NetAddr addr;
  if (NS_SUCCEEDED(rec->GetNextAddr(mPort, &addr))) {
    uint32_t count;
    nsresult rv = mSocket->SendWithAddress(&addr, mData.Elements(),
                                           mData.Length(), &count);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

class PendingSendStream : public nsIDNSListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  PendingSendStream(nsUDPSocket *aSocket, uint16_t aPort,
                    nsIInputStream *aStream)
      : mSocket(aSocket)
      , mPort(aPort)
      , mStream(aStream) {}

private:
  virtual ~PendingSendStream() {}

  nsRefPtr<nsUDPSocket> mSocket;
  uint16_t mPort;
  nsCOMPtr<nsIInputStream> mStream;
};

NS_IMPL_ISUPPORTS(PendingSendStream, nsIDNSListener)

NS_IMETHODIMP
PendingSendStream::OnLookupComplete(nsICancelable *request,
                                    nsIDNSRecord  *rec,
                                    nsresult       status)
{
  if (NS_FAILED(status)) {
    NS_WARNING("Failed to send UDP packet due to DNS lookup failure");
    return NS_OK;
  }

  NetAddr addr;
  if (NS_SUCCEEDED(rec->GetNextAddr(mPort, &addr))) {
    nsresult rv = mSocket->SendBinaryStreamWithAddress(&addr, mStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

class SendRequestRunnable: public nsRunnable {
public:
  SendRequestRunnable(nsUDPSocket *aSocket,
                      const NetAddr &aAddr,
                      FallibleTArray<uint8_t> &aData)
    : mSocket(aSocket)
    , mAddr(aAddr)
    , mData(aData)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsRefPtr<nsUDPSocket> mSocket;
  const NetAddr mAddr;
  FallibleTArray<uint8_t> mData;
};

NS_IMETHODIMP
SendRequestRunnable::Run()
{
  uint32_t count;
  mSocket->SendWithAddress(&mAddr, mData.Elements(),
                           mData.Length(), &count);
  return NS_OK;
}

} // anonymous namespace

NS_IMETHODIMP
nsUDPSocket::AsyncListen(nsIUDPSocketListener *aListener)
{
  // ensuring mFD implies ensuring mLock
  NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mListener == nullptr, NS_ERROR_IN_PROGRESS);
  {
    MutexAutoLock lock(mLock);
    mListener = new SocketListenerProxy(aListener);
    mListenerTarget = NS_GetCurrentThread();
  }
  return PostEvent(this, &nsUDPSocket::OnMsgAttach);
}

NS_IMETHODIMP
nsUDPSocket::Send(const nsACString &aHost, uint16_t aPort,
                  const uint8_t *aData, uint32_t aDataLength,
                  uint32_t *_retval)
{
  NS_ENSURE_ARG(aData);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 0;

  FallibleTArray<uint8_t> fallibleArray;
  if (!fallibleArray.InsertElementsAt(0, aData, aDataLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDNSListener> listener = new PendingSend(this, aPort, fallibleArray);

  nsresult rv = ResolveHost(aHost, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = aDataLength;
  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::SendWithAddr(nsINetAddr *aAddr, const uint8_t *aData,
                          uint32_t aDataLength, uint32_t *_retval)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);
  NS_ENSURE_ARG_POINTER(_retval);

  NetAddr netAddr;
  aAddr->GetNetAddr(&netAddr);
  return SendWithAddress(&netAddr, aData, aDataLength, _retval);
}

NS_IMETHODIMP
nsUDPSocket::SendWithAddress(const NetAddr *aAddr, const uint8_t *aData,
                             uint32_t aDataLength, uint32_t *_retval)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aData);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = 0;

  PRNetAddr prAddr;
  NetAddrToPRNetAddr(aAddr, &prAddr);

  bool onSTSThread = false;
  mSts->IsOnCurrentThread(&onSTSThread);

  if (onSTSThread) {
    MutexAutoLock lock(mLock);
    if (!mFD) {
      // socket is not initialized or has been closed
      return NS_ERROR_FAILURE;
    }
    int32_t count = PR_SendTo(mFD, aData, sizeof(uint8_t) *aDataLength,
                              0, &prAddr, PR_INTERVAL_NO_WAIT);
    if (count < 0) {
      PRErrorCode code = PR_GetError();
      return ErrorAccordingToNSPR(code);
    }
    this->AddOutputBytes(count);
    *_retval = count;
  } else {
    FallibleTArray<uint8_t> fallibleArray;
    if (!fallibleArray.InsertElementsAt(0, aData, aDataLength)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mSts->Dispatch(new SendRequestRunnable(this, *aAddr, fallibleArray),
                                 NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
    *_retval = aDataLength;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::SendBinaryStream(const nsACString &aHost, uint16_t aPort,
                              nsIInputStream *aStream)
{
  NS_ENSURE_ARG(aStream);

  nsCOMPtr<nsIDNSListener> listener = new PendingSendStream(this, aPort, aStream);

  return ResolveHost(aHost, listener);
}

NS_IMETHODIMP
nsUDPSocket::SendBinaryStreamWithAddress(const NetAddr *aAddr, nsIInputStream *aStream)
{
  NS_ENSURE_ARG(aAddr);
  NS_ENSURE_ARG(aStream);

  PRNetAddr prAddr;
  PR_InitializeNetAddr(PR_IpAddrAny, 0, &prAddr);
  NetAddrToPRNetAddr(aAddr, &prAddr);

  nsRefPtr<nsUDPOutputStream> os = new nsUDPOutputStream(this, mFD, prAddr);
  return NS_AsyncCopy(aStream, os, mSts, NS_ASYNCCOPY_VIA_READSEGMENTS,
                      UDP_PACKET_CHUNK_SIZE);
}

nsresult
nsUDPSocket::SetSocketOption(const PRSocketOptionData& aOpt)
{
  bool onSTSThread = false;
  mSts->IsOnCurrentThread(&onSTSThread);

  if (!onSTSThread) {
    // Dispatch to STS thread and re-enter this method there
    nsCOMPtr<nsIRunnable> runnable = new SetSocketOptionRunnable(this, aOpt);
    nsresult rv = mSts->Dispatch(runnable, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (PR_SetSocketOption(mFD, &aOpt) != PR_SUCCESS) {
    SOCKET_LOG(("nsUDPSocket::SetSocketOption [this=%p] failed for type %d, "
      "error %d\n", this, aOpt.option, PR_GetError()));
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::JoinMulticast(const nsACString& aAddr, const nsACString& aIface)
{
  if (NS_WARN_IF(aAddr.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prAddr;
  if (PR_StringToNetAddr(aAddr.BeginReading(), &prAddr) != PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  PRNetAddr prIface;
  if (aIface.IsEmpty()) {
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &prIface);
  } else {
    if (PR_StringToNetAddr(aIface.BeginReading(), &prIface) != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
  }

  return JoinMulticastInternal(prAddr, prIface);
}

NS_IMETHODIMP
nsUDPSocket::JoinMulticastAddr(const NetAddr aAddr, const NetAddr* aIface)
{
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prAddr;
  NetAddrToPRNetAddr(&aAddr, &prAddr);

  PRNetAddr prIface;
  if (!aIface) {
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &prIface);
  } else {
    NetAddrToPRNetAddr(aIface, &prIface);
  }

  return JoinMulticastInternal(prAddr, prIface);
}

nsresult
nsUDPSocket::JoinMulticastInternal(const PRNetAddr& aAddr,
                                   const PRNetAddr& aIface)
{
  PRSocketOptionData opt;

  opt.option = PR_SockOpt_AddMember;
  opt.value.add_member.mcaddr = aAddr;
  opt.value.add_member.ifaddr = aIface;

  nsresult rv = SetSocketOption(opt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::LeaveMulticast(const nsACString& aAddr, const nsACString& aIface)
{
  if (NS_WARN_IF(aAddr.IsEmpty())) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prAddr;
  if (PR_StringToNetAddr(aAddr.BeginReading(), &prAddr) != PR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  PRNetAddr prIface;
  if (aIface.IsEmpty()) {
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &prIface);
  } else {
    if (PR_StringToNetAddr(aIface.BeginReading(), &prIface) != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
  }

  return LeaveMulticastInternal(prAddr, prIface);
}

NS_IMETHODIMP
nsUDPSocket::LeaveMulticastAddr(const NetAddr aAddr, const NetAddr* aIface)
{
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prAddr;
  NetAddrToPRNetAddr(&aAddr, &prAddr);

  PRNetAddr prIface;
  if (!aIface) {
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &prIface);
  } else {
    NetAddrToPRNetAddr(aIface, &prIface);
  }

  return LeaveMulticastInternal(prAddr, prIface);
}

nsresult
nsUDPSocket::LeaveMulticastInternal(const PRNetAddr& aAddr,
                                    const PRNetAddr& aIface)
{
  PRSocketOptionData opt;

  opt.option = PR_SockOpt_DropMember;
  opt.value.drop_member.mcaddr = aAddr;
  opt.value.drop_member.ifaddr = aIface;

  nsresult rv = SetSocketOption(opt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::GetMulticastLoopback(bool* aLoopback)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUDPSocket::SetMulticastLoopback(bool aLoopback)
{
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRSocketOptionData opt;

  opt.option = PR_SockOpt_McastLoopback;
  opt.value.mcast_loopback = aLoopback;

  nsresult rv = SetSocketOption(opt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocket::GetMulticastInterface(nsACString& aIface)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUDPSocket::GetMulticastInterfaceAddr(NetAddr* aIface)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsUDPSocket::SetMulticastInterface(const nsACString& aIface)
{
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prIface;
  if (aIface.IsEmpty()) {
    PR_InitializeNetAddr(PR_IpAddrAny, 0, &prIface);
  } else {
    if (PR_StringToNetAddr(aIface.BeginReading(), &prIface) != PR_SUCCESS) {
      return NS_ERROR_FAILURE;
    }
  }

  return SetMulticastInterfaceInternal(prIface);
}

NS_IMETHODIMP
nsUDPSocket::SetMulticastInterfaceAddr(NetAddr aIface)
{
  if (NS_WARN_IF(!mFD)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  PRNetAddr prIface;
  NetAddrToPRNetAddr(&aIface, &prIface);

  return SetMulticastInterfaceInternal(prIface);
}

nsresult
nsUDPSocket::SetMulticastInterfaceInternal(const PRNetAddr& aIface)
{
  PRSocketOptionData opt;

  opt.option = PR_SockOpt_McastInterface;
  opt.value.mcast_if = aIface;

  nsresult rv = SetSocketOption(opt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

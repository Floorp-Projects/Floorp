/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIServiceManager.h"
#include "nsSocketTransport2.h"
#include "nsUDPServerSocket.h"
#include "nsProxyRelease.h"
#include "nsAutoPtr.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "prnetdb.h"
#include "prio.h"
#include "mozilla/Attributes.h"
#include "nsNetAddr.h"
#include "nsNetSegmentUtils.h"
#include "nsStreamUtils.h"
#include "nsIPipe.h"
#include "prerror.h"
#include "nsINSSErrorsService.h"

using namespace mozilla::net;
using namespace mozilla;

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------

typedef void (nsUDPServerSocket:: *nsUDPServerSocketFunc)(void);

static nsresult
PostEvent(nsUDPServerSocket *s, nsUDPServerSocketFunc func)
{
  nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(s, func);

  if (!gSocketTransportService)
    return NS_ERROR_FAILURE;

  return gSocketTransportService->Dispatch(ev, NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------
// nsUPDOutputStream impl
//-----------------------------------------------------------------------------
NS_IMPL_THREADSAFE_ISUPPORTS1(nsUDPOutputStream, nsIOutputStream)

nsUDPOutputStream::nsUDPOutputStream(nsUDPServerSocket* aServer,
                                     PRFileDesc* aFD,
                                     PRNetAddr& aPrClientAddr)
  : mServer(aServer)
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

  mServer->AddOutputBytes(count);

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
// nsUPDMessage impl
//-----------------------------------------------------------------------------
NS_IMPL_THREADSAFE_ISUPPORTS1(nsUDPMessage, nsIUDPMessage)

nsUDPMessage::nsUDPMessage(PRNetAddr* aAddr,
             nsIOutputStream* aOutputStream,
             const nsACString& aData)
  : mOutputStream(aOutputStream)
  , mData(aData)
{
  memcpy(&mAddr, aAddr, sizeof(NetAddr));
}

nsUDPMessage::~nsUDPMessage()
{
}

/* readonly attribute nsINetAddr from; */
NS_IMETHODIMP nsUDPMessage::GetFromAddr(nsINetAddr * *aFromAddr)
{
  NS_ENSURE_ARG_POINTER(aFromAddr);

  NetAddr clientAddr;
  PRNetAddrToNetAddr(&mAddr, &clientAddr);

  nsCOMPtr<nsINetAddr> result = new nsNetAddr(&clientAddr);
  result.forget(aFromAddr);

  return NS_OK;
}

/* readonly attribute ACString data; */
NS_IMETHODIMP nsUDPMessage::GetData(nsACString & aData)
{
  aData = mData;
  return NS_OK;
}

/* readonly attribute nsIOutputStream outputStream; */
NS_IMETHODIMP nsUDPMessage::GetOutputStream(nsIOutputStream * *aOutputStream)
{
  NS_ENSURE_ARG_POINTER(aOutputStream);
  NS_IF_ADDREF(*aOutputStream = mOutputStream);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsServerSocket
//-----------------------------------------------------------------------------

nsUDPServerSocket::nsUDPServerSocket()
  : mLock("nsUDPServerSocket.mLock")
  , mFD(nullptr)
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
  MOZ_COUNT_CTOR(nsUDPServerSocket);
}

nsUDPServerSocket::~nsUDPServerSocket()
{
  Close(); // just in case :)

  MOZ_COUNT_DTOR(nsUDPServerSocket);
}

void
nsUDPServerSocket::AddOutputBytes(uint64_t aBytes)
{
  mByteWriteCount += aBytes;
}

void
nsUDPServerSocket::OnMsgClose()
{
  SOCKET_LOG(("nsServerSocket::OnMsgClose [this=%p]\n", this));

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
nsUDPServerSocket::OnMsgAttach()
{
  SOCKET_LOG(("nsServerSocket::OnMsgAttach [this=%p]\n", this));

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
nsUDPServerSocket::TryAttach()
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
      NS_NewRunnableMethod(this, &nsUDPServerSocket::OnMsgAttach);

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

//-----------------------------------------------------------------------------
// nsServerSocket::nsASocketHandler
//-----------------------------------------------------------------------------

void
nsUDPServerSocket::OnSocketReady(PRFileDesc *fd, int16_t outFlags)
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
  char buff[1500];
  count = PR_RecvFrom(mFD, buff, sizeof(buff), 0, &prClientAddr, PR_INTERVAL_NO_WAIT);

  if (count < 1) {
    NS_WARNING("error of recvfrom on UDP socket");
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }
  mByteReadCount += count;

  nsCString data;
  if (!data.Assign(buff, count, mozilla::fallible_t())) {
    mCondition = NS_ERROR_OUT_OF_MEMORY;
    return;
  }

  nsCOMPtr<nsIAsyncInputStream> pipeIn;
  nsCOMPtr<nsIAsyncOutputStream> pipeOut;

  uint32_t segsize = 1400;
  uint32_t segcount = 0;
  net_ResolveSegmentParams(segsize, segcount);
  nsresult rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut),
                true, true, segsize, segcount);

  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsUDPOutputStream> os = new nsUDPOutputStream(this, mFD, prClientAddr);
  rv = NS_AsyncCopy(pipeIn, os, mSts,
                    NS_ASYNCCOPY_VIA_READSEGMENTS, 1400);

  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIUDPMessage> message = new nsUDPMessage(&prClientAddr, pipeOut, data);
  mListener->OnPacketReceived(this, message);
}

void
nsUDPServerSocket::OnSocketDetached(PRFileDesc *fd)
{
  // force a failure condition if none set; maybe the STS is shutting down :-/
  if (NS_SUCCEEDED(mCondition))
    mCondition = NS_ERROR_ABORT;

  if (mFD)
  {
    NS_ASSERTION(mFD == fd, "wrong file descriptor");
    PR_Close(mFD);
    mFD = nullptr;
  }

  if (mListener)
  {
    // need to atomically clear mListener.  see our Close() method.
    nsCOMPtr<nsIUDPServerSocketListener> listener;
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
nsUDPServerSocket::IsLocal(bool *aIsLocal)
{
  // If bound to loopback, this server socket only accepts local connections.
  *aIsLocal = mAddr.raw.family == nsINetAddr::FAMILY_LOCAL;
}

//-----------------------------------------------------------------------------
// nsServerSocket::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUDPServerSocket, nsIUDPServerSocket)


//-----------------------------------------------------------------------------
// nsServerSocket::nsIServerSocket
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsUDPServerSocket::Init(int32_t aPort, bool aLoopbackOnly)
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

  return InitWithAddress(&addr);
}

NS_IMETHODIMP
nsUDPServerSocket::InitWithAddress(const NetAddr *aAddr)
{
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);

  //
  // configure listening socket...
  //

  mFD = PR_OpenUDPSocket(aAddr->raw.family);
  if (!mFD)
  {
    NS_WARNING("unable to create server socket");
    return NS_ERROR_FAILURE;
  }

  PRSocketOptionData opt;

  opt.option = PR_SockOpt_Reuseaddr;
  opt.value.reuse_addr = true;
  PR_SetSocketOption(mFD, &opt);

  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = true;
  PR_SetSocketOption(mFD, &opt);

  PRNetAddr addr;
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

  // wait until AsyncListen is called before polling the socket for
  // client connections.
  return NS_OK;

fail:
  Close();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsUDPServerSocket::Close()
{
  {
    MutexAutoLock lock(mLock);
    // we want to proxy the close operation to the socket thread if a listener
    // has been set.  otherwise, we should just close the socket here...
    if (!mListener)
    {
      if (mFD)
      {
        PR_Close(mFD);
        mFD = nullptr;
      }
      return NS_OK;
    }
  }
  return PostEvent(this, &nsUDPServerSocket::OnMsgClose);
}

NS_IMETHODIMP
nsUDPServerSocket::GetPort(int32_t *aResult)
{
  // no need to enter the lock here
  uint16_t port;
  if (mAddr.raw.family == PR_AF_INET)
    port = mAddr.inet.port;
  else if (mAddr.raw.family == PR_AF_INET6)
    port = mAddr.inet6.port;
  else
    return NS_ERROR_NOT_INITIALIZED;

  *aResult = (int32_t) PR_ntohs(port);
  return NS_OK;
}

NS_IMETHODIMP
nsUDPServerSocket::GetAddress(NetAddr *aResult)
{
  // no need to enter the lock here
  memcpy(aResult, &mAddr, sizeof(mAddr));
  return NS_OK;
}

namespace {

class ServerSocketListenerProxy MOZ_FINAL : public nsIUDPServerSocketListener
{
public:
  ServerSocketListenerProxy(nsIUDPServerSocketListener* aListener)
    : mListener(new nsMainThreadPtrHolder<nsIUDPServerSocketListener>(aListener))
    , mTargetThread(do_GetCurrentThread())
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUDPSERVERSOCKETLISTENER

  class OnPacketReceivedRunnable : public nsRunnable
  {
  public:
    OnPacketReceivedRunnable(const nsMainThreadPtrHandle<nsIUDPServerSocketListener>& aListener,
                     nsIUDPServerSocket* aServ,
                     nsIUDPMessage* aMessage)
      : mListener(aListener)
      , mServ(aServ)
      , mMessage(aMessage)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUDPServerSocketListener> mListener;
    nsCOMPtr<nsIUDPServerSocket> mServ;
    nsCOMPtr<nsIUDPMessage> mMessage;
  };

  class OnStopListeningRunnable : public nsRunnable
  {
  public:
    OnStopListeningRunnable(const nsMainThreadPtrHandle<nsIUDPServerSocketListener>& aListener,
                            nsIUDPServerSocket* aServ,
                            nsresult aStatus)
      : mListener(aListener)
      , mServ(aServ)
      , mStatus(aStatus)
    { }

    NS_DECL_NSIRUNNABLE

  private:
    nsMainThreadPtrHandle<nsIUDPServerSocketListener> mListener;
    nsCOMPtr<nsIUDPServerSocket> mServ;
    nsresult mStatus;
  };

private:
  nsMainThreadPtrHandle<nsIUDPServerSocketListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(ServerSocketListenerProxy,
                              nsIUDPServerSocketListener)

NS_IMETHODIMP
ServerSocketListenerProxy::OnPacketReceived(nsIUDPServerSocket* aServ,
                                            nsIUDPMessage* aMessage)
{
  nsRefPtr<OnPacketReceivedRunnable> r =
    new OnPacketReceivedRunnable(mListener, aServ, aMessage);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnStopListening(nsIUDPServerSocket* aServ,
                                           nsresult aStatus)
{
  nsRefPtr<OnStopListeningRunnable> r =
    new OnStopListeningRunnable(mListener, aServ, aStatus);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnPacketReceivedRunnable::Run()
{
  mListener->OnPacketReceived(mServ, mMessage);
  return NS_OK;
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnStopListeningRunnable::Run()
{
  mListener->OnStopListening(mServ, mStatus);
  return NS_OK;
}

} // anonymous namespace

NS_IMETHODIMP
nsUDPServerSocket::AsyncListen(nsIUDPServerSocketListener *aListener)
{
  // ensuring mFD implies ensuring mLock
  NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mListener == nullptr, NS_ERROR_IN_PROGRESS);
  {
    MutexAutoLock lock(mLock);
    mListener = new ServerSocketListenerProxy(aListener);
    mListenerTarget = NS_GetCurrentThread();
  }
  return PostEvent(this, &nsUDPServerSocket::OnMsgAttach);
}

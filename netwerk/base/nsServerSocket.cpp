/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSocketTransport2.h"
#include "nsServerSocket.h"
#include "nsProxyRelease.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "prnetdb.h"
#include "prio.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/net/DNS.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------

using nsServerSocketFunc = void (nsServerSocket::*)();

static nsresult PostEvent(nsServerSocket* s, nsServerSocketFunc func) {
  nsCOMPtr<nsIRunnable> ev = NewRunnableMethod("net::PostEvent", s, func);
  if (!gSocketTransportService) return NS_ERROR_FAILURE;

  return gSocketTransportService->Dispatch(ev, NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------
// nsServerSocket
//-----------------------------------------------------------------------------

nsServerSocket::nsServerSocket() {
  // we want to be able to access the STS directly, and it may not have been
  // constructed yet.  the STS constructor sets gSocketTransportService.
  if (!gSocketTransportService) {
    // This call can fail if we're offline, for example.
    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  }
  // make sure the STS sticks around as long as we do
  NS_IF_ADDREF(gSocketTransportService);
}

nsServerSocket::~nsServerSocket() {
  Close();  // just in case :)

  // release our reference to the STS
  nsSocketTransportService* serv = gSocketTransportService;
  NS_IF_RELEASE(serv);
}

void nsServerSocket::OnMsgClose() {
  SOCKET_LOG(("nsServerSocket::OnMsgClose [this=%p]\n", this));

  if (NS_FAILED(mCondition)) return;

  // tear down socket.  this signals the STS to detach our socket handler.
  mCondition = NS_BINDING_ABORTED;

  // if we are attached, then we'll close the socket in our OnSocketDetached.
  // otherwise, call OnSocketDetached from here.
  if (!mAttached) OnSocketDetached(mFD);
}

void nsServerSocket::OnMsgAttach() {
  SOCKET_LOG(("nsServerSocket::OnMsgAttach [this=%p]\n", this));

  if (NS_FAILED(mCondition)) return;

  mCondition = TryAttach();

  // if we hit an error while trying to attach then bail...
  if (NS_FAILED(mCondition)) {
    NS_ASSERTION(!mAttached, "should not be attached already");
    OnSocketDetached(mFD);
  }
}

nsresult nsServerSocket::TryAttach() {
  nsresult rv;

  if (!gSocketTransportService) return NS_ERROR_FAILURE;

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
  if (!gSocketTransportService->CanAttachSocket()) {
    nsCOMPtr<nsIRunnable> event = NewRunnableMethod(
        "net::nsServerSocket::OnMsgAttach", this, &nsServerSocket::OnMsgAttach);
    if (!event) return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = gSocketTransportService->NotifyWhenCanAttachSocket(event);
    if (NS_FAILED(rv)) return rv;
  }

  //
  // ok, we can now attach our socket to the STS for polling
  //
  rv = gSocketTransportService->AttachSocket(mFD, this);
  if (NS_FAILED(rv)) return rv;

  mAttached = true;

  //
  // now, configure our poll flags for listening...
  //
  mPollFlags = (PR_POLL_READ | PR_POLL_EXCEPT);
  return NS_OK;
}

void nsServerSocket::CreateClientTransport(PRFileDesc* aClientFD,
                                           const NetAddr& aClientAddr) {
  RefPtr<nsSocketTransport> trans = new nsSocketTransport;
  if (NS_WARN_IF(!trans)) {
    mCondition = NS_ERROR_OUT_OF_MEMORY;
    return;
  }

  nsresult rv = trans->InitWithConnectedSocket(aClientFD, &aClientAddr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mCondition = rv;
    return;
  }

  mListener->OnSocketAccepted(this, trans);
}

//-----------------------------------------------------------------------------
// nsServerSocket::nsASocketHandler
//-----------------------------------------------------------------------------

void nsServerSocket::OnSocketReady(PRFileDesc* fd, int16_t outFlags) {
  NS_ASSERTION(NS_SUCCEEDED(mCondition), "oops");
  NS_ASSERTION(mFD == fd, "wrong file descriptor");
  NS_ASSERTION(outFlags != -1, "unexpected timeout condition reached");

  if (outFlags & (PR_POLL_ERR | PR_POLL_HUP | PR_POLL_NVAL)) {
    NS_WARNING("error polling on listening socket");
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }

  PRFileDesc* clientFD;
  PRNetAddr prClientAddr;

  // NSPR doesn't tell us the peer address's length (as provided by the
  // 'accept' system call), so we can't distinguish between named,
  // unnamed, and abstract peer addresses. Clear prClientAddr first, so
  // that the path will at least be reliably empty for unnamed and
  // abstract addresses, and not garbage when the peer is unnamed.
  memset(&prClientAddr, 0, sizeof(prClientAddr));

  clientFD = PR_Accept(mFD, &prClientAddr, PR_INTERVAL_NO_WAIT);
  if (!clientFD) {
    NS_WARNING("PR_Accept failed");
    mCondition = NS_ERROR_UNEXPECTED;
    return;
  }
  PR_SetFDInheritable(clientFD, false);

  NetAddr clientAddr(&prClientAddr);
  // Accept succeeded, create socket transport and notify consumer
  CreateClientTransport(clientFD, clientAddr);
}

void nsServerSocket::OnSocketDetached(PRFileDesc* fd) {
  // force a failure condition if none set; maybe the STS is shutting down :-/
  if (NS_SUCCEEDED(mCondition)) mCondition = NS_ERROR_ABORT;

  if (mFD) {
    NS_ASSERTION(mFD == fd, "wrong file descriptor");
    PR_Close(mFD);
    mFD = nullptr;
  }

  if (mListener) {
    mListener->OnStopListening(this, mCondition);

    // need to atomically clear mListener.  see our Close() method.
    RefPtr<nsIServerSocketListener> listener = nullptr;
    {
      MutexAutoLock lock(mLock);
      listener = ToRefPtr(std::move(mListener));
    }

    // XXX we need to proxy the release to the listener's target thread to work
    // around bug 337492.
    if (listener) {
      NS_ProxyRelease("nsServerSocket::mListener", mListenerTarget,
                      listener.forget());
    }
  }
}

void nsServerSocket::IsLocal(bool* aIsLocal) {
#if defined(XP_UNIX)
  // Unix-domain sockets are always local.
  if (mAddr.raw.family == PR_AF_LOCAL) {
    *aIsLocal = true;
    return;
  }
#endif

  // If bound to loopback, this server socket only accepts local connections.
  *aIsLocal = PR_IsNetAddrType(&mAddr, PR_IpAddrLoopback);
}

void nsServerSocket::KeepWhenOffline(bool* aKeepWhenOffline) {
  *aKeepWhenOffline = mKeepWhenOffline;
}

//-----------------------------------------------------------------------------
// nsServerSocket::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsServerSocket, nsIServerSocket)

//-----------------------------------------------------------------------------
// nsServerSocket::nsIServerSocket
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsServerSocket::Init(int32_t aPort, bool aLoopbackOnly, int32_t aBackLog) {
  return InitSpecialConnection(aPort, aLoopbackOnly ? LoopbackOnly : 0,
                               aBackLog);
}

NS_IMETHODIMP
nsServerSocket::InitIPv6(int32_t aPort, bool aLoopbackOnly, int32_t aBackLog) {
  PRNetAddrValue val;
  PRNetAddr addr;

  if (aPort < 0) {
    aPort = 0;
  }
  if (aLoopbackOnly) {
    val = PR_IpAddrLoopback;
  } else {
    val = PR_IpAddrAny;
  }
  PR_SetNetAddr(val, PR_AF_INET6, aPort, &addr);

  mKeepWhenOffline = false;
  return InitWithAddress(&addr, aBackLog);
}

NS_IMETHODIMP
nsServerSocket::InitWithFilename(nsIFile* aPath, uint32_t aPermissions,
                                 int32_t aBacklog) {
#if defined(XP_UNIX)
  nsresult rv;

  nsAutoCString path;
  rv = aPath->GetNativePath(path);
  if (NS_FAILED(rv)) return rv;

  // Create a Unix domain PRNetAddr referring to the given path.
  PRNetAddr addr;
  if (path.Length() > sizeof(addr.local.path) - 1) {
    return NS_ERROR_FILE_NAME_TOO_LONG;
  }
  addr.local.family = PR_AF_LOCAL;
  memcpy(addr.local.path, path.get(), path.Length());
  addr.local.path[path.Length()] = '\0';

  rv = InitWithAddress(&addr, aBacklog);
  if (NS_FAILED(rv)) return rv;

  return aPath->SetPermissions(aPermissions);
#else
  return NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED;
#endif
}

NS_IMETHODIMP
nsServerSocket::InitWithAbstractAddress(const nsACString& aName,
                                        int32_t aBacklog) {
  // Abstract socket address is supported on Linux and Android only.
  // If not Linux, we should return error.
#if defined(XP_LINUX)
  // Create an abstract socket address PRNetAddr referring to the name
  PRNetAddr addr;
  if (aName.Length() > sizeof(addr.local.path) - 2) {
    return NS_ERROR_FILE_NAME_TOO_LONG;
  }
  addr.local.family = PR_AF_LOCAL;
  addr.local.path[0] = 0;
  memcpy(addr.local.path + 1, aName.BeginReading(), aName.Length());
  addr.local.path[aName.Length() + 1] = 0;

  return InitWithAddress(&addr, aBacklog);
#else
  return NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED;
#endif
}

NS_IMETHODIMP
nsServerSocket::InitSpecialConnection(int32_t aPort, nsServerSocketFlag aFlags,
                                      int32_t aBackLog) {
  PRNetAddrValue val;
  PRNetAddr addr;

  if (aPort < 0) aPort = 0;
  if (aFlags & nsIServerSocket::LoopbackOnly) {
    val = PR_IpAddrLoopback;
  } else {
    val = PR_IpAddrAny;
  }
  PR_SetNetAddr(val, PR_AF_INET, aPort, &addr);

  mKeepWhenOffline = ((aFlags & nsIServerSocket::KeepWhenOffline) != 0);
  return InitWithAddress(&addr, aBackLog);
}

NS_IMETHODIMP
nsServerSocket::InitWithAddress(const PRNetAddr* aAddr, int32_t aBackLog) {
  NS_ENSURE_TRUE(mFD == nullptr, NS_ERROR_ALREADY_INITIALIZED);
  nsresult rv;

  //
  // configure listening socket...
  //

  mFD = PR_OpenTCPSocket(aAddr->raw.family);
  if (!mFD) {
    NS_WARNING("unable to create server socket");
    return ErrorAccordingToNSPR(PR_GetError());
  }

  PR_SetFDInheritable(mFD, false);

  PRSocketOptionData opt;

  opt.option = PR_SockOpt_Reuseaddr;
  opt.value.reuse_addr = true;
  PR_SetSocketOption(mFD, &opt);

  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = true;
  PR_SetSocketOption(mFD, &opt);

  if (PR_Bind(mFD, aAddr) != PR_SUCCESS) {
    NS_WARNING("failed to bind socket");
    goto fail;
  }

  if (aBackLog < 0) aBackLog = 5;  // seems like a reasonable default

  if (PR_Listen(mFD, aBackLog) != PR_SUCCESS) {
    NS_WARNING("cannot listen on socket");
    goto fail;
  }

  // get the resulting socket address, which may be different than what
  // we passed to bind.
  if (PR_GetSockName(mFD, &mAddr) != PR_SUCCESS) {
    NS_WARNING("cannot get socket name");
    goto fail;
  }

  // Set any additional socket defaults needed by child classes
  rv = SetSocketDefaults();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    goto fail;
  }

  // wait until AsyncListen is called before polling the socket for
  // client connections.
  return NS_OK;

fail:
  rv = ErrorAccordingToNSPR(PR_GetError());
  Close();
  return rv;
}

NS_IMETHODIMP
nsServerSocket::Close() {
  {
    MutexAutoLock lock(mLock);
    // we want to proxy the close operation to the socket thread if a listener
    // has been set.  otherwise, we should just close the socket here...
    if (!mListener) {
      if (mFD) {
        PR_Close(mFD);
        mFD = nullptr;
      }
      return NS_OK;
    }
  }
  return PostEvent(this, &nsServerSocket::OnMsgClose);
}

namespace {

class ServerSocketListenerProxy final : public nsIServerSocketListener {
  ~ServerSocketListenerProxy() = default;

 public:
  explicit ServerSocketListenerProxy(nsIServerSocketListener* aListener)
      : mListener(new nsMainThreadPtrHolder<nsIServerSocketListener>(
            "ServerSocketListenerProxy::mListener", aListener)),
        mTarget(GetCurrentEventTarget()) {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

  class OnSocketAcceptedRunnable : public Runnable {
   public:
    OnSocketAcceptedRunnable(
        const nsMainThreadPtrHandle<nsIServerSocketListener>& aListener,
        nsIServerSocket* aServ, nsISocketTransport* aTransport)
        : Runnable("net::ServerSocketListenerProxy::OnSocketAcceptedRunnable"),
          mListener(aListener),
          mServ(aServ),
          mTransport(aTransport) {}

    NS_DECL_NSIRUNNABLE

   private:
    nsMainThreadPtrHandle<nsIServerSocketListener> mListener;
    nsCOMPtr<nsIServerSocket> mServ;
    nsCOMPtr<nsISocketTransport> mTransport;
  };

  class OnStopListeningRunnable : public Runnable {
   public:
    OnStopListeningRunnable(
        const nsMainThreadPtrHandle<nsIServerSocketListener>& aListener,
        nsIServerSocket* aServ, nsresult aStatus)
        : Runnable("net::ServerSocketListenerProxy::OnStopListeningRunnable"),
          mListener(aListener),
          mServ(aServ),
          mStatus(aStatus) {}

    NS_DECL_NSIRUNNABLE

   private:
    nsMainThreadPtrHandle<nsIServerSocketListener> mListener;
    nsCOMPtr<nsIServerSocket> mServ;
    nsresult mStatus;
  };

 private:
  nsMainThreadPtrHandle<nsIServerSocketListener> mListener;
  nsCOMPtr<nsIEventTarget> mTarget;
};

NS_IMPL_ISUPPORTS(ServerSocketListenerProxy, nsIServerSocketListener)

NS_IMETHODIMP
ServerSocketListenerProxy::OnSocketAccepted(nsIServerSocket* aServ,
                                            nsISocketTransport* aTransport) {
  RefPtr<OnSocketAcceptedRunnable> r =
      new OnSocketAcceptedRunnable(mListener, aServ, aTransport);
  return mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnStopListening(nsIServerSocket* aServ,
                                           nsresult aStatus) {
  RefPtr<OnStopListeningRunnable> r =
      new OnStopListeningRunnable(mListener, aServ, aStatus);
  return mTarget->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnSocketAcceptedRunnable::Run() {
  mListener->OnSocketAccepted(mServ, mTransport);
  return NS_OK;
}

NS_IMETHODIMP
ServerSocketListenerProxy::OnStopListeningRunnable::Run() {
  mListener->OnStopListening(mServ, mStatus);
  return NS_OK;
}

}  // namespace

NS_IMETHODIMP
nsServerSocket::AsyncListen(nsIServerSocketListener* aListener) {
  // ensuring mFD implies ensuring mLock
  NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mListener == nullptr, NS_ERROR_IN_PROGRESS);
  {
    MutexAutoLock lock(mLock);
    mListener = new ServerSocketListenerProxy(aListener);
    mListenerTarget = GetCurrentEventTarget();
  }

  // Child classes may need to do additional setup just before listening begins
  nsresult rv = OnSocketListen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return PostEvent(this, &nsServerSocket::OnMsgAttach);
}

NS_IMETHODIMP
nsServerSocket::GetPort(int32_t* aResult) {
  // no need to enter the lock here
  uint16_t port;
  if (mAddr.raw.family == PR_AF_INET) {
    port = mAddr.inet.port;
  } else if (mAddr.raw.family == PR_AF_INET6) {
    port = mAddr.ipv6.port;
  } else {
    return NS_ERROR_FAILURE;
  }

  *aResult = static_cast<int32_t>(NetworkEndian::readUint16(&port));
  return NS_OK;
}

NS_IMETHODIMP
nsServerSocket::GetAddress(PRNetAddr* aResult) {
  // no need to enter the lock here
  memcpy(aResult, &mAddr, sizeof(mAddr));
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

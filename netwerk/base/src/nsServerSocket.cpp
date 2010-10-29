/* vim:set ts=2 sw=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsSocketTransport2.h"
#include "nsServerSocket.h"
#include "nsProxyRelease.h"
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "nsNetError.h"
#include "nsNetCID.h"
#include "prnetdb.h"
#include "prio.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------

typedef void (nsServerSocket:: *nsServerSocketFunc)(void);

static nsresult
PostEvent(nsServerSocket *s, nsServerSocketFunc func)
{
  nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(s, func);
  if (!ev)
    return NS_ERROR_OUT_OF_MEMORY;

  return gSocketTransportService->Dispatch(ev, NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------
// nsServerSocket
//-----------------------------------------------------------------------------

nsServerSocket::nsServerSocket()
  : mLock(nsnull)
  , mFD(nsnull)
  , mAttached(PR_FALSE)
{
  // we want to be able to access the STS directly, and it may not have been
  // constructed yet.  the STS constructor sets gSocketTransportService.
  if (!gSocketTransportService)
  {
    nsCOMPtr<nsISocketTransportService> sts =
        do_GetService(kSocketTransportServiceCID);
    NS_ASSERTION(sts, "no socket transport service");
  }
  // make sure the STS sticks around as long as we do
  NS_ADDREF(gSocketTransportService);
}

nsServerSocket::~nsServerSocket()
{
  Close(); // just in case :)

  if (mLock)
    PR_DestroyLock(mLock);

  // release our reference to the STS
  nsSocketTransportService *serv = gSocketTransportService;
  NS_RELEASE(serv);
}

void
nsServerSocket::OnMsgClose()
{
  SOCKET_LOG(("nsServerSocket::OnMsgClose [this=%p]\n", this));

  if (NS_FAILED(mCondition))
    return;

  // tear down socket.  this signals the STS to detach our socket handler.
  mCondition = NS_BINDING_ABORTED;

  // if we are attached, then we'll close the socket in our OnSocketDetached.
  // otherwise, call OnSocketDetached from here.
  if (!mAttached)
    OnSocketDetached(mFD);
}

void
nsServerSocket::OnMsgAttach()
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
nsServerSocket::TryAttach()
{
  nsresult rv;

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
      NS_NewRunnableMethod(this, &nsServerSocket::OnMsgAttach);
    if (!event)
      return NS_ERROR_OUT_OF_MEMORY;

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

  mAttached = PR_TRUE;

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
nsServerSocket::OnSocketReady(PRFileDesc *fd, PRInt16 outFlags)
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

  PRFileDesc *clientFD;
  PRNetAddr clientAddr;

  clientFD = PR_Accept(mFD, &clientAddr, PR_INTERVAL_NO_WAIT);
  if (!clientFD)
  {
    NS_WARNING("PR_Accept failed");
    mCondition = NS_ERROR_UNEXPECTED;
  }
  else
  {
    nsRefPtr<nsSocketTransport> trans = new nsSocketTransport;
    if (!trans)
      mCondition = NS_ERROR_OUT_OF_MEMORY;
    else
    {
      nsresult rv = trans->InitWithConnectedSocket(clientFD, &clientAddr);
      if (NS_FAILED(rv))
        mCondition = rv;
      else
        mListener->OnSocketAccepted(this, trans);
    }
  }
}

void
nsServerSocket::OnSocketDetached(PRFileDesc *fd)
{
  // force a failure condition if none set; maybe the STS is shutting down :-/
  if (NS_SUCCEEDED(mCondition))
    mCondition = NS_ERROR_ABORT;

  if (mFD)
  {
    NS_ASSERTION(mFD == fd, "wrong file descriptor");
    PR_Close(mFD);
    mFD = nsnull;
  }

  if (mListener)
  {
    mListener->OnStopListening(this, mCondition);

    // need to atomically clear mListener.  see our Close() method.
    nsIServerSocketListener *listener = nsnull;
    {
      nsAutoLock lock(mLock);
      mListener.swap(listener);
    }
    // XXX we need to proxy the release to the listener's target thread to work
    // around bug 337492.
    if (listener)
      NS_ProxyRelease(mListenerTarget, listener);
  }
}


//-----------------------------------------------------------------------------
// nsServerSocket::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsServerSocket, nsIServerSocket)


//-----------------------------------------------------------------------------
// nsServerSocket::nsIServerSocket
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsServerSocket::Init(PRInt32 aPort, PRBool aLoopbackOnly, PRInt32 aBackLog)
{
  PRNetAddrValue val;
  PRNetAddr addr;

  if (aPort < 0)
    aPort = 0;
  if (aLoopbackOnly)
    val = PR_IpAddrLoopback;
  else
    val = PR_IpAddrAny;
  PR_SetNetAddr(val, PR_AF_INET, aPort, &addr);

  return InitWithAddress(&addr, aBackLog);
}

NS_IMETHODIMP
nsServerSocket::InitWithAddress(const PRNetAddr *aAddr, PRInt32 aBackLog)
{
  NS_ENSURE_TRUE(mFD == nsnull, NS_ERROR_ALREADY_INITIALIZED);

  if (!mLock)
  {
    mLock = PR_NewLock();
    if (!mLock)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  //
  // configure listening socket...
  //

  mFD = PR_OpenTCPSocket(aAddr->raw.family);
  if (!mFD)
  {
    NS_WARNING("unable to create server socket");
    return NS_ERROR_FAILURE;
  }

  PRSocketOptionData opt;

  opt.option = PR_SockOpt_Reuseaddr;
  opt.value.reuse_addr = PR_TRUE;
  PR_SetSocketOption(mFD, &opt);

  opt.option = PR_SockOpt_Nonblocking;
  opt.value.non_blocking = PR_TRUE;
  PR_SetSocketOption(mFD, &opt);

  if (PR_Bind(mFD, aAddr) != PR_SUCCESS)
  {
    NS_WARNING("failed to bind socket");
    goto fail;
  }

  if (aBackLog < 0)
    aBackLog = 5; // seems like a reasonable default

  if (PR_Listen(mFD, aBackLog) != PR_SUCCESS)
  {
    NS_WARNING("cannot listen on socket");
    goto fail;
  }

  // get the resulting socket address, which may be different than what
  // we passed to bind.
  if (PR_GetSockName(mFD, &mAddr) != PR_SUCCESS)
  {
    NS_WARNING("cannot get socket name");
    goto fail;
  }

  // wait until AsyncListen is called before polling the socket for
  // client connections.
  return NS_OK;

fail:
  Close();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsServerSocket::Close()
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_NOT_INITIALIZED);
  {
    nsAutoLock lock(mLock);
    // we want to proxy the close operation to the socket thread if a listener
    // has been set.  otherwise, we should just close the socket here...
    if (!mListener)
    {
      if (mFD)
      {
        PR_Close(mFD);
        mFD = nsnull;
      }
      return NS_OK;
    }
  }
  return PostEvent(this, &nsServerSocket::OnMsgClose);
}

NS_IMETHODIMP
nsServerSocket::AsyncListen(nsIServerSocketListener *aListener)
{
  // ensuring mFD implies ensuring mLock
  NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(mListener == nsnull, NS_ERROR_IN_PROGRESS);
  {
    nsAutoLock lock(mLock);
    nsresult rv = NS_GetProxyForObject(NS_PROXY_TO_CURRENT_THREAD,
                                       NS_GET_IID(nsIServerSocketListener),
                                       aListener,
                                       NS_PROXY_ASYNC | NS_PROXY_ALWAYS,
                                       getter_AddRefs(mListener));
    if (NS_FAILED(rv))
      return rv;
    mListenerTarget = NS_GetCurrentThread();
  }
  return PostEvent(this, &nsServerSocket::OnMsgAttach);
}

NS_IMETHODIMP
nsServerSocket::GetPort(PRInt32 *aResult)
{
  // no need to enter the lock here
  PRUint16 port;
  if (mAddr.raw.family == PR_AF_INET)
    port = mAddr.inet.port;
  else
    port = mAddr.ipv6.port;
  *aResult = (PRInt32) PR_ntohs(port);
  return NS_OK;
}

NS_IMETHODIMP
nsServerSocket::GetAddress(PRNetAddr *aResult)
{
  // no need to enter the lock here
  memcpy(aResult, &mAddr, sizeof(mAddr));
  return NS_OK;
}

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include "nsThreadUtils.h"
#include "nsSSLThread.h"
#include "nsAutoLock.h"
#include "nsNSSIOLayer.h"

#include "ssl.h"

nsSSLThread::nsSSLThread()
: mBusySocket(nsnull),
  mSocketScheduledToBeDestroyed(nsnull),
  mPendingHTTPRequest(nsnull)
{
  NS_ASSERTION(!ssl_thread_singleton, "nsSSLThread is a singleton, caller attempts to create another instance!");
  
  ssl_thread_singleton = this;
}

nsSSLThread::~nsSSLThread()
{
  ssl_thread_singleton = nsnull;
}

PRFileDesc *nsSSLThread::getRealSSLFD(nsNSSSocketInfo *si)
{
  if (!ssl_thread_singleton || !si || !ssl_thread_singleton->mThreadHandle)
    return nsnull;

  nsAutoLock threadLock(ssl_thread_singleton->mMutex);

  if (si->mThreadData->mReplacedSSLFileDesc)
  {
    return si->mThreadData->mReplacedSSLFileDesc;
  }
  else
  {
    return si->mFd->lower;
  }
}

PRStatus nsSSLThread::requestGetsockname(nsNSSSocketInfo *si, PRNetAddr *addr)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return PR_FAILURE;

  return fd->methods->getsockname(fd, addr);
}

PRStatus nsSSLThread::requestGetpeername(nsNSSSocketInfo *si, PRNetAddr *addr)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return PR_FAILURE;

  return fd->methods->getpeername(fd, addr);
}

PRStatus nsSSLThread::requestGetsocketoption(nsNSSSocketInfo *si, 
                                             PRSocketOptionData *data)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return PR_FAILURE;

  return fd->methods->getsocketoption(fd, data);
}

PRStatus nsSSLThread::requestSetsocketoption(nsNSSSocketInfo *si, 
                                             const PRSocketOptionData *data)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return PR_FAILURE;

  return fd->methods->setsocketoption(fd, data);
}

PRStatus nsSSLThread::requestConnectcontinue(nsNSSSocketInfo *si, 
                                             PRInt16 out_flags)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return PR_FAILURE;

  return fd->methods->connectcontinue(fd, out_flags);
}

PRInt32 nsSSLThread::requestRecvMsgPeek(nsNSSSocketInfo *si, void *buf, PRInt32 amount,
                                        PRIntn flags, PRIntervalTime timeout)
{
  if (!ssl_thread_singleton || !si || !ssl_thread_singleton->mThreadHandle)
  {
    PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
    return -1;
  }

  PRFileDesc *realSSLFD;

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    if (si == ssl_thread_singleton->mBusySocket)
    {
      PORT_SetError(PR_WOULD_BLOCK_ERROR);
      return -1;
    }

    switch (si->mThreadData->mSSLState)
    {
      case nsSSLSocketThreadData::ssl_idle:
        break;
    
      case nsSSLSocketThreadData::ssl_reading_done:
        {
          // we have data available that we can return

          // if there was a failure, just return the failure,
          // but do not yet clear our state, that should happen
          // in the call to "read".

          if (si->mThreadData->mSSLResultRemainingBytes < 0) {
            if (si->mThreadData->mPRErrorCode != PR_SUCCESS) {
              PR_SetError(si->mThreadData->mPRErrorCode, 0);
            }

            return si->mThreadData->mSSLResultRemainingBytes;
          }

          PRInt32 return_amount = NS_MIN(amount, si->mThreadData->mSSLResultRemainingBytes);

          memcpy(buf, si->mThreadData->mSSLRemainingReadResultData, return_amount);

          return return_amount;
        }

      case nsSSLSocketThreadData::ssl_writing_done:
      case nsSSLSocketThreadData::ssl_pending_write:
      case nsSSLSocketThreadData::ssl_pending_read:

      // for safety reasons, also return would_block on any other state,
      // although this switch statement should be complete and list
      // the appropriate behaviour for each state.
      default:
        {
          PORT_SetError(PR_WOULD_BLOCK_ERROR);
          return -1;
        }
    }

    if (si->mThreadData->mReplacedSSLFileDesc)
    {
      realSSLFD = si->mThreadData->mReplacedSSLFileDesc;
    }
    else
    {
      realSSLFD = si->mFd->lower;
    }
  }

  return realSSLFD->methods->recv(realSSLFD, buf, amount, flags, timeout);
}

nsresult nsSSLThread::requestActivateSSL(nsNSSSocketInfo *si)
{
  PRFileDesc *fd = getRealSSLFD(si);
  if (!fd)
    return NS_ERROR_FAILURE;

  if (SECSuccess != SSL_OptionSet(fd, SSL_SECURITY, PR_TRUE))
    return NS_ERROR_FAILURE;

  if (SECSuccess != SSL_ResetHandshake(fd, PR_FALSE))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

PRInt16 nsSSLThread::requestPoll(nsNSSSocketInfo *si, PRInt16 in_flags, PRInt16 *out_flags)
{
  if (!ssl_thread_singleton || !si || !ssl_thread_singleton->mThreadHandle)
    return 0;

  *out_flags = 0;
  
  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    if (ssl_thread_singleton->mBusySocket)
    {
      // If there is currently a socket busy on the SSL thread,
      // use our tricky implementation.
      
      switch (si->mThreadData->mSSLState)
      {
        case nsSSLSocketThreadData::ssl_writing_done:
        {
          if (in_flags & PR_POLL_WRITE)
          {
            *out_flags |= PR_POLL_WRITE;
          }

          return in_flags;
        }
        break;
        
        case nsSSLSocketThreadData::ssl_reading_done:
        {
          if (in_flags & PR_POLL_READ)
          {
            *out_flags |= PR_POLL_READ;
          }

          return in_flags;
        }
        break;
        
        case nsSSLSocketThreadData::ssl_pending_write:
        case nsSSLSocketThreadData::ssl_pending_read:
        {
          if (si == ssl_thread_singleton->mBusySocket)
          {
            // The lower layer of the socket is currently the pollable event,
            // which signals the readable state.
            
            return PR_POLL_READ;
          }
          
          // We should never get here, well, at least not with the current
          // implementation of SSL thread, where we have one worker only.
          // While another socket is busy, this socket "si" 
          // can not be marked with pending I/O at the same time.
          
          NS_NOTREACHED("Socket not busy on SSL thread marked as pending");
          return 0;
        }
        break;
        
        case nsSSLSocketThreadData::ssl_idle:
        {
          if (si != ssl_thread_singleton->mBusySocket)
          {
            // Some other socket is currently busy on the SSL thread.
            // That busy socket could currently be blocked (e.g. by UI)
            // If socket "si" had become readable/writeable,
            // and we reported the event to the caller,
            // the caller would end up in a busy loop. continously trying to
            // do I/O, although our read/write function will not be able to
            // (because of the blocked busy socket on the SSL thread).
            // To avoid that scenario, for socket "si".we will report 
            // "neither readable nor writeable" to the caller 
            // Let's fake our caller did not ask for readable or writeable
            // when querying the lower layer.
            
            in_flags &= ~(PR_POLL_READ | PR_POLL_WRITE);
          }
        }
        break;
        
        default:
          break;
      }
    }
  }

  return si->mFd->lower->methods->poll(si->mFd->lower, in_flags, out_flags);
}

PRStatus nsSSLThread::requestClose(nsNSSSocketInfo *si)
{
  if (!ssl_thread_singleton || !si)
    return PR_FAILURE;

  PRBool close_later = PR_FALSE;
  nsIRequest* requestToCancel = nsnull;

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    if (ssl_thread_singleton->mBusySocket == si) {
    
      // That's tricky, SSL thread is currently busy with this socket,
      // and might even be blocked on it (UI or OCSP).
      // We should not close the socket directly, but rather
      // schedule closing it, at the time the SSL thread is done.
      // If there is indeed a depending OCSP request pending,
      // we should cancel it now.
      
      if (ssl_thread_singleton->mPendingHTTPRequest)
      {
        requestToCancel = ssl_thread_singleton->mPendingHTTPRequest;
        ssl_thread_singleton->mPendingHTTPRequest = nsnull;
      }
      
      close_later = PR_TRUE;
      ssl_thread_singleton->mSocketScheduledToBeDestroyed = si;
    }
  }

  if (requestToCancel)
  {
    if (NS_IsMainThread())
    {
      requestToCancel->Cancel(NS_ERROR_ABORT);
    }
    else
    {
      NS_WARNING("Attempt to close SSL socket from a thread that is not the main thread. Can not cancel pending HTTP request from NSS");
    }
  
    NS_RELEASE(requestToCancel);
  }
  
  if (!close_later)
  {
    return si->CloseSocketAndDestroy();
  }
  
  return PR_SUCCESS;
}

void nsSSLThread::restoreOriginalSocket_locked(nsNSSSocketInfo *si)
{
  if (si->mThreadData->mReplacedSSLFileDesc)
  {
    if (nsSSLIOLayerHelpers::mPollableEventCurrentlySet)
    {
      nsSSLIOLayerHelpers::mPollableEventCurrentlySet = PR_FALSE;
      PR_WaitForPollableEvent(nsSSLIOLayerHelpers::mSharedPollableEvent);
    }

    // need to restore
    si->mFd->lower = si->mThreadData->mReplacedSSLFileDesc;
    si->mThreadData->mReplacedSSLFileDesc = nsnull;
    nsSSLIOLayerHelpers::mSocketOwningPollableEvent = nsnull;
  }
}

PRInt32 nsSSLThread::requestRead(nsNSSSocketInfo *si, void *buf, PRInt32 amount)
{
  if (!ssl_thread_singleton || !si || !buf || !amount || !ssl_thread_singleton->mThreadHandle)
  {
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return -1;
  }

  PRBool this_socket_is_busy = PR_FALSE;
  PRBool some_other_socket_is_busy = PR_FALSE;
  nsSSLSocketThreadData::ssl_state my_ssl_state;

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    if (ssl_thread_singleton->mExitRequested) {
      PR_SetError(PR_UNKNOWN_ERROR, 0);
      return -1;
    }

    my_ssl_state = si->mThreadData->mSSLState;

    if (ssl_thread_singleton->mBusySocket == si)
    {
      this_socket_is_busy = PR_TRUE;

      if (my_ssl_state == nsSSLSocketThreadData::ssl_reading_done)
      {
        // we will now care for the data that's ready,
        // the socket is no longer busy on the ssl thread
        
        restoreOriginalSocket_locked(si);

        ssl_thread_singleton->mBusySocket = nsnull;
        
        // We'll handle the results further down,
        // while not holding the lock.
      }
    }
    else if (ssl_thread_singleton->mBusySocket)
    {
      some_other_socket_is_busy = PR_TRUE;
    }
  }

  switch (my_ssl_state)
  {
    case nsSSLSocketThreadData::ssl_idle:
      {
        NS_ASSERTION(!this_socket_is_busy, "oops, unexpected incosistency");
        
        if (some_other_socket_is_busy)
        {
          PORT_SetError(PR_WOULD_BLOCK_ERROR);
          return -1;
        }
        
        // ssl thread is not busy, we'll continue below
      }
      break;

    case nsSSLSocketThreadData::ssl_reading_done:
      // there has been a previous request to read, that is now done!
      {
        // failure ?
        if (si->mThreadData->mSSLResultRemainingBytes < 0) {
          if (si->mThreadData->mPRErrorCode != PR_SUCCESS) {
            PR_SetError(si->mThreadData->mPRErrorCode, 0);
            si->mThreadData->mPRErrorCode = PR_SUCCESS;
          }

          si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_idle;
          return si->mThreadData->mSSLResultRemainingBytes;
        }

        PRInt32 return_amount = NS_MIN(amount, si->mThreadData->mSSLResultRemainingBytes);

        memcpy(buf, si->mThreadData->mSSLRemainingReadResultData, return_amount);

        si->mThreadData->mSSLResultRemainingBytes -= return_amount;

        if (!si->mThreadData->mSSLResultRemainingBytes) {
          si->mThreadData->mSSLRemainingReadResultData = nsnull;
          si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_idle;
        }
        else {
          si->mThreadData->mSSLRemainingReadResultData += return_amount;
        }

        return return_amount;
      }
      // we never arrive here, see return statement above
      break;


    // We should not see the following events here,
    // because we have not yet signaled Necko that we are 
    // readable/writable again, so if we end up here,
    // it means that Necko decided to try read/write again,
    // for whatever reason. No problem, just return would_block,
    case nsSSLSocketThreadData::ssl_pending_write:
    case nsSSLSocketThreadData::ssl_pending_read:

    // We should not see this state here, because Necko has previously
    // requested us to write, Necko is not yet aware that it's done,
    // (although it meanwhile is), but Necko now tries to read?
    // If that ever happens, it's confusing, but not a problem,
    // just let Necko know we can not do that now and return would_block.
    case nsSSLSocketThreadData::ssl_writing_done:

    // for safety reasons, also return would_block on any other state,
    // although this switch statement should be complete and list
    // the appropriate behaviour for each state.
    default:
      {
        PORT_SetError(PR_WOULD_BLOCK_ERROR);
        return -1;
      }
      // we never arrive here, see return statement above
      break;
  }

  if (si->isPK11LoggedOut() || si->isAlreadyShutDown()) {
    PR_SetError(PR_SOCKET_SHUTDOWN_ERROR, 0);
    return -1;
  }

  if (si->GetCanceled()) {
    return PR_FAILURE;
  }

  // si is idle and good, and no other socket is currently busy,
  // so it's fine to continue with the request.

  if (!si->mThreadData->ensure_buffer_size(amount))
  {
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return -1;
  }
  
  si->mThreadData->mSSLRequestedTransferAmount = amount;
  si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_pending_read;

  // Remember we are operating on a layered file descriptor, that consists of
  // a PSM code layer (nsNSSIOLayer), a NSS code layer (SSL protocol logic), 
  // and the raw socket at the bottommost layer.
  //
  // We don't want to call the SSL layer read/write directly on this thread, 
  // because it might block, should a callback to UI (for user confirmation)
  // or Necko (for retrieving OCSP verification data) be necessary.
  // As Necko is single threaded, it is currently waiting for this 
  // function to return, and a callback into Necko from NSS couldn't succeed.
  // 
  // Therefore we must defer the request to read/write to a separate SSL thread.
  // We will return WOULD_BLOCK to Necko, and will return the real results
  // once the I/O operation on the SSL thread is ready.
  //
  // The tricky part is to wake up Necko, as soon as the I/O operation 
  // on the SSL thread is done.
  //
  // In order to achieve that, we manipulate the layering of the file 
  // descriptor. Usually the PSM layer points to the SSL layer as its lower 
  // layer. We change that to a pollable event file descriptor.
  //
  // Once we return from this original read/write function, Necko will 
  // poll/select on the file descriptor. As result data is not yet ready, we will 
  // instruct Necko to select on the bottommost file descriptor 
  // (by using appropriate flags in PSM's layer implementation of the 
  // poll method), which is the pollable event.
  //
  // Once the SSL thread is done with the call to the SSL layer, it will 
  // "set" the pollable event, causing Necko to wake up on the file descriptor 
  // and call read/write again. Now that the file descriptor is in the done state, 
  // we'll arrive in this read/write function again. We'll detect the socket is 
  // in the done state, and restore the original SSL level file descriptor.
  // Finally, we return the data obtained on the SSL thread back to our caller.

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    NS_ASSERTION(!nsSSLIOLayerHelpers::mSocketOwningPollableEvent, 
                 "oops, some other socket still owns our shared pollable event");
    NS_ASSERTION(!si->mThreadData->mReplacedSSLFileDesc, "oops");

    si->mThreadData->mReplacedSSLFileDesc = si->mFd->lower;
    si->mFd->lower = nsSSLIOLayerHelpers::mSharedPollableEvent;
    nsSSLIOLayerHelpers::mSocketOwningPollableEvent = si;
    ssl_thread_singleton->mBusySocket = si;

    // notify the thread
    PR_NotifyAllCondVar(ssl_thread_singleton->mCond);
  }

  PORT_SetError(PR_WOULD_BLOCK_ERROR);
  return -1;
}

PRInt32 nsSSLThread::requestWrite(nsNSSSocketInfo *si, const void *buf, PRInt32 amount)
{
  if (!ssl_thread_singleton || !si || !buf || !amount || !ssl_thread_singleton->mThreadHandle)
  {
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return -1;
  }

  PRBool this_socket_is_busy = PR_FALSE;
  PRBool some_other_socket_is_busy = PR_FALSE;
  nsSSLSocketThreadData::ssl_state my_ssl_state;
  
  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);
    
    if (ssl_thread_singleton->mExitRequested) {
      PR_SetError(PR_UNKNOWN_ERROR, 0);
      return -1;
    }

    my_ssl_state = si->mThreadData->mSSLState;

    if (ssl_thread_singleton->mBusySocket == si)
    {
      this_socket_is_busy = PR_TRUE;
      
      if (my_ssl_state == nsSSLSocketThreadData::ssl_writing_done)
      {
        // we will now care for the data that's ready,
        // the socket is no longer busy on the ssl thread
        
        restoreOriginalSocket_locked(si);

        ssl_thread_singleton->mBusySocket = nsnull;
        
        // We'll handle the results further down,
        // while not holding the lock.
      }
    }
    else if (ssl_thread_singleton->mBusySocket)
    {
      some_other_socket_is_busy = PR_TRUE;
    }
  }

  switch (my_ssl_state)
  {
    case nsSSLSocketThreadData::ssl_idle:
      {
        NS_ASSERTION(!this_socket_is_busy, "oops, unexpected incosistency");
        
        if (some_other_socket_is_busy)
        {
          PORT_SetError(PR_WOULD_BLOCK_ERROR);
          return -1;
        }
        
        // ssl thread is not busy, we'll continue below
      }
      break;

    case nsSSLSocketThreadData::ssl_writing_done:
      // there has been a previous request to write, that is now done!
      {
        // failure ?
        if (si->mThreadData->mSSLResultRemainingBytes < 0) {
          if (si->mThreadData->mPRErrorCode != PR_SUCCESS) {
            PR_SetError(si->mThreadData->mPRErrorCode, 0);
            si->mThreadData->mPRErrorCode = PR_SUCCESS;
          }

          si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_idle;
          return si->mThreadData->mSSLResultRemainingBytes;
        }

        PRInt32 return_amount = NS_MIN(amount, si->mThreadData->mSSLResultRemainingBytes);

        si->mThreadData->mSSLResultRemainingBytes -= return_amount;

        if (!si->mThreadData->mSSLResultRemainingBytes) {
          si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_idle;
        }

        return return_amount;
      }
      break;
      
    // We should not see the following events here,
    // because we have not yet signaled Necko that we are 
    // readable/writable again, so if we end up here,
    // it means that Necko decided to try read/write again,
    // for whatever reason. No problem, just return would_block,
    case nsSSLSocketThreadData::ssl_pending_write:
    case nsSSLSocketThreadData::ssl_pending_read:

    // We should not see this state here, because Necko has previously
    // requested us to read, Necko is not yet aware that it's done,
    // (although it meanwhile is), but Necko now tries to write?
    // If that ever happens, it's confusing, but not a problem,
    // just let Necko know we can not do that now and return would_block.
    case nsSSLSocketThreadData::ssl_reading_done:

    // for safety reasons, also return would_block on any other state,
    // although this switch statement should be complete and list
    // the appropriate behaviour for each state.
    default:
      {
        PORT_SetError(PR_WOULD_BLOCK_ERROR);
        return -1;
      }
      // we never arrive here, see return statement above
      break;
  }

  if (si->isPK11LoggedOut() || si->isAlreadyShutDown()) {
    PR_SetError(PR_SOCKET_SHUTDOWN_ERROR, 0);
    return -1;
  }

  if (si->GetCanceled()) {
    return PR_FAILURE;
  }

  // si is idle and good, and no other socket is currently busy,
  // so it's fine to continue with the request.

  if (!si->mThreadData->ensure_buffer_size(amount))
  {
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return -1;
  }

  memcpy(si->mThreadData->mSSLDataBuffer, buf, amount);
  si->mThreadData->mSSLRequestedTransferAmount = amount;
  si->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_pending_write;

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);

    NS_ASSERTION(!nsSSLIOLayerHelpers::mSocketOwningPollableEvent, 
                 "oops, some other socket still owns our shared pollable event");
    NS_ASSERTION(!si->mThreadData->mReplacedSSLFileDesc, "oops");

    si->mThreadData->mReplacedSSLFileDesc = si->mFd->lower;
    si->mFd->lower = nsSSLIOLayerHelpers::mSharedPollableEvent;
    nsSSLIOLayerHelpers::mSocketOwningPollableEvent = si;
    ssl_thread_singleton->mBusySocket = si;

    PR_NotifyAllCondVar(ssl_thread_singleton->mCond);
  }

  PORT_SetError(PR_WOULD_BLOCK_ERROR);
  return -1;
}

void nsSSLThread::Run(void)
{
  // Helper variable, we don't want to call destroy 
  // while holding the mutex.
  nsNSSSocketInfo *socketToDestroy = nsnull;

  while (PR_TRUE)
  {
    if (socketToDestroy)
    {
      socketToDestroy->CloseSocketAndDestroy();
      socketToDestroy = nsnull;
    }

    // remember whether we'll write or read
    nsSSLSocketThreadData::ssl_state busy_socket_ssl_state;
  
    {
      // In this scope we need mutex protection,
      // as we find out what needs to be done.
      
      nsAutoLock threadLock(ssl_thread_singleton->mMutex);
      
      if (mSocketScheduledToBeDestroyed)
      {
        if (mBusySocket == mSocketScheduledToBeDestroyed)
        {
          // That's rare, but it happens.
          // We have received a request to close the socket,
          // although I/O results have not yet been consumed.

          restoreOriginalSocket_locked(mBusySocket);

          mBusySocket->mThreadData->mSSLState = nsSSLSocketThreadData::ssl_idle;
          mBusySocket = nsnull;
        }
      
        socketToDestroy = mSocketScheduledToBeDestroyed;
        mSocketScheduledToBeDestroyed = nsnull;
        continue; // go back and finally destroy it, before doing anything else
      }

      if (mExitRequested)
        break;

      PRBool pending_work = PR_FALSE;

      do
      {
        if (mBusySocket
            &&
              (mBusySocket->mThreadData->mSSLState == nsSSLSocketThreadData::ssl_pending_read
              ||
              mBusySocket->mThreadData->mSSLState == nsSSLSocketThreadData::ssl_pending_write))
        {
          pending_work = PR_TRUE;
        }

        if (!pending_work)
        {
          // no work to do ? let's wait a moment

          PRIntervalTime wait_time = PR_TicksPerSecond() / 4;
          PR_WaitCondVar(mCond, wait_time);
        }
        
      } while (!pending_work && !mExitRequested && !mSocketScheduledToBeDestroyed);
      
      if (mSocketScheduledToBeDestroyed)
        continue;
      
      if (mExitRequested)
        break;
      
      if (!pending_work)
        continue;
      
      busy_socket_ssl_state = mBusySocket->mThreadData->mSSLState;
    }

    {
      // In this scope we need to make sure NSS does not go away
      // while we are busy.
    
      nsNSSShutDownPreventionLock locker;

      if (nsSSLSocketThreadData::ssl_pending_write == busy_socket_ssl_state)
      {
        PRInt32 bytesWritten = mBusySocket->mThreadData->mReplacedSSLFileDesc->methods
          ->write(mBusySocket->mThreadData->mReplacedSSLFileDesc, 
                  mBusySocket->mThreadData->mSSLDataBuffer, 
                  mBusySocket->mThreadData->mSSLRequestedTransferAmount);

#ifdef DEBUG_SSL_VERBOSE
        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] wrote %d bytes\n", (void*)fd, bytesWritten));
#endif
        
        bytesWritten = checkHandshake(bytesWritten, mBusySocket->mThreadData->mReplacedSSLFileDesc, mBusySocket);
        if (bytesWritten < 0) {
          // give the error back to caller
          mBusySocket->mThreadData->mPRErrorCode = PR_GetError();
        }

        mBusySocket->mThreadData->mSSLResultRemainingBytes = bytesWritten;
        busy_socket_ssl_state = nsSSLSocketThreadData::ssl_writing_done;
      }
      else if (nsSSLSocketThreadData::ssl_pending_read == busy_socket_ssl_state)
      {
        PRInt32 bytesRead = mBusySocket->mThreadData->mReplacedSSLFileDesc->methods
           ->read(mBusySocket->mThreadData->mReplacedSSLFileDesc, 
                  mBusySocket->mThreadData->mSSLDataBuffer, 
                  mBusySocket->mThreadData->mSSLRequestedTransferAmount);

#ifdef DEBUG_SSL_VERBOSE
        PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("[%p] read %d bytes\n", (void*)fd, bytesRead));
        DEBUG_DUMP_BUFFER((unsigned char*)buf, bytesRead);
#endif
        bytesRead = checkHandshake(bytesRead, mBusySocket->mThreadData->mReplacedSSLFileDesc, mBusySocket);
        if (bytesRead < 0) {
          // give the error back to caller
          mBusySocket->mThreadData->mPRErrorCode = PR_GetError();
        }

        mBusySocket->mThreadData->mSSLResultRemainingBytes = bytesRead;
        mBusySocket->mThreadData->mSSLRemainingReadResultData = 
          mBusySocket->mThreadData->mSSLDataBuffer;
        busy_socket_ssl_state = nsSSLSocketThreadData::ssl_reading_done;
      }
    }

    // avoid setting event repeatedly
    PRBool needToSetPollableEvent = PR_FALSE;

    {
      nsAutoLock threadLock(ssl_thread_singleton->mMutex);
      
      mBusySocket->mThreadData->mSSLState = busy_socket_ssl_state;
      
      if (!nsSSLIOLayerHelpers::mPollableEventCurrentlySet)
      {
        needToSetPollableEvent = PR_TRUE;
        nsSSLIOLayerHelpers::mPollableEventCurrentlySet = PR_TRUE;
      }
    }

    if (needToSetPollableEvent)
    {
      // Wake up the file descriptor on the Necko thread,
      // so it can fetch the results from the SSL I/O call 
      // that we just completed.
      PR_SetPollableEvent(nsSSLIOLayerHelpers::mSharedPollableEvent);
    }
  }

  {
    nsAutoLock threadLock(ssl_thread_singleton->mMutex);
    if (mBusySocket)
    {
      restoreOriginalSocket_locked(mBusySocket);
      mBusySocket = nsnull;
    }
    if (!nsSSLIOLayerHelpers::mPollableEventCurrentlySet)
    {
      nsSSLIOLayerHelpers::mPollableEventCurrentlySet = PR_TRUE;
      PR_SetPollableEvent(nsSSLIOLayerHelpers::mSharedPollableEvent);
    }
  }
}

void nsSSLThread::rememberPendingHTTPRequest(nsIRequest *aRequest)
{
  if (!ssl_thread_singleton)
    return;

  nsAutoLock threadLock(ssl_thread_singleton->mMutex);

  NS_IF_ADDREF(aRequest);
  ssl_thread_singleton->mPendingHTTPRequest = aRequest;
}

void nsSSLThread::cancelPendingHTTPRequest()
{
  if (!ssl_thread_singleton)
    return;

  nsAutoLock threadLock(ssl_thread_singleton->mMutex);

  if (ssl_thread_singleton->mPendingHTTPRequest)
  {
    ssl_thread_singleton->mPendingHTTPRequest->Cancel(NS_ERROR_ABORT);

    NS_RELEASE(ssl_thread_singleton->mPendingHTTPRequest);

    ssl_thread_singleton->mPendingHTTPRequest = nsnull;
  }
}

nsSSLThread *nsSSLThread::ssl_thread_singleton = nsnull;

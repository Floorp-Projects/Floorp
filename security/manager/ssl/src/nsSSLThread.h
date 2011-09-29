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

#ifndef _NSSSLTHREAD_H_
#define _NSSSLTHREAD_H_

#include "nsCOMPtr.h"
#include "nsIRequest.h"
#include "nsPSMBackgroundThread.h"

class nsNSSSocketInfo;
class nsIHttpChannel;

class nsSSLThread : public nsPSMBackgroundThread
{
private:
  // We use mMutex contained in our base class
  // to protect access to these variables:
  //   mBusySocket, mSocketScheduledToBeDestroyed
  // and to nsSSLSocketThreadData::mSSLState
  // while a socket is the busy socket.

  // We use mCond contained in our base class
  // to notify the SSL thread that a new SSL I/O 
  // request has been queued for processing.
  // It can be found in the mBusySocket variable,
  // containing all details in its member.

  // A socket that is currently owned by the SSL thread 
  // and has pending SSL I/O activity or I/O results 
  // not yet fetched by the original caller.
  nsNSSSocketInfo *mBusySocket;
  
  // A socket that should be closed and destroyed
  // as soon as possible. The request was initiated by
  // Necko, but it happened at a time when the SSL
  // thread had ownership of the socket, so the request
  // was delayed. It's now the responsibility of the
  // SSL thread to close and destroy this socket.
  nsNSSSocketInfo *mSocketScheduledToBeDestroyed;

  // Did we receive a request from NSS to fetch HTTP 
  // data on behalf of NSS? (Most likely this is a OCSP request)
  // We track a handle to the HTTP request sent to Necko.
  // As this HTTP request depends on some original SSL socket,
  // we can use this handle to cancel the dependent HTTP request,
  // should we be asked to close the original SSL socket.
  nsCOMPtr<nsIRequest> mPendingHTTPRequest;

  virtual void Run(void);

  // Called from SSL thread only
  static PRInt32 checkHandshake(PRInt32 bytesTransfered, 
                                bool wasReading,
                                PRFileDesc* fd, 
                                nsNSSSocketInfo *socketInfo);

  // Function can be called from either Necko or SSL thread
  // Caller must lock mMutex before this call.
  static void restoreOriginalSocket_locked(nsNSSSocketInfo *si);

  // Helper for requestSomething functions, 
  // caled from the Necko thread only.
  static PRFileDesc *getRealSSLFD(nsNSSSocketInfo *si);

  // Support of blocking sockets is very rudimentary.
  // We only support it because Mozilla's LDAP code requires blocking I/O.
  // We do not support switching the blocking mode of a socket.
  // We require the blocking state has been set prior to the first 
  // read/write call, and will stay that way for the remainder of the socket's lifetime.
  // This function must be called while holding the lock.
  // If the socket is a blocking socket, out_fd will contain the real FD, 
  // on a non-blocking socket out_fd will be nsnull.
  // If there is a failure in obtaining the status of the socket,
  // the function will return PR_FAILURE.
  static PRStatus getRealFDIfBlockingSocket_locked(nsNSSSocketInfo *si, 
                                                   PRFileDesc *&out_fd);
public:
  nsSSLThread();
  ~nsSSLThread();

  static nsSSLThread *ssl_thread_singleton;

  // All requestSomething functions are called from 
  // the Necko thread only.

  static PRInt32 requestRead(nsNSSSocketInfo *si, 
                             void *buf, 
                             PRInt32 amount,
                             PRIntervalTime timeout);

  static PRInt32 requestWrite(nsNSSSocketInfo *si, 
                              const void *buf, 
                              PRInt32 amount,
                              PRIntervalTime timeout);

  static PRInt16 requestPoll(nsNSSSocketInfo *si, 
                             PRInt16 in_flags, 
                             PRInt16 *out_flags);

  static PRInt32 requestRecvMsgPeek(nsNSSSocketInfo *si, void *buf, PRInt32 amount,
                                    PRIntn flags, PRIntervalTime timeout);

  static PRStatus requestClose(nsNSSSocketInfo *si);

  static PRStatus requestGetsockname(nsNSSSocketInfo *si, PRNetAddr *addr);

  static PRStatus requestGetpeername(nsNSSSocketInfo *si, PRNetAddr *addr);

  static PRStatus requestGetsocketoption(nsNSSSocketInfo *si, 
                                         PRSocketOptionData *data);

  static PRStatus requestSetsocketoption(nsNSSSocketInfo *si, 
                                         const PRSocketOptionData *data);

  static PRStatus requestConnectcontinue(nsNSSSocketInfo *si, 
                                         PRInt16 out_flags);

  static nsresult requestActivateSSL(nsNSSSocketInfo *si);
  
  static bool stoppedOrStopping();
};

#endif //_NSSSLTHREAD_H_

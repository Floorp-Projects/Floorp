/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
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

#ifndef _NSNSSIOLAYER_H
#define _NSNSSIOLAYER_H

#include "prtypes.h"
#include "prio.h"
#include "certt.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransportSecurityInfo.h"
#include "nsISSLSocketControl.h"
#include "nsISSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsXPIDLString.h"
#include "nsNSSShutDown.h"

class nsIChannel;
class nsSSLThread;

/*
 * This class is used to store SSL socket I/O state information,
 * that is not being executed directly, but defered to 
 * the separate SSL thread.
 */
class nsSSLSocketThreadData
{
public:
  nsSSLSocketThreadData();
  ~nsSSLSocketThreadData();

  PRBool ensure_buffer_size(PRInt32 amount);
  
  enum ssl_state { 
    ssl_idle,          // not in use by SSL thread, no activity pending
    ssl_pending_write, // waiting for SSL thread to complete writing
    ssl_pending_read,  // waiting for SSL thread to complete reading
    ssl_writing_done,  // SSL write completed, results are ready
    ssl_reading_done   // SSL read completed, results are ready
  };
  
  ssl_state mSSLState;

  // Used to transport I/O error codes between SSL thread
  // and initial caller thread.
  PRErrorCode mPRErrorCode;

  // A buffer used to transfer I/O data between threads
  char *mSSLDataBuffer;
  PRInt32 mSSLDataBufferAllocatedSize;

  // The amount requested to read or write by the caller.
  PRInt32 mSSLRequestedTransferAmount;

  // A pointer into our buffer, to the first byte
  // that has not yet been delivered to the caller.
  // Necessary, as the caller of the read function
  // might request smaller chunks.
  const char *mSSLRemainingReadResultData;
  
  // The caller previously requested to read or write.
  // As the initial request to read or write is defered,
  // the caller might (in theory) request smaller chunks
  // in subsequent calls.
  // This variable stores the amount of bytes successfully
  // transfered, that have not yet been reported to the caller.
  PRInt32 mSSLResultRemainingBytes;

  // When defering SSL read/write activity to another thread,
  // we switch the SSL level file descriptor of the original
  // layered file descriptor to a pollable event,
  // so we can wake up the original caller of the I/O function
  // as soon as data is ready.
  // This variable is used to save the SSL level file descriptor,
  // to allow us to restore the original file descriptor layering.
  PRFileDesc *mReplacedSSLFileDesc;
};

class nsNSSSocketInfo : public nsITransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIInterfaceRequestor,
                        public nsISSLStatusProvider,
                        public nsNSSShutDownObject,
                        public nsOnPK11LogoutCancelObject
{
public:
  nsNSSSocketInfo();
  virtual ~nsNSSSocketInfo();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSISSLSOCKETCONTROL
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISSLSTATUSPROVIDER

  nsresult SetSecurityState(PRUint32 aState);
  nsresult SetShortSecurityDescription(const PRUnichar *aText);
  nsresult SetErrorMessage(const PRUnichar *aText);

  nsresult SetForSTARTTLS(PRBool aForSTARTTLS);
  nsresult GetForSTARTTLS(PRBool *aForSTARTTLS);

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  nsresult GetHandshakePending(PRBool *aHandshakePending);
  nsresult SetHandshakePending(PRBool aHandshakePending);

  nsresult GetHostName(char **aHostName);
  nsresult SetHostName(const char *aHostName);

  nsresult GetPort(PRInt32 *aPort);
  nsresult SetPort(PRInt32 aPort);

  void SetCanceled(PRBool aCanceled);
  PRBool GetCanceled();
  
  void SetHasCleartextPhase(PRBool aHasCleartextPhase);
  PRBool GetHasCleartextPhase();
  
  void SetHandshakeInProgress(PRBool aIsIn);
  PRBool GetHandshakeInProgress() { return mHandshakeInProgress; }
  PRBool HandshakeTimeout();

  void SetAllowTLSIntoleranceTimeout(PRBool aAllow);

  enum BadCertUIStatusType {
    bcuis_not_shown, bcuis_active, bcuis_was_shown
  };

  void SetBadCertUIStatus(BadCertUIStatusType aNewStatus);
  BadCertUIStatusType GetBadCertUIStatus() { return mBadCertUIStatus; }

  nsresult GetExternalErrorReporting(PRBool* state);
  nsresult SetExternalErrorReporting(PRBool aState);

  nsresult RememberCAChain(CERTCertList *aCertList);

  /* Set SSL Status values */
  nsresult SetSSLStatus(nsISSLStatus *aSSLStatus);  
  
  PRStatus CloseSocketAndDestroy();
  
protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  PRFileDesc* mFd;
  enum { 
    blocking_state_unknown, is_nonblocking_socket, is_blocking_socket 
  } mBlockingState;
  PRUint32 mSecurityState;
  nsString mShortDesc;
  nsString mErrorMessage;
  PRPackedBool mExternalErrorReporting;
  PRPackedBool mForSTARTTLS;
  PRPackedBool mHandshakePending;
  PRPackedBool mCanceled;
  PRPackedBool mHasCleartextPhase;
  PRPackedBool mHandshakeInProgress;
  PRPackedBool mAllowTLSIntoleranceTimeout;
  BadCertUIStatusType mBadCertUIStatus;
  PRIntervalTime mHandshakeStartTime;
  PRInt32 mPort;
  nsXPIDLCString mHostName;
  CERTCertList *mCAChain;

  /* SSL Status */
  nsCOMPtr<nsISSLStatus> mSSLStatus;

  nsresult ActivateSSL();

  nsSSLSocketThreadData *mThreadData;

private:
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();

friend class nsSSLThread;
};

class nsCStringHashSet;

class nsSSLIOLayerHelpers
{
public:
  static nsresult Init();
  static void Cleanup();

  static PRDescIdentity nsSSLIOLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;

  static PRLock *mutex;
  static nsCStringHashSet *mTLSIntolerantSites;
  
  static PRBool rememberPossibleTLSProblemSite(PRFileDesc* fd, nsNSSSocketInfo *socketInfo);

  static void addIntolerantSite(const nsCString &str);
  static PRBool isKnownAsIntolerantSite(const nsCString &str);
  
  static PRFileDesc *mSharedPollableEvent;
  static nsNSSSocketInfo *mSocketOwningPollableEvent;
  
  static PRBool mPollableEventCurrentlySet;
};

nsresult nsSSLIOLayerNewSocket(PRInt32 family,
                               const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               PRBool forSTARTTLS);

nsresult nsSSLIOLayerAddToSocket(PRInt32 family,
                                 const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 PRBool forSTARTTLS);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error);
 
#endif /* _NSNSSIOLAYER_H */

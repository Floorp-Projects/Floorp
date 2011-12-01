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
#include "mozilla/Mutex.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsITransportSecurityInfo.h"
#include "nsISSLSocketControl.h"
#include "nsSSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsXPIDLString.h"
#include "nsNSSShutDown.h"
#include "nsIClientAuthDialogs.h"
#include "nsAutoPtr.h"
#include "nsNSSCertificate.h"
#include "nsDataHashtable.h"

namespace mozilla {

class MutexAutoLock;

namespace psm {

enum SSLErrorMessageType {
  OverridableCertErrorMessage  = 1, // for *overridable* certificate errors
  PlainErrorMessage = 2             // all other errors (or "no error")
};

} // namespace psm

} // namespace mozilla

class nsNSSSocketInfo : public nsITransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIInterfaceRequestor,
                        public nsISSLStatusProvider,
                        public nsIAssociatedContentSecurity,
                        public nsISerializable,
                        public nsIClassInfo,
                        public nsIClientAuthUserDecision,
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
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO
  NS_DECL_NSICLIENTAUTHUSERDECISION

  nsresult SetSecurityState(PRUint32 aState);
  nsresult SetShortSecurityDescription(const PRUnichar *aText);

  nsresult SetForSTARTTLS(bool aForSTARTTLS);
  nsresult GetForSTARTTLS(bool *aForSTARTTLS);

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  nsresult GetHandshakePending(bool *aHandshakePending);
  nsresult SetHandshakePending(bool aHandshakePending);

  const char * GetHostName() const {
    return mHostName.get();
  }
  nsresult GetHostName(char **aHostName);
  nsresult SetHostName(const char *aHostName);

  nsresult GetPort(PRInt32 *aPort);
  nsresult SetPort(PRInt32 aPort);

  void GetPreviousCert(nsIX509Cert** _result);

  PRErrorCode GetErrorCode() const;
  void SetCanceled(PRErrorCode errorCode,
                   ::mozilla::psm::SSLErrorMessageType errorMessageType);
  
  void SetHasCleartextPhase(bool aHasCleartextPhase);
  bool GetHasCleartextPhase();
  
  void SetHandshakeInProgress(bool aIsIn);
  bool GetHandshakeInProgress() { return mHandshakeInProgress; }
  bool HandshakeTimeout();

  void SetAllowTLSIntoleranceTimeout(bool aAllow);

  nsresult RememberCAChain(CERTCertList *aCertList);

  /* Set SSL Status values */
  nsresult SetSSLStatus(nsSSLStatus *aSSLStatus);
  nsSSLStatus* SSLStatus() { return mSSLStatus; }
  void SetStatusErrorBits(nsIX509Cert & cert, PRUint32 collected_errors);

  PRStatus CloseSocketAndDestroy(
                const nsNSSShutDownPreventionLock & proofOfLock);
  
  bool IsCertIssuerBlacklisted() const {
    return mIsCertIssuerBlacklisted;
  }
  void SetCertIssuerBlacklisted() {
    mIsCertIssuerBlacklisted = true;
  }

  // XXX: These are only used on for diagnostic purposes
  enum CertVerificationState {
    before_cert_verification,
    waiting_for_cert_verification,
    after_cert_verification
  };
  void SetCertVerificationWaiting();
  // Use errorCode == 0 to indicate success; in that case, errorMessageType is
  // ignored.
  void SetCertVerificationResult(PRErrorCode errorCode,
              ::mozilla::psm::SSLErrorMessageType errorMessageType);
  
  // for logging only
  PRBool IsWaitingForCertVerification() const
  {
    return mCertVerificationState == waiting_for_cert_verification;
  }
  

protected:
  mutable ::mozilla::Mutex mMutex;

  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  PRFileDesc* mFd;
  CertVerificationState mCertVerificationState;
  PRUint32 mSecurityState;
  PRInt32 mSubRequestsHighSecurity;
  PRInt32 mSubRequestsLowSecurity;
  PRInt32 mSubRequestsBrokenSecurity;
  PRInt32 mSubRequestsNoSecurity;
  nsString mShortDesc;

  PRErrorCode mErrorCode;
  ::mozilla::psm::SSLErrorMessageType mErrorMessageType;
  nsString mErrorMessageCached;
  nsresult formatErrorMessage(::mozilla::MutexAutoLock const & proofOfLock);

  bool mDocShellDependentStuffKnown;
  bool mExternalErrorReporting; // DocShellDependent
  bool mForSTARTTLS;
  bool mHandshakePending;
  bool mHasCleartextPhase;
  bool mHandshakeInProgress;
  bool mAllowTLSIntoleranceTimeout;
  bool mRememberClientAuthCertificate;
  PRIntervalTime mHandshakeStartTime;
  PRInt32 mPort;
  nsXPIDLCString mHostName;
  PRErrorCode mIsCertIssuerBlacklisted;

  /* SSL Status */
  nsRefPtr<nsSSLStatus> mSSLStatus;

  nsresult ActivateSSL();

private:
  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class nsCStringHashSet;

class nsSSLStatus;
class nsNSSSocketInfo;

class nsPSMRememberCertErrorsTable
{
private:
  struct CertStateBits
  {
    bool mIsDomainMismatch;
    bool mIsNotValidAtThisTime;
    bool mIsUntrusted;
  };
  nsDataHashtableMT<nsCStringHashKey, CertStateBits> mErrorHosts;
  nsresult GetHostPortKey(nsNSSSocketInfo* infoObject, nsCAutoString& result);

public:
  friend class nsSSLIOLayerHelpers;
  nsPSMRememberCertErrorsTable();
  void RememberCertHasError(nsNSSSocketInfo* infoObject,
                           nsSSLStatus* status,
                           SECStatus certVerificationResult);
  void LookupCertErrorBits(nsNSSSocketInfo* infoObject,
                           nsSSLStatus* status);
};

class nsSSLIOLayerHelpers
{
public:
  static nsresult Init();
  static void Cleanup();

  static bool nsSSLIOLayerInitialized;
  static PRDescIdentity nsSSLIOLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;

  static mozilla::Mutex *mutex;
  static nsCStringHashSet *mTLSIntolerantSites;
  static nsCStringHashSet *mTLSTolerantSites;
  static nsPSMRememberCertErrorsTable* mHostsWithCertErrors;

  static nsCStringHashSet *mRenegoUnrestrictedSites;
  static bool mTreatUnsafeNegotiationAsBroken;
  static PRInt32 mWarnLevelMissingRFC5746;

  static void setTreatUnsafeNegotiationAsBroken(bool broken);
  static bool treatUnsafeNegotiationAsBroken();

  static void setWarnLevelMissingRFC5746(PRInt32 level);
  static PRInt32 getWarnLevelMissingRFC5746();

  static void getSiteKey(nsNSSSocketInfo *socketInfo, nsCSubstring &key);
  static bool rememberPossibleTLSProblemSite(PRFileDesc* fd, nsNSSSocketInfo *socketInfo);
  static void rememberTolerantSite(PRFileDesc* ssl_layer_fd, nsNSSSocketInfo *socketInfo);

  static void addIntolerantSite(const nsCString &str);
  static void removeIntolerantSite(const nsCString &str);
  static bool isKnownAsIntolerantSite(const nsCString &str);

  static void setRenegoUnrestrictedSites(const nsCString &str);
  static bool isRenegoUnrestrictedSite(const nsCString &str);
};

nsresult nsSSLIOLayerNewSocket(PRInt32 family,
                               const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               bool forSTARTTLS,
                               bool anonymousLoad);

nsresult nsSSLIOLayerAddToSocket(PRInt32 family,
                                 const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 bool forSTARTTLS,
                                 bool anonymousLoad);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error);

// 16786594-0296-4471-8096-8f84497ca428
#define NS_NSSSOCKETINFO_CID \
{ 0x16786594, 0x0296, 0x4471, \
    { 0x80, 0x96, 0x8f, 0x84, 0x49, 0x7c, 0xa4, 0x28 } }


#endif /* _NSNSSIOLAYER_H */

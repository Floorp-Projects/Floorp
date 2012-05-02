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

#ifndef _MOZILLA_PSM_TRANSPORTSECURITYINFO_H
#define _MOZILLA_PSM_TRANSPORTSECURITYINFO_H

#include "certt.h"
#include "mozilla/Mutex.h"
#include "nsIInterfaceRequestor.h"
#include "nsITransportSecurityInfo.h"
#include "nsSSLStatus.h"
#include "nsISSLStatusProvider.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsNSSShutDown.h"
#include "nsDataHashtable.h"

namespace mozilla { namespace psm {

enum SSLErrorMessageType {
  OverridableCertErrorMessage  = 1, // for *overridable* certificate errors
  PlainErrorMessage = 2             // all other errors (or "no error")
};

class TransportSecurityInfo : public nsITransportSecurityInfo,
                              public nsIInterfaceRequestor,
                              public nsISSLStatusProvider,
                              public nsIAssociatedContentSecurity,
                              public nsISerializable,
                              public nsIClassInfo,
                              public nsNSSShutDownObject,
                              public nsOnPK11LogoutCancelObject
{
public:
  TransportSecurityInfo();
  virtual ~TransportSecurityInfo();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISSLSTATUSPROVIDER
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsresult SetSecurityState(PRUint32 aState);
  nsresult SetShortSecurityDescription(const PRUnichar *aText);

  const char * GetHostName() const {
    return mHostName.get();
  }
  nsresult GetHostName(char **aHostName);
  nsresult SetHostName(const char *aHostName);

  PRInt32 GetPort() const { return mPort; }
  nsresult GetPort(PRInt32 *aPort);
  nsresult SetPort(PRInt32 aPort);

  PRErrorCode GetErrorCode() const;
  void SetCanceled(PRErrorCode errorCode,
                   ::mozilla::psm::SSLErrorMessageType errorMessageType);
  
  /* Set SSL Status values */
  nsresult SetSSLStatus(nsSSLStatus *aSSLStatus);
  nsSSLStatus* SSLStatus() { return mSSLStatus; }
  void SetStatusErrorBits(nsIX509Cert & cert, PRUint32 collected_errors);

  bool IsCertIssuerBlacklisted() const {
    return mIsCertIssuerBlacklisted;
  }
  void SetCertIssuerBlacklisted() {
    mIsCertIssuerBlacklisted = true;
  }

private:
  mutable ::mozilla::Mutex mMutex;

protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

private:
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

  PRInt32 mPort;
  nsXPIDLCString mHostName;
  PRErrorCode mIsCertIssuerBlacklisted;

  /* SSL Status */
  nsRefPtr<nsSSLStatus> mSSLStatus;

  virtual void virtualDestroyNSSReference();
  void destructorSafeDestroyNSSReference();
};

class RememberCertErrorsTable
{
private:
  RememberCertErrorsTable();

  struct CertStateBits
  {
    bool mIsDomainMismatch;
    bool mIsNotValidAtThisTime;
    bool mIsUntrusted;
  };
  nsDataHashtableMT<nsCStringHashKey, CertStateBits> mErrorHosts;

public:
  void RememberCertHasError(TransportSecurityInfo * infoobject,
                            nsSSLStatus * status,
                            SECStatus certVerificationResult);
  void LookupCertErrorBits(TransportSecurityInfo * infoObject,
                           nsSSLStatus* status);

  static nsresult Init()
  {
    sInstance = new RememberCertErrorsTable();
    if (!sInstance->mErrorHosts.IsInitialized())
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
  }

  static RememberCertErrorsTable & GetInstance()
  {
    MOZ_ASSERT(sInstance);
    return *sInstance;
  }

  static void Cleanup()
  {
    delete sInstance;
    sInstance = nsnull;
  }
private:
  Mutex mMutex;

  static RememberCertErrorsTable * sInstance;
};

} } // namespace mozilla::psm

// 16786594-0296-4471-8096-8f84497ca428
#define TRANSPORTSECURITYINFO_CID \
{ 0x16786594, 0x0296, 0x4471, \
    { 0x80, 0x96, 0x8f, 0x84, 0x49, 0x7c, 0xa4, 0x28 } }

#endif /* _MOZILLA_PSM_TRANSPORTSECURITYINFO_H */

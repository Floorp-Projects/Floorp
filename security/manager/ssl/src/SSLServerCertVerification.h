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
#ifndef _SSLSERVERCERTVERIFICATION_H
#define _SSLSERVERCERTVERIFICATION_H

#include "seccomon.h"
#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "prerror.h"
#include "nsNSSIOLayer.h"

typedef struct PRFileDesc PRFileDesc;
typedef struct CERTCertificateStr CERTCertificate;
class nsNSSSocketInfo;
class nsNSSShutDownPreventionLock;

namespace mozilla { namespace psm {

SECStatus AuthCertificateHook(void *arg, PRFileDesc *fd, 
                              PRBool checkSig, PRBool isServer);

SECStatus HandleBadCertificate(PRErrorCode defaultErrorCodeToReport,
                               nsNSSSocketInfo * socketInfo,
                               CERTCertificate & cert,
                               const void * fdForLogging,
                               const nsNSSShutDownPreventionLock &);

// Dispatched from a cert verification thread to the STS thread to notify the
// socketInfo of the verification result.
//
// This will cause the PR_Poll in the STS thread to return, so things work
// correctly even if the STS thread is blocked polling (only) on the file
// descriptor that is waiting for this result.
class SSLServerCertVerificationResult : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  SSLServerCertVerificationResult(nsNSSSocketInfo & socketInfo,
                                  PRErrorCode errorCode,
                                  SSLErrorMessageType errorMessageType = 
                                      PlainErrorMessage);

  void Dispatch();
private:
  const nsRefPtr<nsNSSSocketInfo> mSocketInfo;
  const PRErrorCode mErrorCode;
  const SSLErrorMessageType mErrorMessageType;
};

} } // namespace mozilla::psm

#endif

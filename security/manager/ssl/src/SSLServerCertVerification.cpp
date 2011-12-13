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
 *   Terry Hayes <thayes@netscape.com>
 *   Kai Engert <kengert@redhat.com>
 *   Petr Kostka <petr.kostka@st.com>
 *   Honza Bambas <honzab@firemni.cz>
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

/* 
 * All I/O is done on the socket transport thread, including all calls into
 * libssl. That is, all SSL_* functions must be called on the socket transport
 * thread. This also means that all SSL callback functions will be called on
 * the socket transport thread, including in particular the auth certificate
 * hook.
 *
 * During certificate authentication, we call CERT_PKIXVerifyCert or
 * CERT_VerifyCert. These functions may make zero or more HTTP requests
 * for OCSP responses, CRLs, intermediate certificates, etc.
 *
 * If our cert auth hook were to call the CERT_*Verify* functions directly,
 * there would be a deadlock: The CERT_*Verify* function would cause an event
 * to be asynchronously posted to the socket transport thread, and then it
 * would block the socket transport thread waiting to be notified of the HTTP
 * response. However, the HTTP request would never actually be processed
 * because the socket transport thread would be blocked and so it wouldn't be
 * able process HTTP requests. (i.e. Deadlock.)
 *
 * Consequently, we must always call the CERT_*Verify* cert functions off the
 * socket transport thread. To accomplish this, our auth cert hook dispatches a
 * SSLServerCertVerificationJob to a pool of background threads, and then
 * immediatley return SECWouldBlock to libssl. These jobs are where the
 * CERT_*Verify* functions are actually called. 
 *
 * When our auth cert hook returns SECWouldBlock, libssl will carry on the
 * handshake while we validate the certificate. This will free up the socket
 * transport thread so that HTTP requests--in particular, the OCSP/CRL/cert
 * requests needed for cert verification as mentioned above--can be processed.
 *
 * Once the CERT_*Verify* function returns, the cert verification job
 * dispatches a SSLServerCertVerificationResult to the socket transport thread;
 * the SSLServerCertVerificationResult will notify libssl that the certificate
 * authentication is complete. Once libssl is notified that the authentication
 * is complete, it will continue the SSL handshake (if it hasn't already
 * finished) and it will begin allowing us to send/receive data on the
 * connection.
 *
 * Timeline of events:
 *
 *    * libssl calls SSLServerCertVerificationJob::Dispatch on the socket
 *      transport thread.
 *    * SSLServerCertVerificationJob::Dispatch queues a job
 *      (instance of SSLServerCertVerificationJob) to its background thread
 *      pool and returns.
 *    * One of the background threads calls CERT_*Verify*, which may enqueue
 *      some HTTP request(s) onto the socket transport thread, and then
 *      blocks that background thread waiting for the responses and/or timeouts
 *      or errors for those requests.
 *    * Once those HTTP responses have all come back or failed, the
 *      CERT_*Verify* function returns a result indicating that the validation
 *      succeeded or failed.
 *    * If the validation succeeded, then a SSLServerCertVerificationResult
 *      event is posted to the socket transport thread, and the cert
 *      verification thread becomes free to verify other certificates.
 *    * Otherwise, a CertErrorRunnable is posted to the socket transport thread
 *      and then to the main thread (blocking both, see CertErrorRunnable) to
 *      do cert override processing and bad cert listener notification. Then
 *      the cert verification thread becomes free to verify other certificates.
 *    * After processing cert overrides, the CertErrorRunnable will dispatch a
 *      SSLServerCertVerificationResult event to the socket transport thread to
 *      notify it of the result of the override processing; then it returns,
 *      freeing up the main thread.
 *    * The SSLServerCertVerificationResult event will either wake up the 
 *      socket (using SSL_RestartHandshakeAfterServerCert) if validation
 *      succeeded or there was an error override, or it will set an error flag
 *      so that the next I/O operation on the socket will fail, causing the
 *      socket transport thread to close the connection.
 *
 * Cert override processing must happen on the main thread because it accesses
 * the nsICertOverrideService, and that service must be accessed on the main 
 * thread because some extensions (Selenium, in particular) replace it with a
 * Javascript implementation, and chrome JS must always be run on the main
 * thread.
 *
 * SSLServerCertVerificationResult must be dispatched to the socket transport
 * thread because we must only call SSL_* functions on the socket transport
 * thread since they may do I/O, because many parts of nsNSSSocketInfo and
 * the PSM NSS I/O layer are not thread-safe, and because we need the event to
 * interrupt the PR_Poll that may waiting for I/O on the socket for which we
 * are validating the cert.
 */

#include "SSLServerCertVerification.h"
#include "nsNSSComponent.h"
#include "nsNSSCertificate.h"
#include "nsNSSIOLayer.h"

#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "ssl.h"
#include "secerr.h"
#include "sslerr.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

namespace mozilla { namespace psm {

namespace {
// do not use a nsCOMPtr to avoid static initializer/destructor
nsIThreadPool * gCertVerificationThreadPool = nsnull;
} // unnamed namespace

// Called when the socket transport thread starts, to initialize the SSL cert
// verification thread pool. By tying the thread pool startup/shutdown directly
// to the STS thread's lifetime, we ensure that they are *always* available for
// SSL connections and that there are no races during startup and especially
// shutdown. (Previously, we have had multiple problems with races in PSM
// background threads, and the race-prevention/shutdown logic used there is
// brittle. Since this service is critical to things like downloading updates,
// we take no chances.) Also, by doing things this way, we avoid the need for
// locks, since gCertVerificationThreadPool is only ever accessed on the socket
// transport thread.
void
InitializeSSLServerCertVerificationThreads()
{
  // TODO: tuning, make parameters preferences
  // XXX: instantiate nsThreadPool directly, to make this more bulletproof.
  // Currently, the nsThreadPool.h header isn't exported for us to do so.
  nsresult rv = CallCreateInstance(NS_THREADPOOL_CONTRACTID,
                                   &gCertVerificationThreadPool);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create SSL cert verification threads.");
    return;
  }

  (void) gCertVerificationThreadPool->SetIdleThreadLimit(5);
  (void) gCertVerificationThreadPool->SetIdleThreadTimeout(30 * 1000);
  (void) gCertVerificationThreadPool->SetThreadLimit(5);
}

// Called when the socket transport thread finishes, to destroy the thread
// pool. Since the socket transport service has stopped processing events, it
// will not attempt any more SSL I/O operations, so it is clearly safe to shut
// down the SSL cert verification infrastructure. Also, the STS will not
// dispatch many SSL verification result events at this point, so any pending
// cert verifications will (correctly) fail at the point they are dispatched.
//
// The other shutdown race condition that is possible is a race condition with
// shutdown of the nsNSSComponent service. We use the
// nsNSSShutdownPreventionLock where needed (not here) to prevent that.
void StopSSLServerCertVerificationThreads()
{
  if (gCertVerificationThreadPool) {
    gCertVerificationThreadPool->Shutdown();
    NS_RELEASE(gCertVerificationThreadPool);
  }
}

namespace {

class SSLServerCertVerificationJob : public nsRunnable
{
public:
  // Must be called only on the socket transport thread
  static SECStatus Dispatch(const void * fdForLogging,
                            nsNSSSocketInfo * infoObject,
                            CERTCertificate * serverCert);
private:
  NS_DECL_NSIRUNNABLE

  // Must be called only on the socket transport thread
  SSLServerCertVerificationJob(const void * fdForLogging,
                               nsNSSSocketInfo & socketInfo, 
                               CERTCertificate & cert);
  ~SSLServerCertVerificationJob();

  // Runs on one of the background threads
  SECStatus AuthCertificate(const nsNSSShutDownPreventionLock & proofOfLock);

  const void * const mFdForLogging;
  const nsRefPtr<nsNSSSocketInfo> mSocketInfo;
  CERTCertificate * const mCert;
};

SSLServerCertVerificationJob::SSLServerCertVerificationJob(
    const void * fdForLogging, nsNSSSocketInfo & socketInfo,
    CERTCertificate & cert)
  : mFdForLogging(fdForLogging)
  , mSocketInfo(&socketInfo)
  , mCert(CERT_DupCertificate(&cert))
{
}

SSLServerCertVerificationJob::~SSLServerCertVerificationJob()
{
  CERT_DestroyCertificate(mCert);
}

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

SECStatus
PSM_SSL_PKIX_AuthCertificate(CERTCertificate *peerCert, void * pinarg,
                             const char * hostname,
                             const nsNSSShutDownPreventionLock & /*proofOfLock*/)
{
    SECStatus          rv;
    
    if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
        rv = CERT_VerifyCertNow(CERT_GetDefaultCertDB(), peerCert, true,
                                certUsageSSLServer, pinarg);
    }
    else {
        nsresult nsrv;
        nsCOMPtr<nsINSSComponent> inss = do_GetService(kNSSComponentCID, &nsrv);
        if (!inss)
          return SECFailure;
        nsRefPtr<nsCERTValInParamWrapper> survivingParams;
        if (NS_FAILED(inss->GetDefaultCERTValInParam(survivingParams)))
          return SECFailure;

        CERTValOutParam cvout[1];
        cvout[0].type = cert_po_end;

        rv = CERT_PKIXVerifyCert(peerCert, certificateUsageSSLServer,
                                survivingParams->GetRawPointerForNSS(),
                                cvout, pinarg);
    }

    if (rv == SECSuccess) {
        /* cert is OK.  This is the client side of an SSL connection.
        * Now check the name field in the cert against the desired hostname.
        * NB: This is our only defense against Man-In-The-Middle (MITM) attacks!
        */
        if (hostname && hostname[0])
            rv = CERT_VerifyCertName(peerCert, hostname);
        else
            rv = SECFailure;
        if (rv != SECSuccess)
            PORT_SetError(SSL_ERROR_BAD_CERT_DOMAIN);
    }
        
    return rv;
}

struct nsSerialBinaryBlacklistEntry
{
  unsigned int len;
  const char *binary_serial;
};

// bug 642395
static struct nsSerialBinaryBlacklistEntry myUTNBlacklistEntries[] = {
  { 17, "\x00\x92\x39\xd5\x34\x8f\x40\xd1\x69\x5a\x74\x54\x70\xe1\xf2\x3f\x43" },
  { 17, "\x00\xd8\xf3\x5f\x4e\xb7\x87\x2b\x2d\xab\x06\x92\xe3\x15\x38\x2f\xb0" },
  { 16, "\x72\x03\x21\x05\xc5\x0c\x08\x57\x3d\x8e\xa5\x30\x4e\xfe\xe8\xb0" },
  { 17, "\x00\xb0\xb7\x13\x3e\xd0\x96\xf9\xb5\x6f\xae\x91\xc8\x74\xbd\x3a\xc0" },
  { 16, "\x39\x2a\x43\x4f\x0e\x07\xdf\x1f\x8a\xa3\x05\xde\x34\xe0\xc2\x29" },
  { 16, "\x3e\x75\xce\xd4\x6b\x69\x30\x21\x21\x88\x30\xae\x86\xa8\x2a\x71" },
  { 17, "\x00\xe9\x02\x8b\x95\x78\xe4\x15\xdc\x1a\x71\x0a\x2b\x88\x15\x44\x47" },
  { 17, "\x00\xd7\x55\x8f\xda\xf5\xf1\x10\x5b\xb2\x13\x28\x2b\x70\x77\x29\xa3" },
  { 16, "\x04\x7e\xcb\xe9\xfc\xa5\x5f\x7b\xd0\x9e\xae\x36\xe1\x0c\xae\x1e" },
  { 17, "\x00\xf5\xc8\x6a\xf3\x61\x62\xf1\x3a\x64\xf5\x4f\x6d\xc9\x58\x7c\x06" },
  { 0, 0 } // end marker
};

// Call this if we have already decided that a cert should be treated as INVALID,
// in order to check if we to worsen the error to REVOKED.
PRErrorCode
PSM_SSL_DigiNotarTreatAsRevoked(CERTCertificate * serverCert,
                                CERTCertList * serverCertChain)
{
  // If any involved cert was issued by DigiNotar, 
  // and serverCert was issued after 01-JUL-2011,
  // then worsen the error to revoked.
  
  PRTime cutoff = 0;
  PRStatus status = PR_ParseTimeString("01-JUL-2011 00:00", true, &cutoff);
  if (status != PR_SUCCESS) {
    NS_ASSERTION(status == PR_SUCCESS, "PR_ParseTimeString failed");
    // be safe, assume it's afterwards, keep going
  } else {
    PRTime notBefore = 0, notAfter = 0;
    if (CERT_GetCertTimes(serverCert, &notBefore, &notAfter) == SECSuccess &&
           notBefore < cutoff) {
      // no worsening for certs issued before the cutoff date
      return 0;
    }
  }
  
  for (CERTCertListNode *node = CERT_LIST_HEAD(serverCertChain);
       !CERT_LIST_END(node, serverCertChain);
       node = CERT_LIST_NEXT(node)) {
    if (node->cert->issuerName &&
        strstr(node->cert->issuerName, "CN=DigiNotar")) {
      return SEC_ERROR_REVOKED_CERTIFICATE;
    }
  }
  
  return 0;
}

// Call this only if a cert has been reported by NSS as VALID
PRErrorCode
PSM_SSL_BlacklistDigiNotar(CERTCertificate * serverCert,
                           CERTCertList * serverCertChain)
{
  bool isDigiNotarIssuedCert = false;

  for (CERTCertListNode *node = CERT_LIST_HEAD(serverCertChain);
       !CERT_LIST_END(node, serverCertChain);
       node = CERT_LIST_NEXT(node)) {
    if (!node->cert->issuerName)
      continue;

    if (strstr(node->cert->issuerName, "CN=DigiNotar")) {
      isDigiNotarIssuedCert = true;
    }
  }

  if (isDigiNotarIssuedCert) {
    // let's see if we want to worsen the error code to revoked.
    PRErrorCode revoked_code = PSM_SSL_DigiNotarTreatAsRevoked(serverCert, serverCertChain);
    return (revoked_code != 0) ? revoked_code : SEC_ERROR_UNTRUSTED_ISSUER;
  }

  return 0;
}

// This function assumes that we will only use the SPDY connection coalescing
// feature on connections where we have negotiated SPDY using NPN. If we ever
// talk SPDY without having negotiated it with SPDY, this code will give wrong
// and perhaps unsafe results.
//
// Returns SECSuccess on the initial handshake of all connections, on
// renegotiations for any connections where we did not negotiate SPDY, or on any
// SPDY connection where the server's certificate did not change.
//
// Prohibit changing the server cert only if we negotiated SPDY,
// in order to support SPDY's cross-origin connection pooling.

static SECStatus
BlockServerCertChangeForSpdy(nsNSSSocketInfo *infoObject,
                             CERTCertificate *serverCert)
{
  // Get the existing cert. If there isn't one, then there is
  // no cert change to worry about.
  nsCOMPtr<nsIX509Cert> cert;
  nsCOMPtr<nsIX509Cert2> cert2;

  nsRefPtr<nsSSLStatus> status = infoObject->SSLStatus();
  if (!status) {
    // If we didn't have a status, then this is the
    // first handshake on this connection, not a
    // renegotiation.
    return SECSuccess;
  }
  
  status->GetServerCert(getter_AddRefs(cert));
  cert2 = do_QueryInterface(cert);
  if (!cert2) {
    NS_NOTREACHED("every nsSSLStatus must have a cert"
                  "that implements nsIX509Cert2");
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  // Filter out sockets that did not neogtiate SPDY via NPN
  nsCAutoString negotiatedNPN;
  nsresult rv = infoObject->GetNegotiatedNPN(negotiatedNPN);
  NS_ASSERTION(NS_SUCCEEDED(rv),
               "GetNegotiatedNPN() failed during renegotiation");

  if (NS_SUCCEEDED(rv) && !negotiatedNPN.Equals(NS_LITERAL_CSTRING("spdy/2")))
    return SECSuccess;

  // If GetNegotiatedNPN() failed we will assume spdy for safety's safe
  if (NS_FAILED(rv))
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
           ("BlockServerCertChangeForSpdy failed GetNegotiatedNPN() call."
            " Assuming spdy.\n"));

  // Check to see if the cert has actually changed
  CERTCertificate * c = cert2->GetCert();
  NS_ASSERTION(c, "very bad and hopefully impossible state");
  bool sameCert = CERT_CompareCerts(c, serverCert);
  CERT_DestroyCertificate(c);
  if (sameCert)
    return SECSuccess;

  // Report an error - changed cert is confirmed
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("SPDY Refused to allow new cert during renegotiation\n"));
  PR_SetError(SSL_ERROR_RENEGOTIATION_NOT_ALLOWED, 0);
  return SECFailure;
}

SECStatus
SSLServerCertVerificationJob::AuthCertificate(
  nsNSSShutDownPreventionLock const & nssShutdownPreventionLock)
{
  if (BlockServerCertChangeForSpdy(mSocketInfo, mCert) != SECSuccess)
    return SECFailure;

  if (mCert->serialNumber.data &&
      mCert->issuerName &&
      !strcmp(mCert->issuerName, 
        "CN=UTN-USERFirst-Hardware,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US")) {

    unsigned char *server_cert_comparison_start = mCert->serialNumber.data;
    unsigned int server_cert_comparison_len = mCert->serialNumber.len;

    while (server_cert_comparison_len) {
      if (*server_cert_comparison_start != 0)
        break;

      ++server_cert_comparison_start;
      --server_cert_comparison_len;
    }

    nsSerialBinaryBlacklistEntry *walk = myUTNBlacklistEntries;
    for ( ; walk && walk->len; ++walk) {

      unsigned char *locked_cert_comparison_start = (unsigned char*)walk->binary_serial;
      unsigned int locked_cert_comparison_len = walk->len;
      
      while (locked_cert_comparison_len) {
        if (*locked_cert_comparison_start != 0)
          break;
        
        ++locked_cert_comparison_start;
        --locked_cert_comparison_len;
      }

      if (server_cert_comparison_len == locked_cert_comparison_len &&
          !memcmp(server_cert_comparison_start, locked_cert_comparison_start, locked_cert_comparison_len)) {
        PR_SetError(SEC_ERROR_REVOKED_CERTIFICATE, 0);
        return SECFailure;
      }
    }
  }

  SECStatus rv = PSM_SSL_PKIX_AuthCertificate(mCert, mSocketInfo,
                                              mSocketInfo->GetHostName(),
                                              nssShutdownPreventionLock);

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.

  nsRefPtr<nsSSLStatus> status = mSocketInfo->SSLStatus();
  nsRefPtr<nsNSSCertificate> nsc;

  if (!status || !status->mServerCert) {
    nsc = nsNSSCertificate::Create(mCert);
  }

  CERTCertList *certList = nsnull;
  certList = CERT_GetCertChainFromCert(mCert, PR_Now(), certUsageSSLCA);
  if (!certList) {
    rv = SECFailure;
  } else {
    PRErrorCode blacklistErrorCode;
    if (rv == SECSuccess) { // PSM_SSL_PKIX_AuthCertificate said "valid cert"
      blacklistErrorCode = PSM_SSL_BlacklistDigiNotar(mCert, certList);
    } else { // PSM_SSL_PKIX_AuthCertificate said "invalid cert"
      PRErrorCode savedErrorCode = PORT_GetError();
      // Check if we want to worsen the error code to "revoked".
      blacklistErrorCode = PSM_SSL_DigiNotarTreatAsRevoked(mCert, certList);
      if (blacklistErrorCode == 0) {
        // we don't worsen the code, let's keep the original error code from NSS
        PORT_SetError(savedErrorCode);
      }
    }
      
    if (blacklistErrorCode != 0) {
      mSocketInfo->SetCertIssuerBlacklisted();
      PORT_SetError(blacklistErrorCode);
      rv = SECFailure;
    }
  }

  if (rv == SECSuccess) {
    if (nsc) {
      bool dummyIsEV;
      nsc->GetIsExtendedValidation(&dummyIsEV); // the nsc object will cache the status
    }
    
    nsCOMPtr<nsINSSComponent> nssComponent;
      
    for (CERTCertListNode *node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList);
         node = CERT_LIST_NEXT(node)) {

      if (node->cert->slot) {
        // This cert was found on a token, no need to remember it in the temp db.
        continue;
      }

      if (node->cert->isperm) {
        // We don't need to remember certs already stored in perm db.
        continue;
      }
        
      if (node->cert == mCert) {
        // We don't want to remember the server cert, 
        // the code that cares for displaying page info does this already.
        continue;
      }

      // We have found a signer cert that we want to remember.
      char* nickname = nsNSSCertificate::defaultServerNickname(node->cert);
      if (nickname && *nickname) {
        PK11SlotInfo *slot = PK11_GetInternalKeySlot();
        if (slot) {
          PK11_ImportCert(slot, node->cert, CK_INVALID_HANDLE, 
                          nickname, false);
          PK11_FreeSlot(slot);
        }
      }
      PR_FREEIF(nickname);
    }

    if (certList) {
      CERT_DestroyCertList(certList);
    }

    // The connection may get terminated, for example, if the server requires
    // a client cert. Let's provide a minimal SSLStatus
    // to the caller that contains at least the cert and its status.
    if (!status) {
      status = new nsSSLStatus();
      mSocketInfo->SetSSLStatus(status);
    }

    if (rv == SECSuccess) {
      // Certificate verification succeeded delete any potential record
      // of certificate error bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->RememberCertHasError(
        mSocketInfo, nsnull, rv);
    }
    else {
      // Certificate verification failed, update the status' bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->LookupCertErrorBits(
        mSocketInfo, status);
    }

    if (status && !status->mServerCert) {
      status->mServerCert = nsc;
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("AuthCertificate setting NEW cert %p\n", status->mServerCert.get()));
    }
  }

  return rv;
}

/*static*/ SECStatus
SSLServerCertVerificationJob::Dispatch(const void * fdForLogging,
                                       nsNSSSocketInfo * socketInfo,
                                       CERTCertificate * serverCert)
{
  // Runs on the socket transport thread

  if (!socketInfo || !serverCert) {
    NS_ERROR("Invalid parameters for SSL server cert validation");
    socketInfo->SetCertVerificationResult(PR_INVALID_STATE_ERROR,
                                          PlainErrorMessage);
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }
  
  nsRefPtr<SSLServerCertVerificationJob> job
    = new SSLServerCertVerificationJob(fdForLogging, *socketInfo, *serverCert);

  socketInfo->SetCertVerificationWaiting();
  nsresult nrv;
  if (!gCertVerificationThreadPool) {
    nrv = NS_ERROR_NOT_INITIALIZED;
  } else {
    nrv = gCertVerificationThreadPool->Dispatch(job, NS_DISPATCH_NORMAL);
  }
  if (NS_FAILED(nrv)) {
    PRErrorCode error = nrv == NS_ERROR_OUT_OF_MEMORY
                      ? SEC_ERROR_NO_MEMORY
                      : PR_INVALID_STATE_ERROR;
    socketInfo->SetCertVerificationResult(error, PlainErrorMessage);
    PORT_SetError(error);
    return SECFailure;
  }

  PORT_SetError(PR_WOULD_BLOCK_ERROR);
  return SECWouldBlock;    
}

NS_IMETHODIMP
SSLServerCertVerificationJob::Run()
{
  // Runs on a cert verification thread

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
          ("[%p] SSLServerCertVerificationJob::Run\n", mSocketInfo.get()));

  PRErrorCode error;

  nsNSSShutDownPreventionLock nssShutdownPrevention;
  if (mSocketInfo->isAlreadyShutDown()) {
    error = SEC_ERROR_USER_CANCELLED;
  } else {
    // Reset the error code here so we can detect if AuthCertificate fails to
    // set the error code if/when it fails.
    PR_SetError(0, 0); 
    SECStatus rv = AuthCertificate(nssShutdownPrevention);
    if (rv == SECSuccess) {
      nsRefPtr<SSLServerCertVerificationResult> restart 
        = new SSLServerCertVerificationResult(*mSocketInfo, 0);
      restart->Dispatch();
      return NS_OK;
    }

    error = PR_GetError();
    if (error != 0) {
      rv = HandleBadCertificate(error, mSocketInfo, *mCert, mFdForLogging,
                                nssShutdownPrevention);
      if (rv == SECSuccess) {
        // The CertErrorRunnable will run on the main thread and it will dispatch
        // the cert verification result to the socket transport thread, so we
        // don't have to. This way, this verification thread doesn't need to
        // wait for the CertErrorRunnable to complete.
        return NS_OK; 
      }
      // DispatchCertErrorRunnable set a new error code.
      error = PR_GetError(); 
    }
  }

  if (error == 0) {
    NS_NOTREACHED("no error set during certificate validation failure");
    error = PR_INVALID_STATE_ERROR;
  }

  nsRefPtr<SSLServerCertVerificationResult> failure
    = new SSLServerCertVerificationResult(*mSocketInfo, error);
  failure->Dispatch();
  return NS_OK;
}

} // unnamed namespace

// Extracts whatever information we need out of fd (using SSL_*) and passes it
// to SSLServerCertVerificationJob::Dispatch. SSLServerCertVerificationJob should
// never do anything with fd except logging.
SECStatus
AuthCertificateHook(void *arg, PRFileDesc *fd, PRBool checkSig, PRBool isServer)
{
  // Runs on the socket transport thread

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
         ("[%p] starting AuthCertificateHook\n", fd));

  // Modern libssl always passes PR_TRUE for checkSig, and we have no means of
  // doing verification without checking signatures.
  NS_ASSERTION(checkSig, "AuthCertificateHook: checkSig unexpectedly false");

  // PSM never causes libssl to call this function with PR_TRUE for isServer,
  // and many things in PSM assume that we are a client.
  NS_ASSERTION(!isServer, "AuthCertificateHook: isServer unexpectedly true");

  if (!checkSig || isServer) {
      PR_SetError(PR_INVALID_STATE_ERROR, 0);
      return SECFailure;
  }
      
  CERTCertificate *serverCert = SSL_PeerCertificate(fd);

  nsNSSSocketInfo *socketInfo = static_cast<nsNSSSocketInfo*>(arg);
  SECStatus rv = SSLServerCertVerificationJob::Dispatch(
                        static_cast<const void *>(fd), socketInfo, serverCert);

  CERT_DestroyCertificate(serverCert);

  return rv;
}

SSLServerCertVerificationResult::SSLServerCertVerificationResult(
        nsNSSSocketInfo & socketInfo, PRErrorCode errorCode,
        SSLErrorMessageType errorMessageType)
  : mSocketInfo(&socketInfo)
  , mErrorCode(errorCode)
  , mErrorMessageType(errorMessageType)
{
}

void
SSLServerCertVerificationResult::Dispatch()
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> stsTarget
    = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  NS_ASSERTION(stsTarget,
               "Failed to get socket transport service event target");
  rv = stsTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ASSERTION(NS_SUCCEEDED(rv), 
               "Failed to dispatch SSLServerCertVerificationResult");
}

NS_IMETHODIMP
SSLServerCertVerificationResult::Run()
{
  // TODO: Assert that we're on the socket transport thread
  mSocketInfo->SetCertVerificationResult(mErrorCode, mErrorMessageType);
  return NS_OK;
}

} } // namespace mozilla::psm

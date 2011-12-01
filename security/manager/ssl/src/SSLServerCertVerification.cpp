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
#include "nsNSSComponent.h"
#include "nsNSSCertificate.h"
#include "nsNSSCleaner.h"
#include "nsNSSIOLayer.h"

#include "ssl.h"
#include "cert.h"
#include "secerr.h"
#include "sslerr.h"

using namespace mozilla;

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);
NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

SECStatus
PSM_SSL_PKIX_AuthCertificate(PRFileDesc *fd, CERTCertificate *peerCert, bool checksig, bool isServer)
{
    SECStatus          rv;
    SECCertUsage       certUsage;
    SECCertificateUsage certificateusage;
    void *             pinarg;
    char *             hostname;
    
    pinarg = SSL_RevealPinArg(fd);
    hostname = SSL_RevealURL(fd);
    
    /* this may seem backwards, but isn't. */
    certUsage = isServer ? certUsageSSLClient : certUsageSSLServer;
    certificateusage = isServer ? certificateUsageSSLClient : certificateUsageSSLServer;

    if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
        rv = CERT_VerifyCertNow(CERT_GetDefaultCertDB(), peerCert, checksig, certUsage,
                                pinarg);
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

        rv = CERT_PKIXVerifyCert(peerCert, certificateusage,
                                survivingParams->GetRawPointerForNSS(),
                                cvout, pinarg);
    }

    if ( rv == SECSuccess && !isServer ) {
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
        
    PORT_Free(hostname);
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


SECStatus PR_CALLBACK AuthCertificateCallback(void* client_data, PRFileDesc* fd,
                                              PRBool checksig, PRBool isServer) {
  nsNSSShutDownPreventionLock locker;

  CERTCertificate *serverCert = SSL_PeerCertificate(fd);
  CERTCertificateCleaner serverCertCleaner(serverCert);

  if (serverCert && 
      serverCert->serialNumber.data &&
      serverCert->issuerName &&
      !strcmp(serverCert->issuerName, 
        "CN=UTN-USERFirst-Hardware,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US")) {

    unsigned char *server_cert_comparison_start = (unsigned char*)serverCert->serialNumber.data;
    unsigned int server_cert_comparison_len = serverCert->serialNumber.len;

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

  SECStatus rv = PSM_SSL_PKIX_AuthCertificate(fd, serverCert, checksig, isServer);

  // We want to remember the CA certs in the temp db, so that the application can find the
  // complete chain at any time it might need it.
  // But we keep only those CA certs in the temp db, that we didn't already know.

  if (serverCert) {
    nsNSSSocketInfo* infoObject = (nsNSSSocketInfo*) fd->higher->secret;
    nsRefPtr<nsSSLStatus> status = infoObject->SSLStatus();
    nsRefPtr<nsNSSCertificate> nsc;

    if (!status || !status->mServerCert) {
      nsc = nsNSSCertificate::Create(serverCert);
    }

    CERTCertList *certList = nsnull;
    certList = CERT_GetCertChainFromCert(serverCert, PR_Now(), certUsageSSLCA);
    if (!certList) {
      rv = SECFailure;
    } else {
      PRErrorCode blacklistErrorCode;
      if (rv == SECSuccess) { // PSM_SSL_PKIX_AuthCertificate said "valid cert"
        blacklistErrorCode = PSM_SSL_BlacklistDigiNotar(serverCert, certList);
      } else { // PSM_SSL_PKIX_AuthCertificate said "invalid cert"
        PRErrorCode savedErrorCode = PORT_GetError();
        // Check if we want to worsen the error code to "revoked".
        blacklistErrorCode = PSM_SSL_DigiNotarTreatAsRevoked(serverCert, certList);
        if (blacklistErrorCode == 0) {
          // we don't worsen the code, let's keep the original error code from NSS
          PORT_SetError(savedErrorCode);
        }
      }
      
      if (blacklistErrorCode != 0) {
        infoObject->SetCertIssuerBlacklisted();
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
        
        if (node->cert == serverCert) {
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

    }

    if (certList) {
      CERT_DestroyCertList(certList);
    }

    // The connection may get terminated, for example, if the server requires
    // a client cert. Let's provide a minimal SSLStatus
    // to the caller that contains at least the cert and its status.
    if (!status) {
      status = new nsSSLStatus();
      infoObject->SetSSLStatus(status);
    }

    if (rv == SECSuccess) {
      // Certificate verification succeeded delete any potential record
      // of certificate error bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->RememberCertHasError(
        infoObject, nsnull, rv);
    }
    else {
      // Certificate verification failed, update the status' bits.
      nsSSLIOLayerHelpers::mHostsWithCertErrors->LookupCertErrorBits(
        infoObject, status);
    }

    if (status && !status->mServerCert) {
      status->mServerCert = nsc;
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("AuthCertificateCallback setting NEW cert %p\n", status->mServerCert.get()));
    }
  }

  return rv;
}

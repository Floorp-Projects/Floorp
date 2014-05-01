/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCMS.h"

#include "CertVerifier.h"
#include "pkix/pkixtypes.h"
#include "nsISupports.h"
#include "nsNSSHelper.h"
#include "nsNSSCertificate.h"
#include "smime.h"
#include "cms.h"
#include "nsICMSMessageErrors.h"
#include "nsIArray.h"
#include "nsArrayUtils.h"
#include "nsCertVerificationThread.h"

#include "prlog.h"

using namespace mozilla;
using namespace mozilla::psm;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsCMSMessage, nsICMSMessage, nsICMSMessage2)

nsCMSMessage::nsCMSMessage()
{
  m_cmsMsg = nullptr;
}
nsCMSMessage::nsCMSMessage(NSSCMSMessage *aCMSMsg)
{
  m_cmsMsg = aCMSMsg;
}

nsCMSMessage::~nsCMSMessage()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCMSMessage::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCMSMessage::destructorSafeDestroyNSSReference()
{
  if (m_cmsMsg) {
    NSS_CMSMessage_Destroy(m_cmsMsg);
  }
}

NS_IMETHODIMP nsCMSMessage::VerifySignature()
{
  return CommonVerifySignature(nullptr, 0);
}

NSSCMSSignerInfo* nsCMSMessage::GetTopLevelSignerInfo()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return nullptr;

  if (!m_cmsMsg)
    return nullptr;

  if (!NSS_CMSMessage_IsSigned(m_cmsMsg))
    return nullptr;

  NSSCMSContentInfo *cinfo = NSS_CMSMessage_ContentLevel(m_cmsMsg, 0);
  if (!cinfo)
    return nullptr;

  NSSCMSSignedData *sigd = (NSSCMSSignedData*)NSS_CMSContentInfo_GetContent(cinfo);
  if (!sigd)
    return nullptr;

  PR_ASSERT(NSS_CMSSignedData_SignerInfoCount(sigd) > 0);
  return NSS_CMSSignedData_GetSignerInfo(sigd, 0);
}

NS_IMETHODIMP nsCMSMessage::GetSignerEmailAddress(char * * aEmail)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::GetSignerEmailAddress\n"));
  NS_ENSURE_ARG(aEmail);

  NSSCMSSignerInfo *si = GetTopLevelSignerInfo();
  if (!si)
    return NS_ERROR_FAILURE;

  *aEmail = NSS_CMSSignerInfo_GetSignerEmailAddress(si);
  return NS_OK;
}

NS_IMETHODIMP nsCMSMessage::GetSignerCommonName(char ** aName)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::GetSignerCommonName\n"));
  NS_ENSURE_ARG(aName);

  NSSCMSSignerInfo *si = GetTopLevelSignerInfo();
  if (!si)
    return NS_ERROR_FAILURE;

  *aName = NSS_CMSSignerInfo_GetSignerCommonName(si);
  return NS_OK;
}

NS_IMETHODIMP nsCMSMessage::ContentIsEncrypted(bool *isEncrypted)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::ContentIsEncrypted\n"));
  NS_ENSURE_ARG(isEncrypted);

  if (!m_cmsMsg)
    return NS_ERROR_FAILURE;

  *isEncrypted = NSS_CMSMessage_IsEncrypted(m_cmsMsg);

  return NS_OK;
}

NS_IMETHODIMP nsCMSMessage::ContentIsSigned(bool *isSigned)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::ContentIsSigned\n"));
  NS_ENSURE_ARG(isSigned);

  if (!m_cmsMsg)
    return NS_ERROR_FAILURE;

  *isSigned = NSS_CMSMessage_IsSigned(m_cmsMsg);

  return NS_OK;
}

NS_IMETHODIMP nsCMSMessage::GetSignerCert(nsIX509Cert **scert)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NSSCMSSignerInfo *si = GetTopLevelSignerInfo();
  if (!si)
    return NS_ERROR_FAILURE;

  if (si->cert) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::GetSignerCert got signer cert\n"));

    *scert = nsNSSCertificate::Create(si->cert);
    if (*scert) {
      (*scert)->AddRef();
    }
  }
  else {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::GetSignerCert no signer cert, do we have a cert list? %s\n",
      (si->certList ? "yes" : "no") ));

    *scert = nullptr;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsCMSMessage::GetEncryptionCert(nsIX509Cert **ecert)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCMSMessage::VerifyDetachedSignature(unsigned char* aDigestData, uint32_t aDigestDataLen)
{
  if (!aDigestData || !aDigestDataLen)
    return NS_ERROR_FAILURE;

  return CommonVerifySignature(aDigestData, aDigestDataLen);
}

nsresult nsCMSMessage::CommonVerifySignature(unsigned char* aDigestData, uint32_t aDigestDataLen)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature, content level count %d\n", NSS_CMSMessage_ContentLevelCount(m_cmsMsg)));
  NSSCMSContentInfo *cinfo = nullptr;
  NSSCMSSignedData *sigd = nullptr;
  NSSCMSSignerInfo *si;
  int32_t nsigners;
  RefPtr<SharedCertVerifier> certVerifier;
  nsresult rv = NS_ERROR_FAILURE;

  if (!NSS_CMSMessage_IsSigned(m_cmsMsg)) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - not signed\n"));
    return NS_ERROR_CMS_VERIFY_NOT_SIGNED;
  } 

  cinfo = NSS_CMSMessage_ContentLevel(m_cmsMsg, 0);
  if (cinfo) {
    // I don't like this hard cast. We should check in some way, that we really have this type.
    sigd = (NSSCMSSignedData*)NSS_CMSContentInfo_GetContent(cinfo);
  }
  
  if (!sigd) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - no content info\n"));
    rv = NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO;
    goto loser;
  }

  if (aDigestData && aDigestDataLen)
  {
    SECItem digest;
    digest.data = aDigestData;
    digest.len = aDigestDataLen;

    if (NSS_CMSSignedData_SetDigestValue(sigd, SEC_OID_SHA1, &digest)) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - bad digest\n"));
      rv = NS_ERROR_CMS_VERIFY_BAD_DIGEST;
      goto loser;
    }
  }

  // Import certs. Note that import failure is not a signature verification failure. //
  if (NSS_CMSSignedData_ImportCerts(sigd, CERT_GetDefaultCertDB(), certUsageEmailRecipient, true) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - can not import certs\n"));
  }

  nsigners = NSS_CMSSignedData_SignerInfoCount(sigd);
  PR_ASSERT(nsigners > 0);
  NS_ENSURE_TRUE(nsigners > 0, NS_ERROR_UNEXPECTED);
  si = NSS_CMSSignedData_GetSignerInfo(sigd, 0);

  // See bug 324474. We want to make sure the signing cert is 
  // still valid at the current time.

  certVerifier = GetDefaultCertVerifier();
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);

  {
    SECStatus srv = certVerifier->VerifyCert(si->cert,
                                             certificateUsageEmailSigner,
                                             PR_Now(), nullptr /*XXX pinarg*/,
                                             nullptr /*hostname*/);
    if (srv != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
             ("nsCMSMessage::CommonVerifySignature - signing cert not trusted now\n"));
      rv = NS_ERROR_CMS_VERIFY_UNTRUSTED;
      goto loser;
    }
  }

  // We verify the first signer info,  only //
  if (NSS_CMSSignedData_VerifySignerInfo(sigd, 0, CERT_GetDefaultCertDB(), certUsageEmailSigner) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - unable to verify signature\n"));

    if (NSSCMSVS_SigningCertNotFound == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - signing cert not found\n"));
      rv = NS_ERROR_CMS_VERIFY_NOCERT;
    }
    else if(NSSCMSVS_SigningCertNotTrusted == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - signing cert not trusted at signing time\n"));
      rv = NS_ERROR_CMS_VERIFY_UNTRUSTED;
    }
    else if(NSSCMSVS_Unverified == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - can not verify\n"));
      rv = NS_ERROR_CMS_VERIFY_ERROR_UNVERIFIED;
    }
    else if(NSSCMSVS_ProcessingError == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - processing error\n"));
      rv = NS_ERROR_CMS_VERIFY_ERROR_PROCESSING;
    }
    else if(NSSCMSVS_BadSignature == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - bad signature\n"));
      rv = NS_ERROR_CMS_VERIFY_BAD_SIGNATURE;
    }
    else if(NSSCMSVS_DigestMismatch == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - digest mismatch\n"));
      rv = NS_ERROR_CMS_VERIFY_DIGEST_MISMATCH;
    }
    else if(NSSCMSVS_SignatureAlgorithmUnknown == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - algo unknown\n"));
      rv = NS_ERROR_CMS_VERIFY_UNKNOWN_ALGO;
    }
    else if(NSSCMSVS_SignatureAlgorithmUnsupported == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - algo not supported\n"));
      rv = NS_ERROR_CMS_VERIFY_UNSUPPORTED_ALGO;
    }
    else if(NSSCMSVS_MalformedSignature == si->verificationStatus) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - malformed signature\n"));
      rv = NS_ERROR_CMS_VERIFY_MALFORMED_SIGNATURE;
    }

    goto loser;
  }

  // Save the profile. Note that save import failure is not a signature verification failure. //
  if (NSS_SMIMESignerInfo_SaveSMIMEProfile(si) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CommonVerifySignature - unable to save smime profile\n"));
  }

  rv = NS_OK;
loser:
  return rv;
}

NS_IMETHODIMP nsCMSMessage::AsyncVerifySignature(
                              nsISMimeVerificationListener *aListener)
{
  return CommonAsyncVerifySignature(aListener, nullptr, 0);
}

NS_IMETHODIMP nsCMSMessage::AsyncVerifyDetachedSignature(
                              nsISMimeVerificationListener *aListener,
                              unsigned char* aDigestData, uint32_t aDigestDataLen)
{
  if (!aDigestData || !aDigestDataLen)
    return NS_ERROR_FAILURE;

  return CommonAsyncVerifySignature(aListener, aDigestData, aDigestDataLen);
}

nsresult nsCMSMessage::CommonAsyncVerifySignature(nsISMimeVerificationListener *aListener,
                                                  unsigned char* aDigestData, uint32_t aDigestDataLen)
{
  nsSMimeVerificationJob *job = new nsSMimeVerificationJob;
  
  if (aDigestData)
  {
    job->digest_data = new unsigned char[aDigestDataLen];
    memcpy(job->digest_data, aDigestData, aDigestDataLen);
  }
  else
  {
    job->digest_data = nullptr;
  }
  
  job->digest_len = aDigestDataLen;
  job->mMessage = this;
  job->mListener = aListener;

  nsresult rv = nsCertVerificationThread::addJob(job);
  if (NS_FAILED(rv))
    delete job;

  return rv;
}

class nsZeroTerminatedCertArray : public nsNSSShutDownObject
{
public:
  nsZeroTerminatedCertArray()
  :mCerts(nullptr), mPoolp(nullptr), mSize(0)
  {
  }
  
  ~nsZeroTerminatedCertArray()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown()) {
      return;
    }
    destructorSafeDestroyNSSReference();
    shutdown(calledFromObject);
  }

  void virtualDestroyNSSReference()
  {
    destructorSafeDestroyNSSReference();
  }

  void destructorSafeDestroyNSSReference()
  {
    if (mCerts)
    {
      for (uint32_t i=0; i < mSize; i++) {
        if (mCerts[i]) {
          CERT_DestroyCertificate(mCerts[i]);
        }
      }
    }

    if (mPoolp)
      PORT_FreeArena(mPoolp, false);
  }

  bool allocate(uint32_t count)
  {
    // only allow allocation once
    if (mPoolp)
      return false;
  
    mSize = count;

    if (!mSize)
      return false;
  
    mPoolp = PORT_NewArena(1024);
    if (!mPoolp)
      return false;

    mCerts = (CERTCertificate**)PORT_ArenaZAlloc(
      mPoolp, (count+1)*sizeof(CERTCertificate*));

    if (!mCerts)
      return false;

    // null array, including zero termination
    for (uint32_t i = 0; i < count+1; i++) {
      mCerts[i] = nullptr;
    }

    return true;
  }
  
  void set(uint32_t i, CERTCertificate *c)
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return;

    if (i >= mSize)
      return;
    
    if (mCerts[i]) {
      CERT_DestroyCertificate(mCerts[i]);
    }
    
    mCerts[i] = CERT_DupCertificate(c);
  }
  
  CERTCertificate *get(uint32_t i)
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return nullptr;

    if (i >= mSize)
      return nullptr;
    
    return CERT_DupCertificate(mCerts[i]);
  }

  CERTCertificate **getRawArray()
  {
    nsNSSShutDownPreventionLock locker;
    if (isAlreadyShutDown())
      return nullptr;

    return mCerts;
  }

private:
  CERTCertificate **mCerts;
  PLArenaPool *mPoolp;
  uint32_t mSize;
};

NS_IMETHODIMP nsCMSMessage::CreateEncrypted(nsIArray * aRecipientCerts)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted\n"));
  NSSCMSContentInfo *cinfo;
  NSSCMSEnvelopedData *envd;
  NSSCMSRecipientInfo *recipientInfo;
  nsZeroTerminatedCertArray recipientCerts;
  SECOidTag bulkAlgTag;
  int keySize;
  uint32_t i;
  nsCOMPtr<nsIX509Cert2> nssRecipientCert;
  nsresult rv = NS_ERROR_FAILURE;

  // Check the recipient certificates //
  uint32_t recipientCertCount;
  aRecipientCerts->GetLength(&recipientCertCount);
  PR_ASSERT(recipientCertCount > 0);

  if (!recipientCerts.allocate(recipientCertCount)) {
    goto loser;
  }

  for (i=0; i<recipientCertCount; i++) {
    nsCOMPtr<nsIX509Cert> x509cert = do_QueryElementAt(aRecipientCerts, i);

    nssRecipientCert = do_QueryInterface(x509cert);

    if (!nssRecipientCert)
      return NS_ERROR_FAILURE;

    mozilla::pkix::ScopedCERTCertificate c(nssRecipientCert->GetCert());
    recipientCerts.set(i, c.get());
  }
  
  // Find a bulk key algorithm //
  if (NSS_SMIMEUtil_FindBulkAlgForRecipients(recipientCerts.getRawArray(), &bulkAlgTag,
                                            &keySize) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't find bulk alg for recipients\n"));
    rv = NS_ERROR_CMS_ENCRYPT_NO_BULK_ALG;
    goto loser;
  }

  m_cmsMsg = NSS_CMSMessage_Create(nullptr);
  if (!m_cmsMsg) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't create new cms message\n"));
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto loser;
  }

  if ((envd = NSS_CMSEnvelopedData_Create(m_cmsMsg, bulkAlgTag, keySize)) == nullptr) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't create enveloped data\n"));
    goto loser;
  }

  cinfo = NSS_CMSMessage_GetContentInfo(m_cmsMsg);
  if (NSS_CMSContentInfo_SetContent_EnvelopedData(m_cmsMsg, cinfo, envd) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't create content enveloped data\n"));
    goto loser;
  }

  cinfo = NSS_CMSEnvelopedData_GetContentInfo(envd);
  if (NSS_CMSContentInfo_SetContent_Data(m_cmsMsg, cinfo, nullptr, false) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't set content data\n"));
    goto loser;
  }

  // Create and attach recipient information //
  for (i=0; i < recipientCertCount; i++) {
    mozilla::pkix::ScopedCERTCertificate rc(recipientCerts.get(i));
    if ((recipientInfo = NSS_CMSRecipientInfo_Create(m_cmsMsg, rc.get())) == nullptr) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't create recipient info\n"));
      goto loser;
    }
    if (NSS_CMSEnvelopedData_AddRecipient(envd, recipientInfo) != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateEncrypted - can't add recipient info\n"));
      goto loser;
    }
  }

  return NS_OK;
loser:
  if (m_cmsMsg) {
    NSS_CMSMessage_Destroy(m_cmsMsg);
    m_cmsMsg = nullptr;
  }

  return rv;
}

NS_IMETHODIMP nsCMSMessage::CreateSigned(nsIX509Cert* aSigningCert, nsIX509Cert* aEncryptCert, unsigned char* aDigestData, uint32_t aDigestDataLen)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned\n"));
  NSSCMSContentInfo *cinfo;
  NSSCMSSignedData *sigd;
  NSSCMSSignerInfo *signerinfo;
  mozilla::pkix::ScopedCERTCertificate scert;
  mozilla::pkix::ScopedCERTCertificate ecert;
  nsCOMPtr<nsIX509Cert2> aSigningCert2 = do_QueryInterface(aSigningCert);
  nsresult rv = NS_ERROR_FAILURE;

  /* Get the certs */
  if (aSigningCert2) {
    scert = aSigningCert2->GetCert();
  }
  if (!scert) {
    return NS_ERROR_FAILURE;
  }

  if (aEncryptCert) {
    nsCOMPtr<nsIX509Cert2> aEncryptCert2 = do_QueryInterface(aEncryptCert);
    if (aEncryptCert2) {
      ecert = aEncryptCert2->GetCert();
    }
  }

  /*
   * create the message object
   */
  m_cmsMsg = NSS_CMSMessage_Create(nullptr); /* create a message on its own pool */
  if (!m_cmsMsg) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't create new message\n"));
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto loser;
  }

  /*
   * build chain of objects: message->signedData->data
   */
  if ((sigd = NSS_CMSSignedData_Create(m_cmsMsg)) == nullptr) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't create signed data\n"));
    goto loser;
  }
  cinfo = NSS_CMSMessage_GetContentInfo(m_cmsMsg);
  if (NSS_CMSContentInfo_SetContent_SignedData(m_cmsMsg, cinfo, sigd) 
          != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't set content signed data\n"));
    goto loser;
  }

  cinfo = NSS_CMSSignedData_GetContentInfo(sigd);

  /* we're always passing data in and detaching optionally */
  if (NSS_CMSContentInfo_SetContent_Data(m_cmsMsg, cinfo, nullptr, true) 
          != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't set content data\n"));
    goto loser;
  }

  /* 
   * create & attach signer information
   */
  if ((signerinfo = NSS_CMSSignerInfo_Create(m_cmsMsg, scert.get(), SEC_OID_SHA1)) 
          == nullptr) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't create signer info\n"));
    goto loser;
  }

  /* we want the cert chain included for this one */
  if (NSS_CMSSignerInfo_IncludeCerts(signerinfo, NSSCMSCM_CertChain, 
                                       certUsageEmailSigner) 
          != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't include signer cert chain\n"));
    goto loser;
  }

  if (NSS_CMSSignerInfo_AddSigningTime(signerinfo, PR_Now()) 
	      != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add signing time\n"));
    goto loser;
  }

  if (NSS_CMSSignerInfo_AddSMIMECaps(signerinfo) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add smime caps\n"));
    goto loser;
  }

  if (ecert) {
    if (NSS_CMSSignerInfo_AddSMIMEEncKeyPrefs(signerinfo, ecert.get(),
	                                      CERT_GetDefaultCertDB())
	  != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add smime enc key prefs\n"));
      goto loser;
    }

    if (NSS_CMSSignerInfo_AddMSSMIMEEncKeyPrefs(signerinfo, ecert.get(),
	                                        CERT_GetDefaultCertDB())
	  != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add MS smime enc key prefs\n"));
      goto loser;
    }

    // If signing and encryption cert are identical, don't add it twice.
    bool addEncryptionCert =
      (ecert && (!scert || !CERT_CompareCerts(ecert.get(), scert.get())));

    if (addEncryptionCert &&
        NSS_CMSSignedData_AddCertificate(sigd, ecert.get()) != SECSuccess) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add own encryption certificate\n"));
      goto loser;
    }
  }

  if (NSS_CMSSignedData_AddSignerInfo(sigd, signerinfo) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't add signer info\n"));
    goto loser;
  }

  // Finally, add the pre-computed digest if passed in
  if (aDigestData) {
    SECItem digest;

    digest.data = aDigestData;
    digest.len = aDigestDataLen;

    if (NSS_CMSSignedData_SetDigestValue(sigd, SEC_OID_SHA1, &digest)) {
      PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSMessage::CreateSigned - can't set digest value\n"));
      goto loser;
    }
  }

  return NS_OK;
loser:
  if (m_cmsMsg) {
    NSS_CMSMessage_Destroy(m_cmsMsg);
    m_cmsMsg = nullptr;
  }
  return rv;
}

NS_IMPL_ISUPPORTS(nsCMSDecoder, nsICMSDecoder)

nsCMSDecoder::nsCMSDecoder()
: m_dcx(nullptr)
{
}

nsCMSDecoder::~nsCMSDecoder()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCMSDecoder::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCMSDecoder::destructorSafeDestroyNSSReference()
{
  if (m_dcx) {
    NSS_CMSDecoder_Cancel(m_dcx);
    m_dcx = nullptr;
  }
}

/* void start (in NSSCMSContentCallback cb, in voidPtr arg); */
NS_IMETHODIMP nsCMSDecoder::Start(NSSCMSContentCallback cb, void * arg)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSDecoder::Start\n"));
  m_ctx = new PipUIContext();

  m_dcx = NSS_CMSDecoder_Start(0, cb, arg, 0, m_ctx, 0, 0);
  if (!m_dcx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSDecoder::Start - can't start decoder\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* void update (in string bug, in long len); */
NS_IMETHODIMP nsCMSDecoder::Update(const char *buf, int32_t len)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSDecoder::Update\n"));
  NSS_CMSDecoder_Update(m_dcx, (char *)buf, len);
  return NS_OK;
}

/* void finish (); */
NS_IMETHODIMP nsCMSDecoder::Finish(nsICMSMessage ** aCMSMsg)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSDecoder::Finish\n"));
  NSSCMSMessage *cmsMsg;
  cmsMsg = NSS_CMSDecoder_Finish(m_dcx);
  m_dcx = nullptr;
  if (cmsMsg) {
    nsCMSMessage *obj = new nsCMSMessage(cmsMsg);
    // The NSS object cmsMsg still carries a reference to the context
    // we gave it on construction.
    // Make sure the context will live long enough.
    obj->referenceContext(m_ctx);
    *aCMSMsg = obj;
    NS_ADDREF(*aCMSMsg);
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsCMSEncoder, nsICMSEncoder)

nsCMSEncoder::nsCMSEncoder()
: m_ecx(nullptr)
{
}

nsCMSEncoder::~nsCMSEncoder()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown()) {
    return;
  }
  destructorSafeDestroyNSSReference();
  shutdown(calledFromObject);
}

void nsCMSEncoder::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void nsCMSEncoder::destructorSafeDestroyNSSReference()
{
  if (m_ecx)
    NSS_CMSEncoder_Cancel(m_ecx);
}

/* void start (); */
NS_IMETHODIMP nsCMSEncoder::Start(nsICMSMessage *aMsg, NSSCMSContentCallback cb, void * arg)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Start\n"));
  nsCMSMessage *cmsMsg = static_cast<nsCMSMessage*>(aMsg);
  m_ctx = new PipUIContext();

  m_ecx = NSS_CMSEncoder_Start(cmsMsg->getCMS(), cb, arg, 0, 0, 0, m_ctx, 0, 0, 0, 0);
  if (!m_ecx) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Start - can't start encoder\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* void update (in string aBuf, in long aLen); */
NS_IMETHODIMP nsCMSEncoder::Update(const char *aBuf, int32_t aLen)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Update\n"));
  if (!m_ecx || NSS_CMSEncoder_Update(m_ecx, aBuf, aLen) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Update - can't update encoder\n"));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* void finish (); */
NS_IMETHODIMP nsCMSEncoder::Finish()
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = NS_OK;
  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Finish\n"));
  if (!m_ecx || NSS_CMSEncoder_Finish(m_ecx) != SECSuccess) {
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Finish - can't finish encoder\n"));
    rv = NS_ERROR_FAILURE;
  }
  m_ecx = nullptr;
  return rv;
}

/* void encode (in nsICMSMessage aMsg); */
NS_IMETHODIMP nsCMSEncoder::Encode(nsICMSMessage *aMsg)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("nsCMSEncoder::Encode\n"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

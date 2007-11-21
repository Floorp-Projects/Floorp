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
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include "nsAppDirectoryServiceDefs.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsPromiseFlatString.h"
#include "nsTArray.h"

#include "cert.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNSSCertificate.h"
#include "nsNSSCleaner.h"

#ifdef DEBUG
#ifndef PSM_ENABLE_TEST_EV_ROOTS
#define PSM_ENABLE_TEST_EV_ROOTS
#endif
#endif

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(CERTCertList, CERT_DestroyCertList)
NSSCleanupAutoPtrClass_WithParam(SECItem, SECITEM_FreeItem, TrueParam, PR_TRUE)

#define CONST_OID static const unsigned char
#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }

struct nsMyTrustedEVInfo
{
  const char *dotted_oid;
  const char *oid_name; // Set this to null to signal an invalid structure,
                        // (We can't have an empty list, so we'll use a dummy entry)
  SECOidTag oid_tag;
  const char *ev_root_subject;
  const char *ev_root_issuer;
  const char *ev_root_sha1_fingerprint;
};

static struct nsMyTrustedEVInfo myTrustedEVInfos[] = {
  {
    "2.16.840.1.113733.1.7.23.6",
    "Verisign EV OID",
    SEC_OID_UNKNOWN,
    "OU=Class 3 Public Primary Certification Authority,O=\"VeriSign, Inc.\",C=US",
    "OU=Class 3 Public Primary Certification Authority,O=\"VeriSign, Inc.\",C=US",
    "74:2C:31:92:E6:07:E4:24:EB:45:49:54:2B:E1:BB:C5:3E:61:74:E2"
  },
  {
    "0.0.0.0",
    0, // for real entries use a string like "Sample INVALID EV OID"
    SEC_OID_UNKNOWN,
    "OU=Sample Certification Authority,O=\"Sample, Inc.\",C=US",
    "OU=Sample Certification Authority,O=\"Sample, Inc.\",C=US",
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33"
  }
};

static SECOidTag
register_oid(const SECItem *oid_item, const char *oid_name)
{
  if (!oid_item)
    return SEC_OID_UNKNOWN;

  SECOidData od;
  od.oid.len = oid_item->len;
  od.oid.data = oid_item->data;
  od.offset = SEC_OID_UNKNOWN;
  od.desc = oid_name;
  od.mechanism = CKM_INVALID_MECHANISM;
  od.supportedExtension = INVALID_CERT_EXTENSION;
  return SECOID_AddEntry(&od);
}

#ifdef PSM_ENABLE_TEST_EV_ROOTS
class nsMyTrustedEVInfoClass : public nsMyTrustedEVInfo
{
public:
  nsMyTrustedEVInfoClass();
  ~nsMyTrustedEVInfoClass();
};

nsMyTrustedEVInfoClass::nsMyTrustedEVInfoClass()
{
  dotted_oid = nsnull;
  oid_name = nsnull;
  oid_tag = SEC_OID_UNKNOWN;
  ev_root_subject = nsnull;
  ev_root_issuer = nsnull;
  ev_root_sha1_fingerprint = nsnull;
}

nsMyTrustedEVInfoClass::~nsMyTrustedEVInfoClass()
{
  delete dotted_oid;
  delete oid_name;
  delete ev_root_subject;
  delete ev_root_issuer;
  delete ev_root_sha1_fingerprint;
}

typedef nsTArray< nsMyTrustedEVInfoClass* > testEVArray; 
static testEVArray *testEVInfos;
static PRBool testEVInfosLoaded = PR_FALSE;
#endif

static PRBool isEVMatch(SECOidTag policyOIDTag, 
                        CERTCertificate *rootCert, 
                        const nsMyTrustedEVInfo &info)
{
  if (!rootCert)
    return PR_FALSE;

  NS_ConvertUTF8toUTF16 info_subject(info.ev_root_subject);
  NS_ConvertUTF8toUTF16 info_issuer(info.ev_root_issuer);
  NS_ConvertASCIItoUTF16 info_sha1(info.ev_root_sha1_fingerprint);

  nsNSSCertificate c(rootCert);

  nsAutoString subjectName;
  if (NS_FAILED(c.GetSubjectName(subjectName)))
    return PR_FALSE;

  if (subjectName != info_subject)
    return PR_FALSE;

  nsAutoString issuerName;
  if (NS_FAILED(c.GetIssuerName(issuerName)))
    return PR_FALSE;

  if (issuerName != info_issuer)
    return PR_FALSE;

  nsAutoString fingerprint;
  if (NS_FAILED(c.GetSha1Fingerprint(fingerprint)))
    return PR_FALSE;

  if (fingerprint != info_sha1)
    return PR_FALSE;

  return (policyOIDTag == info.oid_tag);
}

#ifdef PSM_ENABLE_TEST_EV_ROOTS
static const char kTestEVRootsFileName[] = "test_ev_roots.txt";

static void
loadTestEVInfos()
{
  if (!testEVInfos)
    return;

  testEVInfos->Clear();

  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return;

  nsCOMPtr<nsIFile> aFile;
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (!aFile)
    return;

  aFile->AppendNative(NS_LITERAL_CSTRING(kTestEVRootsFileName));

  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), aFile);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv))
    return;

  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;

  /* file format
   *
   * file format must be strictly followed
   * strings in file must be UTF-8
   * each record consists of multiple lines
   * each line consists of a descriptor, a single space, and the data
   * the descriptors are:
   *   1_subject
   *   2_issuer
   *   3_fingerprint (in format XX:XX:XX:...)
   *   4_readable_oid (treated as a comment)
   * the input file must strictly follow this order
   * the input file may contain 0, 1 or many records
   * completely empty lines are ignored
   * lines that start with the # char are ignored
   */

  int line_counter = 0;
  PRBool found_error = PR_FALSE;

  enum { 
    pos_subject, pos_issuer, pos_fingerprint, pos_readable_oid
  } reader_position = pos_subject;

  nsCString subject, issuer, fingerprint, readable_oid;

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(buffer, &isMore))) {
    ++line_counter;
    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    PRInt32 seperatorIndex = buffer.FindChar(' ', 0);
    if (seperatorIndex == 0) {
      found_error = PR_TRUE;
      break;
    }

    const nsASingleFragmentCString &descriptor = Substring(buffer, 0, seperatorIndex);
    const nsASingleFragmentCString &data = 
            Substring(buffer, seperatorIndex + 1, 
                      buffer.Length() - seperatorIndex + 1);

    if (reader_position == pos_subject &&
        descriptor.EqualsLiteral(("1_subject"))) {
      subject = data;
      reader_position = pos_issuer;
      continue;
    }
    else if (reader_position == pos_issuer &&
        descriptor.EqualsLiteral(("2_issuer"))) {
      issuer = data;
      reader_position = pos_fingerprint;
      continue;
    }
    else if (reader_position == pos_fingerprint &&
        descriptor.EqualsLiteral(("3_fingerprint"))) {
      fingerprint = data;
      reader_position = pos_readable_oid;
      continue;
    }
    else if (reader_position == pos_readable_oid &&
        descriptor.EqualsLiteral(("4_readable_oid"))) {
      readable_oid = data;
      reader_position = pos_subject;
    }
    else {
      found_error = PR_TRUE;
      break;
    }

    nsMyTrustedEVInfoClass *temp_ev = new nsMyTrustedEVInfoClass;
    if (!temp_ev)
      return;

    temp_ev->ev_root_subject = strdup(subject.get());
    temp_ev->ev_root_issuer = strdup(issuer.get());
    temp_ev->ev_root_sha1_fingerprint = strdup(fingerprint.get());
    temp_ev->oid_name = strdup(readable_oid.get());
    temp_ev->dotted_oid = strdup(readable_oid.get());

    SECItem ev_oid_item;
    ev_oid_item.data = nsnull;
    ev_oid_item.len = 0;
    SECStatus srv = SEC_StringToOID(nsnull, &ev_oid_item,
                                    readable_oid.get(), readable_oid.Length());
    if (srv != SECSuccess) {
      delete temp_ev;
      found_error = PR_TRUE;
      break;
    }

    temp_ev->oid_tag = register_oid(&ev_oid_item, temp_ev->oid_name);
    SECITEM_FreeItem(&ev_oid_item, PR_FALSE);

    testEVInfos->AppendElement(temp_ev);
  }

  if (found_error) {
    fprintf(stderr, "invalid line %d in test_ev_roots file\n", line_counter);
  }
}

static PRBool 
isEVPolicyInExternalDebugRootsFile(SECOidTag policyOIDTag)
{
  if (!testEVInfos)
    return PR_FALSE;

  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return PR_FALSE;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return PR_FALSE;

  for (size_t i=0; i<testEVInfos->Length(); ++i) {
    nsMyTrustedEVInfoClass *ev = testEVInfos->ElementAt(i);
    if (!ev)
      continue;
    if (policyOIDTag == ev->oid_tag)
      return PR_TRUE;
  }

  return PR_FALSE;
}

static PRBool 
isEVMatchInExternalDebugRootsFile(SECOidTag policyOIDTag, 
                                  CERTCertificate *rootCert)
{
  if (!testEVInfos)
    return PR_FALSE;

  if (!rootCert)
    return PR_FALSE;
  
  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return PR_FALSE;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return PR_FALSE;

  for (size_t i=0; i<testEVInfos->Length(); ++i) {
    nsMyTrustedEVInfoClass *ev = testEVInfos->ElementAt(i);
    if (!ev)
      continue;
    if (isEVMatch(policyOIDTag, rootCert, *ev))
      return PR_TRUE;
  }

  return PR_FALSE;
}
#endif

static PRBool 
isEVPolicy(SECOidTag policyOIDTag)
{
  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (policyOIDTag == entry.oid_tag) {
      return PR_TRUE;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (isEVPolicyInExternalDebugRootsFile(policyOIDTag)) {
    return PR_TRUE;
  }
#endif

  return PR_FALSE;
}

static PRBool 
isApprovedForEV(SECOidTag policyOIDTag, CERTCertificate *rootCert)
{
  if (!rootCert)
    return PR_FALSE;

  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (isEVMatch(policyOIDTag, rootCert, entry)) {
      return PR_TRUE;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (isEVMatchInExternalDebugRootsFile(policyOIDTag, rootCert)) {
    return PR_TRUE;
  }
#endif

  return PR_FALSE;
}

PRStatus PR_CALLBACK
nsNSSComponent::IdentityInfoInit()
{
  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;

    SECItem ev_oid_item;
    ev_oid_item.data = nsnull;
    ev_oid_item.len = 0;
    SECStatus srv = SEC_StringToOID(nsnull, &ev_oid_item, 
                                    entry.dotted_oid, 0);
    if (srv != SECSuccess)
      continue;

    entry.oid_tag = register_oid(&ev_oid_item, entry.oid_name);

    SECITEM_FreeItem(&ev_oid_item, PR_FALSE);
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (!testEVInfosLoaded) {
    testEVInfosLoaded = PR_TRUE;
    testEVInfos = new testEVArray;
    if (testEVInfos) {
      loadTestEVInfos();
    }
  }
#endif

  return PR_SUCCESS;
}

// Find the first policy OID that is known to be an EV policy OID.
static SECStatus getFirstEVPolicy(CERTCertificate *cert, SECOidTag &outOidTag)
{
  if (!cert)
    return SECFailure;

  if (cert->extensions) {
    for (int i=0; cert->extensions[i] != nsnull; i++) {
      const SECItem *oid = &cert->extensions[i]->id;

      SECOidTag oidTag = SECOID_FindOIDTag(oid);
      if (oidTag != SEC_OID_X509_CERTIFICATE_POLICIES)
        continue;

      SECItem *value = &cert->extensions[i]->value;

      CERTCertificatePolicies *policies;
      CERTPolicyInfo **policyInfos, *policyInfo;
    
      policies = CERT_DecodeCertificatePoliciesExtension(value);
      if (!policies)
        continue;
    
      policyInfos = policies->policyInfos;

      while (*policyInfos != NULL) {
        policyInfo = *policyInfos++;

        SECOidTag oid_tag = SECOID_FindOIDTag(&policyInfo->policyID);
        if (oid_tag == SEC_OID_UNKNOWN) // not in our list of OIDs accepted for EV
          continue;

        if (!isEVPolicy(oid_tag))
          continue;

        outOidTag = oid_tag;
        return SECSuccess;
      }
    }
  }

  return SECFailure;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetIsExtendedValidation(PRBool* aIsEV)
{
  NS_ENSURE_ARG(aIsEV);
  *aIsEV = PR_FALSE;

  if (!mCert)
    return NS_OK;

  return mCert->GetIsExtendedValidation(aIsEV);
}

NS_IMETHODIMP
nsNSSSocketInfo::GetValidEVPolicyOid(nsACString &outDottedOid)
{
  if (!mCert)
    return NS_OK;

  return mCert->GetValidEVPolicyOid(outDottedOid);
}

nsresult
nsNSSCertificate::hasValidEVOidTag(SECOidTag &resultOidTag, PRBool &validEV)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  nsresult nrv;
  nsCOMPtr<nsINSSComponent> nssComponent = 
    do_GetService(PSM_COMPONENT_CONTRACTID, &nrv);
  if (NS_FAILED(nrv))
    return nrv;
  nssComponent->EnsureIdentityInfoLoaded();

  validEV = PR_FALSE;
  resultOidTag = SEC_OID_UNKNOWN;

  SECOidTag oid_tag;
  SECStatus rv = getFirstEVPolicy(mCert, oid_tag);
  if (rv != SECSuccess)
    return NS_OK;

  if (oid_tag == SEC_OID_UNKNOWN) // not in our list of OIDs accepted for EV
    return NS_OK;

  CERTValInParam cvin[3];
  cvin[0].type = cert_pi_policyOID;
  cvin[0].value.arraySize = 1; 
  cvin[0].value.array.oids = &oid_tag;

  cvin[1].type = cert_pi_revocationFlags;
  cvin[1].value.scalar.ul = CERT_REV_FAIL_SOFT_CRL
                            | CERT_REV_FLAG_CRL
                            ;
  cvin[2].type = cert_pi_end;

  CERTValOutParam cvout[2];
  cvout[0].type = cert_po_trustAnchor;
  cvout[1].type = cert_po_end;

  rv = CERT_PKIXVerifyCert(mCert, certificateUsageSSLServer,
                           cvin, cvout, nsnull);
  if (rv != SECSuccess)
    return NS_OK;

  CERTCertificate *issuerCert = cvout[0].value.pointer.cert;
  CERTCertificateCleaner issuerCleaner(issuerCert);

  validEV = isApprovedForEV(oid_tag, issuerCert);
  if (validEV)
    resultOidTag = oid_tag;
 
  return NS_OK;
}

NS_IMETHODIMP
nsNSSCertificate::GetIsExtendedValidation(PRBool* aIsEV)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aIsEV);
  *aIsEV = PR_FALSE;

  SECOidTag oid_tag;
  return hasValidEVOidTag(oid_tag, *aIsEV);
}

NS_IMETHODIMP
nsNSSCertificate::GetValidEVPolicyOid(nsACString &outDottedOid)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECOidTag oid_tag;
  PRBool valid;
  nsresult rv = hasValidEVOidTag(oid_tag, valid);
  if (NS_FAILED(rv))
    return rv;

  if (valid) {
    SECOidData *oid_data = SECOID_FindOIDByTag(oid_tag);
    if (!oid_data)
      return NS_ERROR_FAILURE;

    char *oid_str = CERT_GetOidString(&oid_data->oid);
    if (!oid_str)
      return NS_ERROR_FAILURE;

    outDottedOid = oid_str;
    PR_smprintf_free(oid_str);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSComponent::EnsureIdentityInfoLoaded()
{
  PRStatus rv = PR_CallOnce(&mIdentityInfoCallOnce, IdentityInfoInit);
  return (rv == PR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE; 
}

// only called during shutdown
void
nsNSSComponent::CleanupIdentityInfo()
{
#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (testEVInfosLoaded) {
    testEVInfosLoaded = PR_FALSE;
    if (testEVInfos) {
      for (size_t i; i<testEVInfos->Length(); ++i) {
        delete testEVInfos->ElementAt(i);
      }
      testEVInfos->Clear();
      delete testEVInfos;
      testEVInfos = nsnull;
    }
  }
#endif
  memset(&mIdentityInfoCallOnce, 0, sizeof(PRCallOnceType));
}

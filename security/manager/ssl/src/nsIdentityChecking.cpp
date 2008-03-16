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
#include "base64.h"
#include "nsNSSComponent.h"
#include "nsNSSIOLayer.h"
#include "nsNSSCertificate.h"
#include "nsNSSCleaner.h"

#ifdef DEBUG
#ifndef PSM_ENABLE_TEST_EV_ROOTS
#define PSM_ENABLE_TEST_EV_ROOTS
#endif
#endif

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)
NSSCleanupAutoPtrClass(CERTCertList, CERT_DestroyCertList)
NSSCleanupAutoPtrClass_WithParam(SECItem, SECITEM_FreeItem, TrueParam, PR_TRUE)

#define CONST_OID static const unsigned char
#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }

struct nsMyTrustedEVInfo
{
  char *dotted_oid;
  char *oid_name; // Set this to null to signal an invalid structure,
                  // (We can't have an empty list, so we'll use a dummy entry)
  SECOidTag oid_tag;
  char *ev_root_sha1_fingerprint;
  char *issuer_base64;
  char *serial_base64;
  CERTCertificate *cert;
};

static struct nsMyTrustedEVInfo myTrustedEVInfos[] = {
  {
    // OU=Go Daddy Class 2 Certification Authority,O=\"The Go Daddy Group, Inc.\",C=US
    "2.16.840.1.114413.1.7.23.3",
    "Go Daddy EV OID a",
    SEC_OID_UNKNOWN,
    "27:96:BA:E6:3F:18:01:E2:77:26:1B:A0:D7:77:70:02:8F:20:EE:E4",
    "MGMxCzAJBgNVBAYTAlVTMSEwHwYDVQQKExhUaGUgR28gRGFkZHkgR3JvdXAsIElu"
    "Yy4xMTAvBgNVBAsTKEdvIERhZGR5IENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRo"
    "b3JpdHk=",
    "AA==",
    nsnull
  },
  {
    // E=info@valicert.com,CN=http://www.valicert.com/,OU=ValiCert Class 2 Policy Validation Authority,O=\"ValiCert, Inc.\",L=ValiCert Validation Network
    "2.16.840.1.114413.1.7.23.3",
    "Go Daddy EV OID a",
    SEC_OID_UNKNOWN,
    "31:7A:2A:D0:7F:2B:33:5E:F5:A1:C3:4E:4B:57:E8:B7:D8:F1:FC:A6",
    "MIG7MSQwIgYDVQQHExtWYWxpQ2VydCBWYWxpZGF0aW9uIE5ldHdvcmsxFzAVBgNV"
    "BAoTDlZhbGlDZXJ0LCBJbmMuMTUwMwYDVQQLEyxWYWxpQ2VydCBDbGFzcyAyIFBv"
    "bGljeSBWYWxpZGF0aW9uIEF1dGhvcml0eTEhMB8GA1UEAxMYaHR0cDovL3d3dy52"
    "YWxpY2VydC5jb20vMSAwHgYJKoZIhvcNAQkBFhFpbmZvQHZhbGljZXJ0LmNvbQ==",
    "AQ==",
    nsnull
  },
  {
    // E=info@valicert.com,CN=http://www.valicert.com/,OU=ValiCert Class 2 Policy Validation Authority,O=\"ValiCert, Inc.\",L=ValiCert Validation Network
    "2.16.840.1.114414.1.7.23.3",
    "Go Daddy EV OID b",
    SEC_OID_UNKNOWN,
    "31:7A:2A:D0:7F:2B:33:5E:F5:A1:C3:4E:4B:57:E8:B7:D8:F1:FC:A6",
    "MIG7MSQwIgYDVQQHExtWYWxpQ2VydCBWYWxpZGF0aW9uIE5ldHdvcmsxFzAVBgNV"
    "BAoTDlZhbGlDZXJ0LCBJbmMuMTUwMwYDVQQLEyxWYWxpQ2VydCBDbGFzcyAyIFBv"
    "bGljeSBWYWxpZGF0aW9uIEF1dGhvcml0eTEhMB8GA1UEAxMYaHR0cDovL3d3dy52"
    "YWxpY2VydC5jb20vMSAwHgYJKoZIhvcNAQkBFhFpbmZvQHZhbGljZXJ0LmNvbQ==",
    "AQ==",
    nsnull
  },
  {
    // OU=Starfield Class 2 Certification Authority,O=\"Starfield Technologies, Inc.\",C=US
    "2.16.840.1.114414.1.7.23.3",
    "Go Daddy EV OID b",
    SEC_OID_UNKNOWN,
    "AD:7E:1C:28:B0:64:EF:8F:60:03:40:20:14:C3:D0:E3:37:0E:B5:8A",
    "MGgxCzAJBgNVBAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVz"
    "LCBJbmMuMTIwMAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9u"
    "IEF1dGhvcml0eQ==",
    "AA==",
    nsnull
  },
  {
    // CN=DigiCert High Assurance EV Root CA,OU=www.digicert.com,O=DigiCert Inc,C=US
    "2.16.840.1.114412.2.1",
    "DigiCert EV OID",
    SEC_OID_UNKNOWN,
    "5F:B7:EE:06:33:E2:59:DB:AD:0C:4C:9A:E6:D3:8F:1A:61:C7:DC:25",
    "MGwxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsT"
    "EHd3dy5kaWdpY2VydC5jb20xKzApBgNVBAMTIkRpZ2lDZXJ0IEhpZ2ggQXNzdXJh"
    "bmNlIEVWIFJvb3QgQ0E=",
    "AqxcJmoLQJuPC3nyrkYldw==",
    nsnull
  },
  {
    // CN=QuoVadis Root CA 2,O=QuoVadis Limited,C=BM
    "1.3.6.1.4.1.8024.0.2.100.1.2",
    "Quo Vadis EV OID",
    SEC_OID_UNKNOWN,
    "CA:3A:FB:CF:12:40:36:4B:44:B2:16:20:88:80:48:39:19:93:7C:F7",
    "MEUxCzAJBgNVBAYTAkJNMRkwFwYDVQQKExBRdW9WYWRpcyBMaW1pdGVkMRswGQYD"
    "VQQDExJRdW9WYWRpcyBSb290IENBIDI=",
    "BQk=",
    nsnull
  },
  {
    // OU=Class 3 Public Primary Certification Authority,O=\"VeriSign, Inc.\",C=US
    "2.16.840.1.113733.1.7.23.6",
    "Verisign EV OID",
    SEC_OID_UNKNOWN,
    "74:2C:31:92:E6:07:E4:24:EB:45:49:54:2B:E1:BB:C5:3E:61:74:E2",
    "MF8xCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5WZXJpU2lnbiwgSW5jLjE3MDUGA1UE"
    "CxMuQ2xhc3MgMyBQdWJsaWMgUHJpbWFyeSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0"
    "eQ==",
    "cLrkHRDZKTS2OMp7A8y6vw==",
    nsnull
  },
  {
    // OU=Sample Certification Authority,O=\"Sample, Inc.\",C=US
    "0.0.0.0",
    0, // for real entries use a string like "Sample INVALID EV OID"
    SEC_OID_UNKNOWN,
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33"
    "Cg==",
    "Cg==",
    nsnull
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
  ev_root_sha1_fingerprint = nsnull;
  issuer_base64 = nsnull;
  serial_base64 = nsnull;
  cert = nsnull;
}

nsMyTrustedEVInfoClass::~nsMyTrustedEVInfoClass()
{
  free(dotted_oid);
  free(oid_name);
  free(ev_root_sha1_fingerprint);
  free(issuer_base64);
  free(serial_base64);
  if (cert)
    CERT_DestroyCertificate(cert);
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

  NS_ConvertASCIItoUTF16 info_sha1(info.ev_root_sha1_fingerprint);

  nsNSSCertificate c(rootCert);

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
   *   1_fingerprint (in format XX:XX:XX:...)
   *   2_readable_oid (treated as a comment)
   * the input file must strictly follow this order
   * the input file may contain 0, 1 or many records
   * completely empty lines are ignored
   * lines that start with the # char are ignored
   */

  int line_counter = 0;
  PRBool found_error = PR_FALSE;

  enum { 
    pos_fingerprint, pos_readable_oid, pos_issuer, pos_serial
  } reader_position = pos_fingerprint;

  nsCString fingerprint, readable_oid, issuer, serial;

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

    if (reader_position == pos_fingerprint &&
        descriptor.EqualsLiteral(("1_fingerprint"))) {
      fingerprint = data;
      reader_position = pos_readable_oid;
      continue;
    }
    else if (reader_position == pos_readable_oid &&
        descriptor.EqualsLiteral(("2_readable_oid"))) {
      readable_oid = data;
      reader_position = pos_issuer;
    }
    else if (reader_position == pos_issuer &&
        descriptor.EqualsLiteral(("3_issuer"))) {
      issuer = data;
      reader_position = pos_serial;
    }
    else if (reader_position == pos_readable_oid &&
        descriptor.EqualsLiteral(("4_serial"))) {
      serial = data;
      reader_position = pos_fingerprint;
    }
    else {
      found_error = PR_TRUE;
      break;
    }

    nsMyTrustedEVInfoClass *temp_ev = new nsMyTrustedEVInfoClass;
    if (!temp_ev)
      return;

    temp_ev->ev_root_sha1_fingerprint = strdup(fingerprint.get());
    temp_ev->oid_name = strdup(readable_oid.get());
    temp_ev->dotted_oid = strdup(readable_oid.get());
    temp_ev->issuer_base64 = strdup(issuer.get());
    temp_ev->serial_base64 = strdup(serial.get());

    SECStatus rv;
    CERTIssuerAndSN ias;

    rv = ATOB_ConvertAsciiToItem(&ias.derIssuer, const_cast<char*>(temp_ev->issuer_base64));
    NS_ASSERTION(rv==SECSuccess, "error converting ascii to binary.");
    rv = ATOB_ConvertAsciiToItem(&ias.serialNumber, const_cast<char*>(temp_ev->serial_base64));
    NS_ASSERTION(rv==SECSuccess, "error converting ascii to binary.");

    temp_ev->cert = CERT_FindCertByIssuerAndSN(nsnull, &ias);
    NS_ASSERTION(temp_ev->cert, "Could not find EV root in NSS storage");

    if (!temp_ev->cert)
      return;

    nsNSSCertificate c(temp_ev->cert);
    nsAutoString fingerprint;
    c.GetSha1Fingerprint(fingerprint);

    NS_ConvertASCIItoUTF16 sha1(temp_ev->ev_root_sha1_fingerprint);

    if (sha1 != fingerprint) {
      NS_ASSERTION(sha1 == fingerprint, "found EV root with unexpected SHA1 mismatch");
      CERT_DestroyCertificate(temp_ev->cert);
      temp_ev->cert = nsnull;
      return;
    }

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
getRootsForOidFromExternalRootsFile(CERTCertList* certList, 
                                    SECOidTag policyOIDTag)
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
      CERT_AddCertToListTail(certList, ev->cert);
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

static CERTCertList*
getRootsForOid(SECOidTag oid_tag)
{
  CERTCertList *certList = CERT_NewCertList();
  if (!certList)
    return nsnull;

  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (entry.oid_tag == oid_tag)
      CERT_AddCertToListTail(certList, entry.cert);
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  getRootsForOidFromExternalRootsFile(certList, oid_tag);
#endif
  return certList;
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

    SECStatus rv;
    CERTIssuerAndSN ias;

    rv = ATOB_ConvertAsciiToItem(&ias.derIssuer, const_cast<char*>(entry.issuer_base64));
    NS_ASSERTION(rv==SECSuccess, "error converting ascii to binary.");
    rv = ATOB_ConvertAsciiToItem(&ias.serialNumber, const_cast<char*>(entry.serial_base64));
    NS_ASSERTION(rv==SECSuccess, "error converting ascii to binary.");

    entry.cert = CERT_FindCertByIssuerAndSN(nsnull, &ias);
    NS_ASSERTION(entry.cert, "Could not find EV root in NSS storage");

    if (!entry.cert)
      continue;

    nsNSSCertificate c(entry.cert);
    nsAutoString fingerprint;
    c.GetSha1Fingerprint(fingerprint);

    NS_ConvertASCIItoUTF16 sha1(entry.ev_root_sha1_fingerprint);

    if (sha1 != fingerprint) {
      NS_ASSERTION(sha1 == fingerprint, "found EV root with unexpected SHA1 mismatch");
      CERT_DestroyCertificate(entry.cert);
      entry.cert = nsnull;
      continue;
    }

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

PRBool
nsNSSSocketInfo::hasCertErrors()
{
  if (!mSSLStatus) {
    // if the status is unknown, assume the cert is bad, better safe than sorry
    return PR_TRUE;
  }

  return mSSLStatus->mHaveCertErrorBits;
}

NS_IMETHODIMP
nsNSSSocketInfo::GetIsExtendedValidation(PRBool* aIsEV)
{
  NS_ENSURE_ARG(aIsEV);
  *aIsEV = PR_FALSE;

  if (!mCert)
    return NS_OK;

  // Never allow bad certs for EV, regardless of overrides.
  if (hasCertErrors())
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(mCert, &rv);
  if (NS_FAILED(rv))
    return rv;

  return idinfo->GetIsExtendedValidation(aIsEV);
}

NS_IMETHODIMP
nsNSSSocketInfo::GetValidEVPolicyOid(nsACString &outDottedOid)
{
  if (!mCert)
    return NS_OK;

  if (hasCertErrors())
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(mCert, &rv);
  if (NS_FAILED(rv))
    return rv;

  return idinfo->GetValidEVPolicyOid(outDottedOid);
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

  PRBool isOCSPEnabled = PR_FALSE;
  nsCOMPtr<nsIX509CertDB> certdb;
  certdb = do_GetService(NS_X509CERTDB_CONTRACTID);
  if (certdb)
    certdb->GetIsOcspOn(&isOCSPEnabled);
  // No OCSP, no EV
  if (!isOCSPEnabled)
    return NS_OK;

  SECOidTag oid_tag;
  SECStatus rv = getFirstEVPolicy(mCert, oid_tag);
  if (rv != SECSuccess)
    return NS_OK;

  if (oid_tag == SEC_OID_UNKNOWN) // not in our list of OIDs accepted for EV
    return NS_OK;

  CERTCertList *rootList = getRootsForOid(oid_tag);
  CERTCertListCleaner rootListCleaner();

  CERTRevocationMethodIndex preferedRevMethods[1] = { 
    cert_revocation_method_ocsp
  };

  PRUint64 revMethodFlags = 
    CERT_REV_M_TEST_USING_THIS_METHOD
    | CERT_REV_M_ALLOW_NETWORK_FETCHING
    | CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE
    | CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE
    | CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO
    | CERT_REV_M_STOP_TESTING_ON_FRESH_INFO;

  PRUint64 revMethodIndependentFlags = 
    CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST
    | CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE;

  PRUint64 methodFlags[2];
  methodFlags[cert_revocation_method_crl] = revMethodFlags;
  methodFlags[cert_revocation_method_ocsp] = revMethodFlags;

  CERTRevocationFlags rev;

  rev.leafTests.number_of_defined_methods = cert_revocation_method_ocsp +1;
  rev.leafTests.cert_rev_flags_per_method = methodFlags;
  rev.leafTests.number_of_preferred_methods = 1;
  rev.leafTests.preferred_methods = preferedRevMethods;
  rev.leafTests.cert_rev_method_independent_flags =
    revMethodIndependentFlags;

  rev.chainTests.number_of_defined_methods = cert_revocation_method_ocsp +1;
  rev.leafTests.cert_rev_flags_per_method = methodFlags;
  rev.chainTests.number_of_preferred_methods = 1;
  rev.chainTests.preferred_methods = preferedRevMethods;
  rev.chainTests.cert_rev_method_independent_flags =
    revMethodIndependentFlags;

  CERTValInParam cvin[3];
  cvin[0].type = cert_pi_policyOID;
  cvin[0].value.arraySize = 1; 
  cvin[0].value.array.oids = &oid_tag;

  cvin[1].type = cert_pi_revocationFlags;
  cvin[1].value.pointer.revocation = &rev;

  cvin[2].type = cert_pi_trustAnchors;
  cvin[2].value.pointer.chain = rootList;

  cvin[3].type = cert_pi_end;

  CERTValOutParam cvout[2];
  cvout[0].type = cert_po_trustAnchor;
  cvout[1].type = cert_po_end;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("calling CERT_PKIXVerifyCert nss cert %p\n", mCert));
  rv = CERT_PKIXVerifyCert(mCert, certificateUsageSSLServer,
                           cvin, cvout, nsnull);
  if (rv != SECSuccess)
    return NS_OK;

  CERTCertificate *issuerCert = cvout[0].value.pointer.cert;
  CERTCertificateCleaner issuerCleaner(issuerCert);

#ifdef PR_LOGGING
  if (PR_LOG_TEST(gPIPNSSLog, PR_LOG_DEBUG)) {
    nsNSSCertificate ic(issuerCert);
    nsAutoString fingerprint;
    ic.GetSha1Fingerprint(fingerprint);
    NS_LossyConvertUTF16toASCII fpa(fingerprint);
    PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("CERT_PKIXVerifyCert returned success, issuer: %s, SHA1: %s\n", 
      issuerCert->subjectName, fpa.get()));
  }
#endif

  validEV = isApprovedForEV(oid_tag, issuerCert);
  if (validEV)
    resultOidTag = oid_tag;
 
  return NS_OK;
}

nsresult
nsNSSCertificate::getValidEVOidTag(SECOidTag &resultOidTag, PRBool &validEV)
{
  if (mCachedEVStatus != ev_status_unknown) {
    validEV = (mCachedEVStatus == ev_status_valid);
    if (validEV)
      resultOidTag = mCachedEVOidTag;
    return NS_OK;
  }

  nsresult rv = hasValidEVOidTag(resultOidTag, validEV);
  if (NS_SUCCEEDED(rv)) {
    if (validEV) {
      mCachedEVOidTag = resultOidTag;
    }
    mCachedEVStatus = validEV ? ev_status_valid : ev_status_invalid;
  }
  return rv;
}

NS_IMETHODIMP
nsNSSCertificate::GetIsExtendedValidation(PRBool* aIsEV)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aIsEV);
  *aIsEV = PR_FALSE;

  if (mCachedEVStatus != ev_status_unknown) {
    *aIsEV = (mCachedEVStatus == ev_status_valid);
    return NS_OK;
  }

  SECOidTag oid_tag;
  return getValidEVOidTag(oid_tag, *aIsEV);
}

NS_IMETHODIMP
nsNSSCertificate::GetValidEVPolicyOid(nsACString &outDottedOid)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  SECOidTag oid_tag;
  PRBool valid;
  nsresult rv = getValidEVOidTag(oid_tag, valid);
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
  nsNSSShutDownPreventionLock locker;
  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (entry.cert) {
      CERT_DestroyCertificate(entry.cert);
      entry.cert = nsnull;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (testEVInfosLoaded) {
    testEVInfosLoaded = PR_FALSE;
    if (testEVInfos) {
      for (size_t i = 0; i<testEVInfos->Length(); ++i) {
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppDirectoryServiceDefs.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "nsPromiseFlatString.h"
#include "nsTArray.h"

#include "cert.h"
#include "base64.h"
#include "nsNSSComponent.h"
#include "nsSSLStatus.h"
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
NSSCleanupAutoPtrClass_WithParam(SECItem, SECITEM_FreeItem, TrueParam, true)

#define CONST_OID static const unsigned char
#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }

struct nsMyTrustedEVInfo
{
  const char *dotted_oid;
  const char *oid_name; // Set this to null to signal an invalid structure,
                  // (We can't have an empty list, so we'll use a dummy entry)
  SECOidTag oid_tag;
  const char *ev_root_sha1_fingerprint;
  const char *issuer_base64;
  const char *serial_base64;
  CERTCertificate *cert;
};

static struct nsMyTrustedEVInfo myTrustedEVInfos[] = {
  /*
   * IMPORTANT! When extending this list, 
   * pairs of dotted_oid and oid_name should always be unique pairs.
   * In other words, if you add another list, that uses the same dotted_oid
   * as an existing entry, then please use the same oid_name.
   */
  {
    // CN=WellsSecure Public Root Certificate Authority,OU=Wells Fargo Bank NA,O=Wells Fargo WellsSecure,C=US
    "2.16.840.1.114171.500.9",
    "WellsSecure EV OID",
    SEC_OID_UNKNOWN,
    "E7:B4:F6:9D:61:EC:90:69:DB:7E:90:A7:40:1A:3C:F4:7D:4F:E8:EE",
    "MIGFMQswCQYDVQQGEwJVUzEgMB4GA1UECgwXV2VsbHMgRmFyZ28gV2VsbHNTZWN1"
    "cmUxHDAaBgNVBAsME1dlbGxzIEZhcmdvIEJhbmsgTkExNjA0BgNVBAMMLVdlbGxz"
    "U2VjdXJlIFB1YmxpYyBSb290IENlcnRpZmljYXRlIEF1dGhvcml0eQ==",
    "AQ==",
    nullptr
  },
  {
    // OU=Security Communication EV RootCA1,O="SECOM Trust Systems CO.,LTD.",C=JP
    "1.2.392.200091.100.721.1",
    "SECOM EV OID",
    SEC_OID_UNKNOWN,
    "FE:B8:C4:32:DC:F9:76:9A:CE:AE:3D:D8:90:8F:FD:28:86:65:64:7D",
    "MGAxCzAJBgNVBAYTAkpQMSUwIwYDVQQKExxTRUNPTSBUcnVzdCBTeXN0ZW1zIENP"
    "LixMVEQuMSowKAYDVQQLEyFTZWN1cml0eSBDb21tdW5pY2F0aW9uIEVWIFJvb3RD"
    "QTE=",
    "AA==",
    nullptr
  },
  {
    // CN=Cybertrust Global Root,O=Cybertrust, Inc
    "1.3.6.1.4.1.6334.1.100.1",
    "Cybertrust EV OID",
    SEC_OID_UNKNOWN,
    "5F:43:E5:B1:BF:F8:78:8C:AC:1C:C7:CA:4A:9A:C6:22:2B:CC:34:C6",
    "MDsxGDAWBgNVBAoTD0N5YmVydHJ1c3QsIEluYzEfMB0GA1UEAxMWQ3liZXJ0cnVz"
    "dCBHbG9iYWwgUm9vdA==",
    "BAAAAAABD4WqLUg=",
    nullptr
  },
  {
    // CN=SwissSign Gold CA - G2,O=SwissSign AG,C=CH
    "2.16.756.1.89.1.2.1.1",
    "SwissSign EV OID",
    SEC_OID_UNKNOWN,
    "D8:C5:38:8A:B7:30:1B:1B:6E:D4:7A:E6:45:25:3A:6F:9F:1A:27:61",
    "MEUxCzAJBgNVBAYTAkNIMRUwEwYDVQQKEwxTd2lzc1NpZ24gQUcxHzAdBgNVBAMT"
    "FlN3aXNzU2lnbiBHb2xkIENBIC0gRzI=",
    "ALtAHEP1Xk+w",
    nullptr
  },
  {
    // CN=StartCom Certification Authority,OU=Secure Digital Certificate Signing,O=StartCom Ltd.,C=IL
    "1.3.6.1.4.1.23223.2",
    "StartCom EV OID",
    SEC_OID_UNKNOWN,
    "3E:2B:F7:F2:03:1B:96:F3:8C:E6:C4:D8:A8:5D:3E:2D:58:47:6A:0F",
    "MH0xCzAJBgNVBAYTAklMMRYwFAYDVQQKEw1TdGFydENvbSBMdGQuMSswKQYDVQQL"
    "EyJTZWN1cmUgRGlnaXRhbCBDZXJ0aWZpY2F0ZSBTaWduaW5nMSkwJwYDVQQDEyBT"
    "dGFydENvbSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "AQ==",
    nullptr
  },
  {
    // CN=VeriSign Class 3 Public Primary Certification Authority - G5,OU="(c) 2006 VeriSign, Inc. - For authorized use only",OU=VeriSign Trust Network,O="VeriSign, Inc.",C=US
    "2.16.840.1.113733.1.7.23.6",
    "VeriSign EV OID",
    SEC_OID_UNKNOWN,
    "4E:B6:D5:78:49:9B:1C:CF:5F:58:1E:AD:56:BE:3D:9B:67:44:A5:E5",
    "MIHKMQswCQYDVQQGEwJVUzEXMBUGA1UEChMOVmVyaVNpZ24sIEluYy4xHzAdBgNV"
    "BAsTFlZlcmlTaWduIFRydXN0IE5ldHdvcmsxOjA4BgNVBAsTMShjKSAyMDA2IFZl"
    "cmlTaWduLCBJbmMuIC0gRm9yIGF1dGhvcml6ZWQgdXNlIG9ubHkxRTBDBgNVBAMT"
    "PFZlcmlTaWduIENsYXNzIDMgUHVibGljIFByaW1hcnkgQ2VydGlmaWNhdGlvbiBB"
    "dXRob3JpdHkgLSBHNQ==",
    "GNrRniZ96LtKIVjNzGs7Sg==",
    nullptr
  },
  {
    // CN=GeoTrust Primary Certification Authority,O=GeoTrust Inc.,C=US
    "1.3.6.1.4.1.14370.1.6",
    "GeoTrust EV OID",
    SEC_OID_UNKNOWN,
    "32:3C:11:8E:1B:F7:B8:B6:52:54:E2:E2:10:0D:D6:02:90:37:F0:96",
    "MFgxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMTEwLwYDVQQD"
    "EyhHZW9UcnVzdCBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9yaXR5",
    "GKy1av1pthU6Y2yv2vrEoQ==",
    nullptr
  },
  {
    // CN=thawte Primary Root CA,OU="(c) 2006 thawte, Inc. - For authorized use only",OU=Certification Services Division,O="thawte, Inc.",C=US
    "2.16.840.1.113733.1.7.48.1",
    "Thawte EV OID",
    SEC_OID_UNKNOWN,
    "91:C6:D6:EE:3E:8A:C8:63:84:E5:48:C2:99:29:5C:75:6C:81:7B:81",
    "MIGpMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMdGhhd3RlLCBJbmMuMSgwJgYDVQQL"
    "Ex9DZXJ0aWZpY2F0aW9uIFNlcnZpY2VzIERpdmlzaW9uMTgwNgYDVQQLEy8oYykg"
    "MjAwNiB0aGF3dGUsIEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25seTEfMB0G"
    "A1UEAxMWdGhhd3RlIFByaW1hcnkgUm9vdCBDQQ==",
    "NE7VVyDV7exJ9C/ON9srbQ==",
    nullptr
  },
  {
    // CN=XRamp Global Certification Authority,O=XRamp Security Services Inc,OU=www.xrampsecurity.com,C=US
    "2.16.840.1.114404.1.1.2.4.1",
    "Trustwave EV OID",
    SEC_OID_UNKNOWN,
    "B8:01:86:D1:EB:9C:86:A5:41:04:CF:30:54:F3:4C:52:B7:E5:58:C6",
    "MIGCMQswCQYDVQQGEwJVUzEeMBwGA1UECxMVd3d3LnhyYW1wc2VjdXJpdHkuY29t"
    "MSQwIgYDVQQKExtYUmFtcCBTZWN1cml0eSBTZXJ2aWNlcyBJbmMxLTArBgNVBAMT"
    "JFhSYW1wIEdsb2JhbCBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "UJRs7Bjq1ZxN1ZfvdY+grQ==",
    nullptr
  },
  {
    // CN=SecureTrust CA,O=SecureTrust Corporation,C=US
    "2.16.840.1.114404.1.1.2.4.1",
    "Trustwave EV OID",
    SEC_OID_UNKNOWN,
    "87:82:C6:C3:04:35:3B:CF:D2:96:92:D2:59:3E:7D:44:D9:34:FF:11",
    "MEgxCzAJBgNVBAYTAlVTMSAwHgYDVQQKExdTZWN1cmVUcnVzdCBDb3Jwb3JhdGlv"
    "bjEXMBUGA1UEAxMOU2VjdXJlVHJ1c3QgQ0E=",
    "DPCOXAgWpa1Cf/DrJxhZ0A==",
    nullptr
  },
  {
    // CN=Secure Global CA,O=SecureTrust Corporation,C=US
    "2.16.840.1.114404.1.1.2.4.1",
    "Trustwave EV OID",
    SEC_OID_UNKNOWN,
    "3A:44:73:5A:E5:81:90:1F:24:86:61:46:1E:3B:9C:C4:5F:F5:3A:1B",
    "MEoxCzAJBgNVBAYTAlVTMSAwHgYDVQQKExdTZWN1cmVUcnVzdCBDb3Jwb3JhdGlv"
    "bjEZMBcGA1UEAxMQU2VjdXJlIEdsb2JhbCBDQQ==",
    "B1YipOjUiolN9BPI8PjqpQ==",
    nullptr
  },
  {
    // CN=COMODO ECC Certification Authority,O=COMODO CA Limited,L=Salford,ST=Greater Manchester,C=GB
    "1.3.6.1.4.1.6449.1.2.1.5.1",
    "Comodo EV OID",
    SEC_OID_UNKNOWN,
    "9F:74:4E:9F:2B:4D:BA:EC:0F:31:2C:50:B6:56:3B:8E:2D:93:C3:11",
    "MIGFMQswCQYDVQQGEwJHQjEbMBkGA1UECBMSR3JlYXRlciBNYW5jaGVzdGVyMRAw"
    "DgYDVQQHEwdTYWxmb3JkMRowGAYDVQQKExFDT01PRE8gQ0EgTGltaXRlZDErMCkG"
    "A1UEAxMiQ09NT0RPIEVDQyBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "H0evqmIAcFBUTAGem2OZKg==",
    nullptr
  },
  {
    // CN=COMODO Certification Authority,O=COMODO CA Limited,L=Salford,ST=Greater Manchester,C=GB
    "1.3.6.1.4.1.6449.1.2.1.5.1",
    "Comodo EV OID",
    SEC_OID_UNKNOWN,
    "66:31:BF:9E:F7:4F:9E:B6:C9:D5:A6:0C:BA:6A:BE:D1:F7:BD:EF:7B",
    "MIGBMQswCQYDVQQGEwJHQjEbMBkGA1UECBMSR3JlYXRlciBNYW5jaGVzdGVyMRAw"
    "DgYDVQQHEwdTYWxmb3JkMRowGAYDVQQKExFDT01PRE8gQ0EgTGltaXRlZDEnMCUG"
    "A1UEAxMeQ09NT0RPIENlcnRpZmljYXRpb24gQXV0aG9yaXR5",
    "ToEtioJl4AsC7j41AkblPQ==",
    nullptr
  },
  {
    // CN=AddTrust External CA Root,OU=AddTrust External TTP Network,O=AddTrust AB,C=SE
    "1.3.6.1.4.1.6449.1.2.1.5.1",
    "Comodo EV OID",
    SEC_OID_UNKNOWN,
    "02:FA:F3:E2:91:43:54:68:60:78:57:69:4D:F5:E4:5B:68:85:18:68",
    "MG8xCzAJBgNVBAYTAlNFMRQwEgYDVQQKEwtBZGRUcnVzdCBBQjEmMCQGA1UECxMd"
    "QWRkVHJ1c3QgRXh0ZXJuYWwgVFRQIE5ldHdvcmsxIjAgBgNVBAMTGUFkZFRydXN0"
    "IEV4dGVybmFsIENBIFJvb3Q=",
    "AQ==",
    nullptr
  },
  {
    // CN=UTN - DATACorp SGC,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US
    "1.3.6.1.4.1.6449.1.2.1.5.1",
    "Comodo EV OID",
    SEC_OID_UNKNOWN,
    "58:11:9F:0E:12:82:87:EA:50:FD:D9:87:45:6F:4F:78:DC:FA:D6:D4",
    "MIGTMQswCQYDVQQGEwJVUzELMAkGA1UECBMCVVQxFzAVBgNVBAcTDlNhbHQgTGFr"
    "ZSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxITAfBgNVBAsT"
    "GGh0dHA6Ly93d3cudXNlcnRydXN0LmNvbTEbMBkGA1UEAxMSVVROIC0gREFUQUNv"
    "cnAgU0dD",
    "RL4Mi1AAIbQR0ypoBqmtaQ==",
    nullptr
  },
  {
    // CN=UTN-USERFirst-Hardware,OU=http://www.usertrust.com,O=The USERTRUST Network,L=Salt Lake City,ST=UT,C=US
    "1.3.6.1.4.1.6449.1.2.1.5.1",
    "Comodo EV OID",
    SEC_OID_UNKNOWN,
    "04:83:ED:33:99:AC:36:08:05:87:22:ED:BC:5E:46:00:E3:BE:F9:D7",
    "MIGXMQswCQYDVQQGEwJVUzELMAkGA1UECBMCVVQxFzAVBgNVBAcTDlNhbHQgTGFr"
    "ZSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxITAfBgNVBAsT"
    "GGh0dHA6Ly93d3cudXNlcnRydXN0LmNvbTEfMB0GA1UEAxMWVVROLVVTRVJGaXJz"
    "dC1IYXJkd2FyZQ==",
    "RL4Mi1AAJLQR0zYq/mUK/Q==",
    nullptr
  },
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
    nullptr
  },
  {
    // CN=Go Daddy Root Certificate Authority - G2,O="GoDaddy.com, Inc.",L=Scottsdale,ST=Arizona,C=US
    "2.16.840.1.114413.1.7.23.3",
    "Go Daddy EV OID a",
    SEC_OID_UNKNOWN,
    "47:BE:AB:C9:22:EA:E8:0E:78:78:34:62:A7:9F:45:C2:54:FD:E6:8B",
    "MIGDMQswCQYDVQQGEwJVUzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2Nv"
    "dHRzZGFsZTEaMBgGA1UEChMRR29EYWRkeS5jb20sIEluYy4xMTAvBgNVBAMTKEdv"
    "IERhZGR5IFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzI=",
    "AA==",
    nullptr
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
    nullptr
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
    nullptr
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
    nullptr
  },
  {
    // CN=Starfield Root Certificate Authority - G2,O="Starfield Technologies, Inc.",L=Scottsdale,ST=Arizona,C=US
    "2.16.840.1.114414.1.7.23.3",
    "Go Daddy EV OID b",
    SEC_OID_UNKNOWN,
    "B5:1C:06:7C:EE:2B:0C:3D:F8:55:AB:2D:92:F4:FE:39:D4:E7:0F:0E",
    "MIGPMQswCQYDVQQGEwJVUzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2Nv"
    "dHRzZGFsZTElMCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEy"
    "MDAGA1UEAxMpU3RhcmZpZWxkIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0g"
    "RzI=",
    "AA==",
    nullptr
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
    nullptr
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
    nullptr
  },
  {
    // CN=Network Solutions Certificate Authority,O=Network Solutions L.L.C.,C=US
    "1.3.6.1.4.1.782.1.2.1.8.1",
    "Network Solutions EV OID",
    SEC_OID_UNKNOWN,
    "74:F8:A3:C3:EF:E7:B3:90:06:4B:83:90:3C:21:64:60:20:E5:DF:CE",
    "MGIxCzAJBgNVBAYTAlVTMSEwHwYDVQQKExhOZXR3b3JrIFNvbHV0aW9ucyBMLkwu"
    "Qy4xMDAuBgNVBAMTJ05ldHdvcmsgU29sdXRpb25zIENlcnRpZmljYXRlIEF1dGhv"
    "cml0eQ==",
    "V8szb8JcFuZHFhfjkDFo4A==",
    nullptr
  },
  {
    // CN=Entrust Root Certification Authority,OU="(c) 2006 Entrust, Inc.",OU=www.entrust.net/CPS is incorporated by reference,O="Entrust, Inc.",C=US
    "2.16.840.1.114028.10.1.2",
    "Entrust EV OID",
    SEC_OID_UNKNOWN,
    "B3:1E:B1:B7:40:E3:6C:84:02:DA:DC:37:D4:4D:F5:D4:67:49:52:F9",
    "MIGwMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNRW50cnVzdCwgSW5jLjE5MDcGA1UE"
    "CxMwd3d3LmVudHJ1c3QubmV0L0NQUyBpcyBpbmNvcnBvcmF0ZWQgYnkgcmVmZXJl"
    "bmNlMR8wHQYDVQQLExYoYykgMjAwNiBFbnRydXN0LCBJbmMuMS0wKwYDVQQDEyRF"
    "bnRydXN0IFJvb3QgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHk=",
    "RWtQVA==",
    nullptr
  },
  {
    // CN=GlobalSign Root CA,OU=Root CA,O=GlobalSign nv-sa,C=BE
    "1.3.6.1.4.1.4146.1.1",
    "GlobalSign EV OID",
    SEC_OID_UNKNOWN,
    "B1:BC:96:8B:D4:F4:9D:62:2A:A8:9A:81:F2:15:01:52:A4:1D:82:9C",
    "MFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMRAwDgYD"
    "VQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxTaWduIFJvb3QgQ0E=",
    "BAAAAAABFUtaw5Q=",
    nullptr
  },
  {
    // CN=GlobalSign,O=GlobalSign,OU=GlobalSign Root CA - R2
    "1.3.6.1.4.1.4146.1.1",
    "GlobalSign EV OID",
    SEC_OID_UNKNOWN,
    "75:E0:AB:B6:13:85:12:27:1C:04:F8:5F:DD:DE:38:E4:B7:24:2E:FE",
    "MEwxIDAeBgNVBAsTF0dsb2JhbFNpZ24gUm9vdCBDQSAtIFIyMRMwEQYDVQQKEwpH"
    "bG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWdu",
    "BAAAAAABD4Ym5g0=",
    nullptr
  },
  {
    // CN=GlobalSign,O=GlobalSign,OU=GlobalSign Root CA - R3
    "1.3.6.1.4.1.4146.1.1",
    "GlobalSign EV OID",
    SEC_OID_UNKNOWN,
    "D6:9B:56:11:48:F0:1C:77:C5:45:78:C1:09:26:DF:5B:85:69:76:AD",
    "MEwxIDAeBgNVBAsTF0dsb2JhbFNpZ24gUm9vdCBDQSAtIFIzMRMwEQYDVQQKEwpH"
    "bG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWdu",
    "BAAAAAABIVhTCKI=",
    nullptr
  },
  {
    // CN=Buypass Class 3 CA 1,O=Buypass AS-983163327,C=NO
    "2.16.578.1.26.1.3.3",
    "Buypass Class 3 CA 1",
    SEC_OID_UNKNOWN,
    "61:57:3A:11:DF:0E:D8:7E:D5:92:65:22:EA:D0:56:D7:44:B3:23:71",
    "MEsxCzAJBgNVBAYTAk5PMR0wGwYDVQQKDBRCdXlwYXNzIEFTLTk4MzE2MzMyNzEd"
    "MBsGA1UEAwwUQnV5cGFzcyBDbGFzcyAzIENBIDE=",
    "Ag==",
    nullptr
  },
  {
    // CN=Class 2 Primary CA,O=Certplus,C=FR
    "1.3.6.1.4.1.22234.2.5.2.3.1",
    "Certplus EV OID",
    SEC_OID_UNKNOWN,
    "74:20:74:41:72:9C:DD:92:EC:79:31:D8:23:10:8D:C2:81:92:E2:BB",
    "MD0xCzAJBgNVBAYTAkZSMREwDwYDVQQKEwhDZXJ0cGx1czEbMBkGA1UEAxMSQ2xh"
    "c3MgMiBQcmltYXJ5IENB",
    "AIW9S/PY2uNp9pTXX8OlRCM=",
    nullptr
  },
  {
    // CN=Chambers of Commerce Root - 2008,O=AC Camerfirma S.A.,serialNumber=A82743287,L=Madrid (see current address at www.camerfirma.com/address),C=EU
    "1.3.6.1.4.1.17326.10.14.2.1.2",
    "Camerfirma EV OID a",
    SEC_OID_UNKNOWN,
    "78:6A:74:AC:76:AB:14:7F:9C:6A:30:50:BA:9E:A8:7E:FE:9A:CE:3C",
    "MIGuMQswCQYDVQQGEwJFVTFDMEEGA1UEBxM6TWFkcmlkIChzZWUgY3VycmVudCBh"
    "ZGRyZXNzIGF0IHd3dy5jYW1lcmZpcm1hLmNvbS9hZGRyZXNzKTESMBAGA1UEBRMJ"
    "QTgyNzQzMjg3MRswGQYDVQQKExJBQyBDYW1lcmZpcm1hIFMuQS4xKTAnBgNVBAMT"
    "IENoYW1iZXJzIG9mIENvbW1lcmNlIFJvb3QgLSAyMDA4",
    "AKPaQn6ksa7a",
    nullptr
  },
  {
    // CN=Global Chambersign Root - 2008,O=AC Camerfirma S.A.,serialNumber=A82743287,L=Madrid (see current address at www.camerfirma.com/address),C=EU
    "1.3.6.1.4.1.17326.10.8.12.1.2",
    "Camerfirma EV OID b",
    SEC_OID_UNKNOWN,
    "4A:BD:EE:EC:95:0D:35:9C:89:AE:C7:52:A1:2C:5B:29:F6:D6:AA:0C",
    "MIGsMQswCQYDVQQGEwJFVTFDMEEGA1UEBxM6TWFkcmlkIChzZWUgY3VycmVudCBh"
    "ZGRyZXNzIGF0IHd3dy5jYW1lcmZpcm1hLmNvbS9hZGRyZXNzKTESMBAGA1UEBRMJ"
    "QTgyNzQzMjg3MRswGQYDVQQKExJBQyBDYW1lcmZpcm1hIFMuQS4xJzAlBgNVBAMT"
    "Hkdsb2JhbCBDaGFtYmVyc2lnbiBSb290IC0gMjAwOA==",
    "AMnN0+nVfSPO",
    nullptr
  },
  {
    // CN=TC TrustCenter Universal CA III,OU=TC TrustCenter Universal CA,O=TC TrustCenter GmbH,C=DE
    "1.2.276.0.44.1.1.1.4",
    "TC TrustCenter EV OID",
    SEC_OID_UNKNOWN,
    "96:56:CD:7B:57:96:98:95:D0:E1:41:46:68:06:FB:B8:C6:11:06:87",
    "MHsxCzAJBgNVBAYTAkRFMRwwGgYDVQQKExNUQyBUcnVzdENlbnRlciBHbWJIMSQw"
    "IgYDVQQLExtUQyBUcnVzdENlbnRlciBVbml2ZXJzYWwgQ0ExKDAmBgNVBAMTH1RD"
    "IFRydXN0Q2VudGVyIFVuaXZlcnNhbCBDQSBJSUk=",
    "YyUAAQACFI0zFQLkbPQ=",
    nullptr
  },
  {
    // CN=AffirmTrust Commercial,O=AffirmTrust,C=US
    "1.3.6.1.4.1.34697.2.1",
    "AffirmTrust EV OID a",
    SEC_OID_UNKNOWN,
    "F9:B5:B6:32:45:5F:9C:BE:EC:57:5F:80:DC:E9:6E:2C:C7:B2:78:B7",
    "MEQxCzAJBgNVBAYTAlVTMRQwEgYDVQQKDAtBZmZpcm1UcnVzdDEfMB0GA1UEAwwW"
    "QWZmaXJtVHJ1c3QgQ29tbWVyY2lhbA==",
    "d3cGJyapsXw=",
    nullptr
  },
  {
    // CN=AffirmTrust Networking,O=AffirmTrust,C=US
    "1.3.6.1.4.1.34697.2.2",
    "AffirmTrust EV OID b",
    SEC_OID_UNKNOWN,
    "29:36:21:02:8B:20:ED:02:F5:66:C5:32:D1:D6:ED:90:9F:45:00:2F",
    "MEQxCzAJBgNVBAYTAlVTMRQwEgYDVQQKDAtBZmZpcm1UcnVzdDEfMB0GA1UEAwwW"
    "QWZmaXJtVHJ1c3QgTmV0d29ya2luZw==",
    "fE8EORzUmS0=",
    nullptr
  },
  {
    // CN=AffirmTrust Premium,O=AffirmTrust,C=US
    "1.3.6.1.4.1.34697.2.3",
    "AffirmTrust EV OID c",
    SEC_OID_UNKNOWN,
    "D8:A6:33:2C:E0:03:6F:B1:85:F6:63:4F:7D:6A:06:65:26:32:28:27",
    "MEExCzAJBgNVBAYTAlVTMRQwEgYDVQQKDAtBZmZpcm1UcnVzdDEcMBoGA1UEAwwT"
    "QWZmaXJtVHJ1c3QgUHJlbWl1bQ==",
    "bYwURrGmCu4=",
    nullptr
  },
  {
    // CN=AffirmTrust Premium ECC,O=AffirmTrust,C=US
    "1.3.6.1.4.1.34697.2.4",
    "AffirmTrust EV OID d",
    SEC_OID_UNKNOWN,
    "B8:23:6B:00:2F:1D:16:86:53:01:55:6C:11:A4:37:CA:EB:FF:C3:BB",
    "MEUxCzAJBgNVBAYTAlVTMRQwEgYDVQQKDAtBZmZpcm1UcnVzdDEgMB4GA1UEAwwX"
    "QWZmaXJtVHJ1c3QgUHJlbWl1bSBFQ0M=",
    "dJclisc/elQ=",
    nullptr
  },
  {
    // CN=Certum Trusted Network CA,OU=Certum Certification Authority,O=Unizeto Technologies S.A.,C=PL
    "1.2.616.1.113527.2.5.1.1",
    "Certum EV OID",
    SEC_OID_UNKNOWN,
    "07:E0:32:E0:20:B7:2C:3F:19:2F:06:28:A2:59:3A:19:A7:0F:06:9E",
    "MH4xCzAJBgNVBAYTAlBMMSIwIAYDVQQKExlVbml6ZXRvIFRlY2hub2xvZ2llcyBT"
    "LkEuMScwJQYDVQQLEx5DZXJ0dW0gQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkxIjAg"
    "BgNVBAMTGUNlcnR1bSBUcnVzdGVkIE5ldHdvcmsgQ0E=",
    "BETA",
    nullptr
  },
  {
    // CN=Izenpe.com,O=IZENPE S.A.,C=ES
    "1.3.6.1.4.1.14777.6.1.1",
    "Izenpe EV OID 1",
    SEC_OID_UNKNOWN,
    "2F:78:3D:25:52:18:A7:4A:65:39:71:B5:2C:A2:9C:45:15:6F:E9:19",
    "MDgxCzAJBgNVBAYTAkVTMRQwEgYDVQQKDAtJWkVOUEUgUy5BLjETMBEGA1UEAwwK"
    "SXplbnBlLmNvbQ==",
    "ALC3WhZIX7/hy/WL1xnmfQ==",
    nullptr
  },
  {
    // CN=Izenpe.com,O=IZENPE S.A.,C=ES
    "1.3.6.1.4.1.14777.6.1.2",
    "Izenpe EV OID 2",
    SEC_OID_UNKNOWN,
    "2F:78:3D:25:52:18:A7:4A:65:39:71:B5:2C:A2:9C:45:15:6F:E9:19",
    "MDgxCzAJBgNVBAYTAkVTMRQwEgYDVQQKDAtJWkVOUEUgUy5BLjETMBEGA1UEAwwK"
    "SXplbnBlLmNvbQ==",
    "ALC3WhZIX7/hy/WL1xnmfQ==",
    nullptr
  },
  {
    // CN=A-Trust-nQual-03,OU=A-Trust-nQual-03,O=A-Trust Ges. f. Sicherheitssysteme im elektr. Datenverkehr GmbH,C=AT
    "1.2.40.0.17.1.22",
    "A-Trust EV OID",
    SEC_OID_UNKNOWN,
    "D3:C0:63:F2:19:ED:07:3E:34:AD:5D:75:0B:32:76:29:FF:D5:9A:F2",
    "MIGNMQswCQYDVQQGEwJBVDFIMEYGA1UECgw/QS1UcnVzdCBHZXMuIGYuIFNpY2hl"
    "cmhlaXRzc3lzdGVtZSBpbSBlbGVrdHIuIERhdGVudmVya2VociBHbWJIMRkwFwYD"
    "VQQLDBBBLVRydXN0LW5RdWFsLTAzMRkwFwYDVQQDDBBBLVRydXN0LW5RdWFsLTAz",
    "AWwe",
    nullptr
  },
  {
    // OU=Sample Certification Authority,O=\"Sample, Inc.\",C=US
    "0.0.0.0",
    0, // for real entries use a string like "Sample INVALID EV OID"
    SEC_OID_UNKNOWN,
    "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33", //UPPERCASE!
    "Cg==",
    "Cg==",
    nullptr
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
  dotted_oid = nullptr;
  oid_name = nullptr;
  oid_tag = SEC_OID_UNKNOWN;
  ev_root_sha1_fingerprint = nullptr;
  issuer_base64 = nullptr;
  serial_base64 = nullptr;
  cert = nullptr;
}

nsMyTrustedEVInfoClass::~nsMyTrustedEVInfoClass()
{
  // Cast away const-ness in order to free these strings
  free(const_cast<char*>(dotted_oid));
  free(const_cast<char*>(oid_name));
  free(const_cast<char*>(ev_root_sha1_fingerprint));
  free(const_cast<char*>(issuer_base64));
  free(const_cast<char*>(serial_base64));
  if (cert)
    CERT_DestroyCertificate(cert);
}

typedef nsTArray< nsMyTrustedEVInfoClass* > testEVArray; 
static testEVArray *testEVInfos;
static bool testEVInfosLoaded = false;
#endif

static bool isEVMatch(SECOidTag policyOIDTag, 
                        CERTCertificate *rootCert, 
                        const nsMyTrustedEVInfo &info)
{
  if (!rootCert)
    return false;

  NS_ConvertASCIItoUTF16 info_sha1(info.ev_root_sha1_fingerprint);

  nsNSSCertificate c(rootCert);

  nsAutoString fingerprint;
  if (NS_FAILED(c.GetSha1Fingerprint(fingerprint)))
    return false;

  if (fingerprint != info_sha1)
    return false;

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
  bool isMore = true;

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
  bool found_error = false;

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
      found_error = true;
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
      continue;
    }
    else if (reader_position == pos_issuer &&
        descriptor.EqualsLiteral(("3_issuer"))) {
      issuer = data;
      reader_position = pos_serial;
      continue;
    }
    else if (reader_position == pos_serial &&
        descriptor.EqualsLiteral(("4_serial"))) {
      serial = data;
      reader_position = pos_fingerprint;
    }
    else {
      found_error = true;
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

    temp_ev->cert = CERT_FindCertByIssuerAndSN(nullptr, &ias);
    NS_ASSERTION(temp_ev->cert, "Could not find EV root in NSS storage");

    SECITEM_FreeItem(&ias.derIssuer, false);
    SECITEM_FreeItem(&ias.serialNumber, false);

    if (!temp_ev->cert)
      return;

    nsNSSCertificate c(temp_ev->cert);
    nsAutoString fingerprint;
    c.GetSha1Fingerprint(fingerprint);

    NS_ConvertASCIItoUTF16 sha1(temp_ev->ev_root_sha1_fingerprint);

    if (sha1 != fingerprint) {
      NS_ASSERTION(sha1 == fingerprint, "found EV root with unexpected SHA1 mismatch");
      CERT_DestroyCertificate(temp_ev->cert);
      temp_ev->cert = nullptr;
      return;
    }

    SECItem ev_oid_item;
    ev_oid_item.data = nullptr;
    ev_oid_item.len = 0;
    SECStatus srv = SEC_StringToOID(nullptr, &ev_oid_item,
                                    readable_oid.get(), readable_oid.Length());
    if (srv != SECSuccess) {
      delete temp_ev;
      found_error = true;
      break;
    }

    temp_ev->oid_tag = register_oid(&ev_oid_item, temp_ev->oid_name);
    SECITEM_FreeItem(&ev_oid_item, false);

    testEVInfos->AppendElement(temp_ev);
  }

  if (found_error) {
    fprintf(stderr, "invalid line %d in test_ev_roots file\n", line_counter);
  }
}

static bool 
isEVPolicyInExternalDebugRootsFile(SECOidTag policyOIDTag)
{
  if (!testEVInfos)
    return false;

  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return false;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return false;

  for (size_t i=0; i<testEVInfos->Length(); ++i) {
    nsMyTrustedEVInfoClass *ev = testEVInfos->ElementAt(i);
    if (!ev)
      continue;
    if (policyOIDTag == ev->oid_tag)
      return true;
  }

  return false;
}

static bool 
getRootsForOidFromExternalRootsFile(CERTCertList* certList, 
                                    SECOidTag policyOIDTag)
{
  if (!testEVInfos)
    return false;

  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return false;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return false;

  for (size_t i=0; i<testEVInfos->Length(); ++i) {
    nsMyTrustedEVInfoClass *ev = testEVInfos->ElementAt(i);
    if (!ev)
      continue;
    if (policyOIDTag == ev->oid_tag)
      CERT_AddCertToListTail(certList, CERT_DupCertificate(ev->cert));
  }

  return false;
}

static bool 
isEVMatchInExternalDebugRootsFile(SECOidTag policyOIDTag, 
                                  CERTCertificate *rootCert)
{
  if (!testEVInfos)
    return false;

  if (!rootCert)
    return false;
  
  char *env_val = getenv("ENABLE_TEST_EV_ROOTS_FILE");
  if (!env_val)
    return false;
    
  int enabled_val = atoi(env_val);
  if (!enabled_val)
    return false;

  for (size_t i=0; i<testEVInfos->Length(); ++i) {
    nsMyTrustedEVInfoClass *ev = testEVInfos->ElementAt(i);
    if (!ev)
      continue;
    if (isEVMatch(policyOIDTag, rootCert, *ev))
      return true;
  }

  return false;
}
#endif

static bool 
isEVPolicy(SECOidTag policyOIDTag)
{
  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (policyOIDTag == entry.oid_tag) {
      return true;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (isEVPolicyInExternalDebugRootsFile(policyOIDTag)) {
    return true;
  }
#endif

  return false;
}

static CERTCertList*
getRootsForOid(SECOidTag oid_tag)
{
  CERTCertList *certList = CERT_NewCertList();
  if (!certList)
    return nullptr;

  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (entry.oid_tag == oid_tag)
      CERT_AddCertToListTail(certList, CERT_DupCertificate(entry.cert));
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  getRootsForOidFromExternalRootsFile(certList, oid_tag);
#endif
  return certList;
}

static bool 
isApprovedForEV(SECOidTag policyOIDTag, CERTCertificate *rootCert)
{
  if (!rootCert)
    return false;

  for (size_t iEV=0; iEV < (sizeof(myTrustedEVInfos)/sizeof(nsMyTrustedEVInfo)); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (!entry.oid_name) // invalid or placeholder list entry
      continue;
    if (isEVMatch(policyOIDTag, rootCert, entry)) {
      return true;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (isEVMatchInExternalDebugRootsFile(policyOIDTag, rootCert)) {
    return true;
  }
#endif

  return false;
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
    ias.serialNumber.type = siUnsignedInteger;

    entry.cert = CERT_FindCertByIssuerAndSN(nullptr, &ias);
    NS_ASSERTION(entry.cert, "Could not find EV root in NSS storage");

    SECITEM_FreeItem(&ias.derIssuer, false);
    SECITEM_FreeItem(&ias.serialNumber, false);

    if (!entry.cert)
      continue;

    nsNSSCertificate c(entry.cert);
    nsAutoString fingerprint;
    c.GetSha1Fingerprint(fingerprint);

    NS_ConvertASCIItoUTF16 sha1(entry.ev_root_sha1_fingerprint);

    if (sha1 != fingerprint) {
      NS_ASSERTION(sha1 == fingerprint, "found EV root with unexpected SHA1 mismatch");
      CERT_DestroyCertificate(entry.cert);
      entry.cert = nullptr;
      continue;
    }

    SECItem ev_oid_item;
    ev_oid_item.data = nullptr;
    ev_oid_item.len = 0;
    SECStatus srv = SEC_StringToOID(nullptr, &ev_oid_item, 
                                    entry.dotted_oid, 0);
    if (srv != SECSuccess)
      continue;

    entry.oid_tag = register_oid(&ev_oid_item, entry.oid_name);

    SECITEM_FreeItem(&ev_oid_item, false);
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (!testEVInfosLoaded) {
    testEVInfosLoaded = true;
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
    for (int i=0; cert->extensions[i] != nullptr; i++) {
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

      bool found = false;
      while (*policyInfos != NULL) {
        policyInfo = *policyInfos++;

        SECOidTag oid_tag = policyInfo->oid;
        if (oid_tag != SEC_OID_UNKNOWN && isEVPolicy(oid_tag)) {
          // in our list of OIDs accepted for EV
          outOidTag = oid_tag;
          found = true;
          break;
        }
      }
      CERT_DestroyCertificatePoliciesExtension(policies);
      if (found)
        return SECSuccess;
    }
  }

  return SECFailure;
}

NS_IMETHODIMP
nsSSLStatus::GetIsExtendedValidation(bool* aIsEV)
{
  NS_ENSURE_ARG_POINTER(aIsEV);
  *aIsEV = false;

  nsCOMPtr<nsIX509Cert> cert = mServerCert;
  nsresult rv;
  nsCOMPtr<nsIIdentityInfo> idinfo = do_QueryInterface(cert, &rv);

  // mServerCert should never be null when this method is called because
  // nsSSLStatus objects always have mServerCert set right after they are
  // constructed and before they are returned. GetIsExtendedValidation should
  // only be called in the chrome process (in e10s), and mServerCert will always
  // implement nsIIdentityInfo in the chrome process.
  if (!idinfo) {
    NS_ERROR("nsSSLStatus has null mServerCert or was called in the content "
             "process");
    return NS_ERROR_UNEXPECTED;
  }

  // Never allow bad certs for EV, regardless of overrides.
  if (mHaveCertErrorBits)
    return NS_OK;

  return idinfo->GetIsExtendedValidation(aIsEV);
}

nsresult
nsNSSCertificate::hasValidEVOidTag(SECOidTag &resultOidTag, bool &validEV)
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

  validEV = false;
  resultOidTag = SEC_OID_UNKNOWN;

  bool isOCSPEnabled = false;
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
  CERTCertListCleaner rootListCleaner(rootList);

  CERTRevocationMethodIndex preferedRevMethods[1] = { 
    cert_revocation_method_ocsp
  };

  PRUint64 revMethodFlags = 
    CERT_REV_M_TEST_USING_THIS_METHOD
    | CERT_REV_M_ALLOW_NETWORK_FETCHING
    | CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE
    | CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE
    | CERT_REV_M_IGNORE_MISSING_FRESH_INFO
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
  rev.chainTests.cert_rev_flags_per_method = methodFlags;
  rev.chainTests.number_of_preferred_methods = 1;
  rev.chainTests.preferred_methods = preferedRevMethods;
  rev.chainTests.cert_rev_method_independent_flags =
    revMethodIndependentFlags;

  CERTValInParam cvin[4];
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
  cvout[0].value.pointer.cert = nullptr;
  cvout[1].type = cert_po_end;

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG, ("calling CERT_PKIXVerifyCert nss cert %p\n", mCert));
  rv = CERT_PKIXVerifyCert(mCert, certificateUsageSSLServer,
                           cvin, cvout, nullptr);
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
nsNSSCertificate::getValidEVOidTag(SECOidTag &resultOidTag, bool &validEV)
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
nsNSSCertificate::GetIsExtendedValidation(bool* aIsEV)
{
  nsNSSShutDownPreventionLock locker;
  if (isAlreadyShutDown())
    return NS_ERROR_NOT_AVAILABLE;

  NS_ENSURE_ARG(aIsEV);
  *aIsEV = false;

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
  bool valid;
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
      entry.cert = nullptr;
    }
  }

#ifdef PSM_ENABLE_TEST_EV_ROOTS
  if (testEVInfosLoaded) {
    testEVInfosLoaded = false;
    if (testEVInfos) {
      for (size_t i = 0; i<testEVInfos->Length(); ++i) {
        delete testEVInfos->ElementAt(i);
      }
      testEVInfos->Clear();
      delete testEVInfos;
      testEVInfos = nullptr;
    }
  }
#endif
  memset(&mIdentityInfoCallOnce, 0, sizeof(PRCallOnceType));
}

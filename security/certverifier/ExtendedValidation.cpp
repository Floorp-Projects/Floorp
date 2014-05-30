/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtendedValidation.h"

#include "cert.h"
#include "certdb.h"
#include "base64.h"
#include "hasht.h"
#include "pkix/nullptr.h"
#include "pkix/pkixtypes.h"
#include "pk11pub.h"
#include "secerr.h"
#include "prerror.h"
#include "prinit.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

#define CONST_OID static const unsigned char
#define OI(x) { siDEROID, (unsigned char*) x, sizeof x }

struct nsMyTrustedEVInfo
{
  const char* dotted_oid;
  const char* oid_name; // Set this to null to signal an invalid structure,
                  // (We can't have an empty list, so we'll use a dummy entry)
  SECOidTag oid_tag;
  const unsigned char ev_root_sha256_fingerprint[SHA256_LENGTH];
  const char* issuer_base64;
  const char* serial_base64;
  CERTCertificate* cert;
};

// HOWTO enable additional CA root certificates for EV:
//
// For each combination of "root certificate" and "policy OID",
// one entry must be added to the array named myTrustedEVInfos.
//
// We use the combination of "issuer name" and "serial number" to
// uniquely identify the certificate. In order to avoid problems
// because of encodings when comparing certificates, we don't
// use plain text representation, we rather use the original encoding
// as it can be found in the root certificate (in base64 format).
//
// We can use the NSS utility named "pp" to extract the encoding.
//
// Build standalone NSS including the NSS tools, then run
//   pp -t certificate-identity -i the-cert-filename
//
// You will need the output from sections "Issuer", "Fingerprint (SHA-256)",
// "Issuer DER Base64" and "Serial DER Base64".
//
// The new section consists of 8 lines:
//
// - a comment that should contain the human readable issuer name
//   of the certificate, as printed by the pp tool
// - the EV policy OID that is associated to the EV grant
// - a text description of the EV policy OID. The array can contain
//   multiple entries with the same OID.
//   Please make sure to use the identical OID text description for
//   all entries with the same policy OID (use the text search
//   feature of your text editor to find duplicates).
//   When adding a new policy OID that is not yet contained in the array,
//   please make sure that your new description is different from
//   all the other descriptions (again use the text search feature
//   to be sure).
// - the constant SEC_OID_UNKNOWN
//   (it will be replaced at runtime with another identifier)
// - the SHA-256 fingerprint
// - the "Issuer DER Base64" as printed by the pp tool.
//   Remove all whitespaces. If you use multiple lines, make sure that
//   only the final line will be followed by a comma.
// - the "Serial DER Base64" (as printed by pp)
// - nullptr
//
// After adding an entry, test it locally against the test site that
// has been provided by the CA. Note that you must use a version of NSS
// where the root certificate has already been added and marked as trusted
// for issuing SSL server certificates (at least).
//
// If you are able to connect to the site without certificate errors,
// but you don't see the EV status indicator, then most likely the CA
// has a problem in their infrastructure. The most common problems are
// related to the CA's OCSP infrastructure, either they use an incorrect
// OCSP signing certificate, or OCSP for the intermediate certificates
// isn't working, or OCSP isn't working at all.

static struct nsMyTrustedEVInfo myTrustedEVInfos[] = {
  // IMPORTANT! When extending this list,
  // pairs of dotted_oid and oid_name should always be unique pairs.
  // In other words, if you add another list, that uses the same dotted_oid
  // as an existing entry, then please use the same oid_name.
#ifdef DEBUG
  // Debug EV certificates should all use the OID (repeating EV OID is OK):
  // 1.3.6.1.4.1.13769.666.666.666.1.500.9.1.
  // If you add or remove debug EV certs you must also modify IdentityInfoInit
  // (there is another #ifdef DEBUG section there) so that the correct number of
  // certs are skipped as these debug EV certs are NOT part of the default trust
  // store.
  {
    // This is the testing EV signature (xpcshell) (RSA)
    // CN=XPCShell EV Testing (untrustworthy) CA,OU=Security Engineering,O=Mozilla - EV debug test CA,L=Mountain View,ST=CA,C=US"
    "1.3.6.1.4.1.13769.666.666.666.1.500.9.1",
    "DEBUGtesting EV OID",
    SEC_OID_UNKNOWN,
    { 0x2D, 0x94, 0x52, 0x70, 0xAA, 0x92, 0x13, 0x0B, 0x1F, 0xB1, 0x24,
      0x0B, 0x24, 0xB1, 0xEE, 0x4E, 0xFB, 0x7C, 0x43, 0x45, 0x45, 0x7F,
      0x97, 0x6C, 0x90, 0xBF, 0xD4, 0x8A, 0x04, 0x79, 0xE4, 0x68 },
    "MIGnMQswCQYDVQQGEwJVUzELMAkGA1UECAwCQ0ExFjAUBgNVBAcMDU1vdW50YWlu"
    "IFZpZXcxIzAhBgNVBAoMGk1vemlsbGEgLSBFViBkZWJ1ZyB0ZXN0IENBMR0wGwYD"
    "VQQLDBRTZWN1cml0eSBFbmdpbmVlcmluZzEvMC0GA1UEAwwmWFBDU2hlbGwgRVYg"
    "VGVzdGluZyAodW50cnVzdHdvcnRoeSkgQ0E=",
    "At+3zdo=",
    nullptr
  },
#endif
  {
    // OU=Security Communication EV RootCA1,O="SECOM Trust Systems CO.,LTD.",C=JP
    "1.2.392.200091.100.721.1",
    "SECOM EV OID",
    SEC_OID_UNKNOWN,
    { 0xA2, 0x2D, 0xBA, 0x68, 0x1E, 0x97, 0x37, 0x6E, 0x2D, 0x39, 0x7D,
      0x72, 0x8A, 0xAE, 0x3A, 0x9B, 0x62, 0x96, 0xB9, 0xFD, 0xBA, 0x60,
      0xBC, 0x2E, 0x11, 0xF6, 0x47, 0xF2, 0xC6, 0x75, 0xFB, 0x37 },
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
    { 0x96, 0x0A, 0xDF, 0x00, 0x63, 0xE9, 0x63, 0x56, 0x75, 0x0C, 0x29,
      0x65, 0xDD, 0x0A, 0x08, 0x67, 0xDA, 0x0B, 0x9C, 0xBD, 0x6E, 0x77,
      0x71, 0x4A, 0xEA, 0xFB, 0x23, 0x49, 0xAB, 0x39, 0x3D, 0xA3 },
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
    { 0x62, 0xDD, 0x0B, 0xE9, 0xB9, 0xF5, 0x0A, 0x16, 0x3E, 0xA0, 0xF8,
      0xE7, 0x5C, 0x05, 0x3B, 0x1E, 0xCA, 0x57, 0xEA, 0x55, 0xC8, 0x68,
      0x8F, 0x64, 0x7C, 0x68, 0x81, 0xF2, 0xC8, 0x35, 0x7B, 0x95 },
    "MEUxCzAJBgNVBAYTAkNIMRUwEwYDVQQKEwxTd2lzc1NpZ24gQUcxHzAdBgNVBAMT"
    "FlN3aXNzU2lnbiBHb2xkIENBIC0gRzI=",
    "ALtAHEP1Xk+w",
    nullptr
  },
  {
    // CN=StartCom Certification Authority,OU=Secure Digital Certificate Signing,O=StartCom Ltd.,C=IL
    "1.3.6.1.4.1.23223.1.1.1",
    "StartCom EV OID",
    SEC_OID_UNKNOWN,
    { 0xC7, 0x66, 0xA9, 0xBE, 0xF2, 0xD4, 0x07, 0x1C, 0x86, 0x3A, 0x31,
      0xAA, 0x49, 0x20, 0xE8, 0x13, 0xB2, 0xD1, 0x98, 0x60, 0x8C, 0xB7,
      0xB7, 0xCF, 0xE2, 0x11, 0x43, 0xB8, 0x36, 0xDF, 0x09, 0xEA },
    "MH0xCzAJBgNVBAYTAklMMRYwFAYDVQQKEw1TdGFydENvbSBMdGQuMSswKQYDVQQL"
    "EyJTZWN1cmUgRGlnaXRhbCBDZXJ0aWZpY2F0ZSBTaWduaW5nMSkwJwYDVQQDEyBT"
    "dGFydENvbSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "AQ==",
    nullptr
  },
  {
    // CN=StartCom Certification Authority,OU=Secure Digital Certificate Signing,O=StartCom Ltd.,C=IL
    "1.3.6.1.4.1.23223.1.1.1",
    "StartCom EV OID",
    SEC_OID_UNKNOWN,
    { 0xE1, 0x78, 0x90, 0xEE, 0x09, 0xA3, 0xFB, 0xF4, 0xF4, 0x8B, 0x9C,
      0x41, 0x4A, 0x17, 0xD6, 0x37, 0xB7, 0xA5, 0x06, 0x47, 0xE9, 0xBC,
      0x75, 0x23, 0x22, 0x72, 0x7F, 0xCC, 0x17, 0x42, 0xA9, 0x11 },
    "MH0xCzAJBgNVBAYTAklMMRYwFAYDVQQKEw1TdGFydENvbSBMdGQuMSswKQYDVQQL"
    "EyJTZWN1cmUgRGlnaXRhbCBDZXJ0aWZpY2F0ZSBTaWduaW5nMSkwJwYDVQQDEyBT"
    "dGFydENvbSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "LQ==",
    nullptr
  },
  {
    // CN=StartCom Certification Authority G2,O=StartCom Ltd.,C=IL
    "1.3.6.1.4.1.23223.1.1.1",
    "StartCom EV OID",
    SEC_OID_UNKNOWN,
    { 0xC7, 0xBA, 0x65, 0x67, 0xDE, 0x93, 0xA7, 0x98, 0xAE, 0x1F, 0xAA,
      0x79, 0x1E, 0x71, 0x2D, 0x37, 0x8F, 0xAE, 0x1F, 0x93, 0xC4, 0x39,
      0x7F, 0xEA, 0x44, 0x1B, 0xB7, 0xCB, 0xE6, 0xFD, 0x59, 0x95 },
    "MFMxCzAJBgNVBAYTAklMMRYwFAYDVQQKEw1TdGFydENvbSBMdGQuMSwwKgYDVQQD"
    "EyNTdGFydENvbSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eSBHMg==",
    "Ow==",
    nullptr
  },
  {
    // CN=VeriSign Class 3 Public Primary Certification Authority - G5,OU="(c) 2006 VeriSign, Inc. - For authorized use only",OU=VeriSign Trust Network,O="VeriSign, Inc.",C=US
    "2.16.840.1.113733.1.7.23.6",
    "VeriSign EV OID",
    SEC_OID_UNKNOWN,
    { 0x9A, 0xCF, 0xAB, 0x7E, 0x43, 0xC8, 0xD8, 0x80, 0xD0, 0x6B, 0x26,
      0x2A, 0x94, 0xDE, 0xEE, 0xE4, 0xB4, 0x65, 0x99, 0x89, 0xC3, 0xD0,
      0xCA, 0xF1, 0x9B, 0xAF, 0x64, 0x05, 0xE4, 0x1A, 0xB7, 0xDF },
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
    { 0x37, 0xD5, 0x10, 0x06, 0xC5, 0x12, 0xEA, 0xAB, 0x62, 0x64, 0x21,
      0xF1, 0xEC, 0x8C, 0x92, 0x01, 0x3F, 0xC5, 0xF8, 0x2A, 0xE9, 0x8E,
      0xE5, 0x33, 0xEB, 0x46, 0x19, 0xB8, 0xDE, 0xB4, 0xD0, 0x6C },
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
    { 0x8D, 0x72, 0x2F, 0x81, 0xA9, 0xC1, 0x13, 0xC0, 0x79, 0x1D, 0xF1,
      0x36, 0xA2, 0x96, 0x6D, 0xB2, 0x6C, 0x95, 0x0A, 0x97, 0x1D, 0xB4,
      0x6B, 0x41, 0x99, 0xF4, 0xEA, 0x54, 0xB7, 0x8B, 0xFB, 0x9F },
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
    { 0xCE, 0xCD, 0xDC, 0x90, 0x50, 0x99, 0xD8, 0xDA, 0xDF, 0xC5, 0xB1,
      0xD2, 0x09, 0xB7, 0x37, 0xCB, 0xE2, 0xC1, 0x8C, 0xFB, 0x2C, 0x10,
      0xC0, 0xFF, 0x0B, 0xCF, 0x0D, 0x32, 0x86, 0xFC, 0x1A, 0xA2 },
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
    { 0xF1, 0xC1, 0xB5, 0x0A, 0xE5, 0xA2, 0x0D, 0xD8, 0x03, 0x0E, 0xC9,
      0xF6, 0xBC, 0x24, 0x82, 0x3D, 0xD3, 0x67, 0xB5, 0x25, 0x57, 0x59,
      0xB4, 0xE7, 0x1B, 0x61, 0xFC, 0xE9, 0xF7, 0x37, 0x5D, 0x73 },
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
    { 0x42, 0x00, 0xF5, 0x04, 0x3A, 0xC8, 0x59, 0x0E, 0xBB, 0x52, 0x7D,
      0x20, 0x9E, 0xD1, 0x50, 0x30, 0x29, 0xFB, 0xCB, 0xD4, 0x1C, 0xA1,
      0xB5, 0x06, 0xEC, 0x27, 0xF1, 0x5A, 0xDE, 0x7D, 0xAC, 0x69 },
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
    { 0x17, 0x93, 0x92, 0x7A, 0x06, 0x14, 0x54, 0x97, 0x89, 0xAD, 0xCE,
      0x2F, 0x8F, 0x34, 0xF7, 0xF0, 0xB6, 0x6D, 0x0F, 0x3A, 0xE3, 0xA3,
      0xB8, 0x4D, 0x21, 0xEC, 0x15, 0xDB, 0xBA, 0x4F, 0xAD, 0xC7 },
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
    { 0x0C, 0x2C, 0xD6, 0x3D, 0xF7, 0x80, 0x6F, 0xA3, 0x99, 0xED, 0xE8,
      0x09, 0x11, 0x6B, 0x57, 0x5B, 0xF8, 0x79, 0x89, 0xF0, 0x65, 0x18,
      0xF9, 0x80, 0x8C, 0x86, 0x05, 0x03, 0x17, 0x8B, 0xAF, 0x66 },
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
    { 0x68, 0x7F, 0xA4, 0x51, 0x38, 0x22, 0x78, 0xFF, 0xF0, 0xC8, 0xB1,
      0x1F, 0x8D, 0x43, 0xD5, 0x76, 0x67, 0x1C, 0x6E, 0xB2, 0xBC, 0xEA,
      0xB4, 0x13, 0xFB, 0x83, 0xD9, 0x65, 0xD0, 0x6D, 0x2F, 0xF2 },
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
    { 0x85, 0xFB, 0x2F, 0x91, 0xDD, 0x12, 0x27, 0x5A, 0x01, 0x45, 0xB6,
      0x36, 0x53, 0x4F, 0x84, 0x02, 0x4A, 0xD6, 0x8B, 0x69, 0xB8, 0xEE,
      0x88, 0x68, 0x4F, 0xF7, 0x11, 0x37, 0x58, 0x05, 0xB3, 0x48 },
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
    { 0x6E, 0xA5, 0x47, 0x41, 0xD0, 0x04, 0x66, 0x7E, 0xED, 0x1B, 0x48,
      0x16, 0x63, 0x4A, 0xA3, 0xA7, 0x9E, 0x6E, 0x4B, 0x96, 0x95, 0x0F,
      0x82, 0x79, 0xDA, 0xFC, 0x8D, 0x9B, 0xD8, 0x81, 0x21, 0x37 },
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
    { 0xC3, 0x84, 0x6B, 0xF2, 0x4B, 0x9E, 0x93, 0xCA, 0x64, 0x27, 0x4C,
      0x0E, 0xC6, 0x7C, 0x1E, 0xCC, 0x5E, 0x02, 0x4F, 0xFC, 0xAC, 0xD2,
      0xD7, 0x40, 0x19, 0x35, 0x0E, 0x81, 0xFE, 0x54, 0x6A, 0xE4 },
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
    { 0x45, 0x14, 0x0B, 0x32, 0x47, 0xEB, 0x9C, 0xC8, 0xC5, 0xB4, 0xF0,
      0xD7, 0xB5, 0x30, 0x91, 0xF7, 0x32, 0x92, 0x08, 0x9E, 0x6E, 0x5A,
      0x63, 0xE2, 0x74, 0x9D, 0xD3, 0xAC, 0xA9, 0x19, 0x8E, 0xDA },
    "MIGDMQswCQYDVQQGEwJVUzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2Nv"
    "dHRzZGFsZTEaMBgGA1UEChMRR29EYWRkeS5jb20sIEluYy4xMTAvBgNVBAMTKEdv"
    "IERhZGR5IFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzI=",
    "AA==",
    nullptr
  },
  {
    // OU=Starfield Class 2 Certification Authority,O=\"Starfield Technologies, Inc.\",C=US
    "2.16.840.1.114414.1.7.23.3",
    "Go Daddy EV OID b",
    SEC_OID_UNKNOWN,
    { 0x14, 0x65, 0xFA, 0x20, 0x53, 0x97, 0xB8, 0x76, 0xFA, 0xA6, 0xF0,
      0xA9, 0x95, 0x8E, 0x55, 0x90, 0xE4, 0x0F, 0xCC, 0x7F, 0xAA, 0x4F,
      0xB7, 0xC2, 0xC8, 0x67, 0x75, 0x21, 0xFB, 0x5F, 0xB6, 0x58 },
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
    { 0x2C, 0xE1, 0xCB, 0x0B, 0xF9, 0xD2, 0xF9, 0xE1, 0x02, 0x99, 0x3F,
      0xBE, 0x21, 0x51, 0x52, 0xC3, 0xB2, 0xDD, 0x0C, 0xAB, 0xDE, 0x1C,
      0x68, 0xE5, 0x31, 0x9B, 0x83, 0x91, 0x54, 0xDB, 0xB7, 0xF5 },
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
    { 0x74, 0x31, 0xE5, 0xF4, 0xC3, 0xC1, 0xCE, 0x46, 0x90, 0x77, 0x4F,
      0x0B, 0x61, 0xE0, 0x54, 0x40, 0x88, 0x3B, 0xA9, 0xA0, 0x1E, 0xD0,
      0x0B, 0xA6, 0xAB, 0xD7, 0x80, 0x6E, 0xD3, 0xB1, 0x18, 0xCF },
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
    { 0x85, 0xA0, 0xDD, 0x7D, 0xD7, 0x20, 0xAD, 0xB7, 0xFF, 0x05, 0xF8,
      0x3D, 0x54, 0x2B, 0x20, 0x9D, 0xC7, 0xFF, 0x45, 0x28, 0xF7, 0xD6,
      0x77, 0xB1, 0x83, 0x89, 0xFE, 0xA5, 0xE5, 0xC4, 0x9E, 0x86 },
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
    { 0x15, 0xF0, 0xBA, 0x00, 0xA3, 0xAC, 0x7A, 0xF3, 0xAC, 0x88, 0x4C,
      0x07, 0x2B, 0x10, 0x11, 0xA0, 0x77, 0xBD, 0x77, 0xC0, 0x97, 0xF4,
      0x01, 0x64, 0xB2, 0xF8, 0x59, 0x8A, 0xBD, 0x83, 0x86, 0x0C },
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
    { 0x73, 0xC1, 0x76, 0x43, 0x4F, 0x1B, 0xC6, 0xD5, 0xAD, 0xF4, 0x5B,
      0x0E, 0x76, 0xE7, 0x27, 0x28, 0x7C, 0x8D, 0xE5, 0x76, 0x16, 0xC1,
      0xE6, 0xE6, 0x14, 0x1A, 0x2B, 0x2C, 0xBC, 0x7D, 0x8E, 0x4C },
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
    { 0xEB, 0xD4, 0x10, 0x40, 0xE4, 0xBB, 0x3E, 0xC7, 0x42, 0xC9, 0xE3,
      0x81, 0xD3, 0x1E, 0xF2, 0xA4, 0x1A, 0x48, 0xB6, 0x68, 0x5C, 0x96,
      0xE7, 0xCE, 0xF3, 0xC1, 0xDF, 0x6C, 0xD4, 0x33, 0x1C, 0x99 },
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
    { 0xCA, 0x42, 0xDD, 0x41, 0x74, 0x5F, 0xD0, 0xB8, 0x1E, 0xB9, 0x02,
      0x36, 0x2C, 0xF9, 0xD8, 0xBF, 0x71, 0x9D, 0xA1, 0xBD, 0x1B, 0x1E,
      0xFC, 0x94, 0x6F, 0x5B, 0x4C, 0x99, 0xF4, 0x2C, 0x1B, 0x9E },
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
    { 0xCB, 0xB5, 0x22, 0xD7, 0xB7, 0xF1, 0x27, 0xAD, 0x6A, 0x01, 0x13,
      0x86, 0x5B, 0xDF, 0x1C, 0xD4, 0x10, 0x2E, 0x7D, 0x07, 0x59, 0xAF,
      0x63, 0x5A, 0x7C, 0xF4, 0x72, 0x0D, 0xC9, 0x63, 0xC5, 0x3B },
    "MEwxIDAeBgNVBAsTF0dsb2JhbFNpZ24gUm9vdCBDQSAtIFIzMRMwEQYDVQQKEwpH"
    "bG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWdu",
    "BAAAAAABIVhTCKI=",
    nullptr
  },
  {
    // CN=Buypass Class 3 CA 1,O=Buypass AS-983163327,C=NO
    "2.16.578.1.26.1.3.3",
    "Buypass EV OID",
    SEC_OID_UNKNOWN,
    { 0xB7, 0xB1, 0x2B, 0x17, 0x1F, 0x82, 0x1D, 0xAA, 0x99, 0x0C, 0xD0,
      0xFE, 0x50, 0x87, 0xB1, 0x28, 0x44, 0x8B, 0xA8, 0xE5, 0x18, 0x4F,
      0x84, 0xC5, 0x1E, 0x02, 0xB5, 0xC8, 0xFB, 0x96, 0x2B, 0x24 },
    "MEsxCzAJBgNVBAYTAk5PMR0wGwYDVQQKDBRCdXlwYXNzIEFTLTk4MzE2MzMyNzEd"
    "MBsGA1UEAwwUQnV5cGFzcyBDbGFzcyAzIENBIDE=",
    "Ag==",
    nullptr
  },
  {
    // CN=Buypass Class 3 Root CA,O=Buypass AS-983163327,C=NO
    "2.16.578.1.26.1.3.3",
    "Buypass EV OID",
    SEC_OID_UNKNOWN,
    { 0xED, 0xF7, 0xEB, 0xBC, 0xA2, 0x7A, 0x2A, 0x38, 0x4D, 0x38, 0x7B,
      0x7D, 0x40, 0x10, 0xC6, 0x66, 0xE2, 0xED, 0xB4, 0x84, 0x3E, 0x4C,
      0x29, 0xB4, 0xAE, 0x1D, 0x5B, 0x93, 0x32, 0xE6, 0xB2, 0x4D },
    "ME4xCzAJBgNVBAYTAk5PMR0wGwYDVQQKDBRCdXlwYXNzIEFTLTk4MzE2MzMyNzEg"
    "MB4GA1UEAwwXQnV5cGFzcyBDbGFzcyAzIFJvb3QgQ0E=",
    "Ag==",
    nullptr
  },
  {
    // CN=Class 2 Primary CA,O=Certplus,C=FR
    "1.3.6.1.4.1.22234.2.5.2.3.1",
    "Certplus EV OID",
    SEC_OID_UNKNOWN,
    { 0x0F, 0x99, 0x3C, 0x8A, 0xEF, 0x97, 0xBA, 0xAF, 0x56, 0x87, 0x14,
      0x0E, 0xD5, 0x9A, 0xD1, 0x82, 0x1B, 0xB4, 0xAF, 0xAC, 0xF0, 0xAA,
      0x9A, 0x58, 0xB5, 0xD5, 0x7A, 0x33, 0x8A, 0x3A, 0xFB, 0xCB },
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
    { 0x06, 0x3E, 0x4A, 0xFA, 0xC4, 0x91, 0xDF, 0xD3, 0x32, 0xF3, 0x08,
      0x9B, 0x85, 0x42, 0xE9, 0x46, 0x17, 0xD8, 0x93, 0xD7, 0xFE, 0x94,
      0x4E, 0x10, 0xA7, 0x93, 0x7E, 0xE2, 0x9D, 0x96, 0x93, 0xC0 },
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
    { 0x13, 0x63, 0x35, 0x43, 0x93, 0x34, 0xA7, 0x69, 0x80, 0x16, 0xA0,
      0xD3, 0x24, 0xDE, 0x72, 0x28, 0x4E, 0x07, 0x9D, 0x7B, 0x52, 0x20,
      0xBB, 0x8F, 0xBD, 0x74, 0x78, 0x16, 0xEE, 0xBE, 0xBA, 0xCA },
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
    { 0x30, 0x9B, 0x4A, 0x87, 0xF6, 0xCA, 0x56, 0xC9, 0x31, 0x69, 0xAA,
      0xA9, 0x9C, 0x6D, 0x98, 0x88, 0x54, 0xD7, 0x89, 0x2B, 0xD5, 0x43,
      0x7E, 0x2D, 0x07, 0xB2, 0x9C, 0xBE, 0xDA, 0x55, 0xD3, 0x5D },
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
    { 0x03, 0x76, 0xAB, 0x1D, 0x54, 0xC5, 0xF9, 0x80, 0x3C, 0xE4, 0xB2,
      0xE2, 0x01, 0xA0, 0xEE, 0x7E, 0xEF, 0x7B, 0x57, 0xB6, 0x36, 0xE8,
      0xA9, 0x3C, 0x9B, 0x8D, 0x48, 0x60, 0xC9, 0x6F, 0x5F, 0xA7 },
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
    { 0x0A, 0x81, 0xEC, 0x5A, 0x92, 0x97, 0x77, 0xF1, 0x45, 0x90, 0x4A,
      0xF3, 0x8D, 0x5D, 0x50, 0x9F, 0x66, 0xB5, 0xE2, 0xC5, 0x8F, 0xCD,
      0xB5, 0x31, 0x05, 0x8B, 0x0E, 0x17, 0xF3, 0xF0, 0xB4, 0x1B },
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
    { 0x70, 0xA7, 0x3F, 0x7F, 0x37, 0x6B, 0x60, 0x07, 0x42, 0x48, 0x90,
      0x45, 0x34, 0xB1, 0x14, 0x82, 0xD5, 0xBF, 0x0E, 0x69, 0x8E, 0xCC,
      0x49, 0x8D, 0xF5, 0x25, 0x77, 0xEB, 0xF2, 0xE9, 0x3B, 0x9A },
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
    { 0xBD, 0x71, 0xFD, 0xF6, 0xDA, 0x97, 0xE4, 0xCF, 0x62, 0xD1, 0x64,
      0x7A, 0xDD, 0x25, 0x81, 0xB0, 0x7D, 0x79, 0xAD, 0xF8, 0x39, 0x7E,
      0xB4, 0xEC, 0xBA, 0x9C, 0x5E, 0x84, 0x88, 0x82, 0x14, 0x23 },
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
    { 0x5C, 0x58, 0x46, 0x8D, 0x55, 0xF5, 0x8E, 0x49, 0x7E, 0x74, 0x39,
      0x82, 0xD2, 0xB5, 0x00, 0x10, 0xB6, 0xD1, 0x65, 0x37, 0x4A, 0xCF,
      0x83, 0xA7, 0xD4, 0xA3, 0x2D, 0xB7, 0x68, 0xC4, 0x40, 0x8E },
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
    { 0x25, 0x30, 0xCC, 0x8E, 0x98, 0x32, 0x15, 0x02, 0xBA, 0xD9, 0x6F,
      0x9B, 0x1F, 0xBA, 0x1B, 0x09, 0x9E, 0x2D, 0x29, 0x9E, 0x0F, 0x45,
      0x48, 0xBB, 0x91, 0x4F, 0x36, 0x3B, 0xC0, 0xD4, 0x53, 0x1F },
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
    { 0x25, 0x30, 0xCC, 0x8E, 0x98, 0x32, 0x15, 0x02, 0xBA, 0xD9, 0x6F,
      0x9B, 0x1F, 0xBA, 0x1B, 0x09, 0x9E, 0x2D, 0x29, 0x9E, 0x0F, 0x45,
      0x48, 0xBB, 0x91, 0x4F, 0x36, 0x3B, 0xC0, 0xD4, 0x53, 0x1F },
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
    { 0x79, 0x3C, 0xBF, 0x45, 0x59, 0xB9, 0xFD, 0xE3, 0x8A, 0xB2, 0x2D,
      0xF1, 0x68, 0x69, 0xF6, 0x98, 0x81, 0xAE, 0x14, 0xC4, 0xB0, 0x13,
      0x9A, 0xC7, 0x88, 0xA7, 0x8A, 0x1A, 0xFC, 0xCA, 0x02, 0xFB },
    "MIGNMQswCQYDVQQGEwJBVDFIMEYGA1UECgw/QS1UcnVzdCBHZXMuIGYuIFNpY2hl"
    "cmhlaXRzc3lzdGVtZSBpbSBlbGVrdHIuIERhdGVudmVya2VociBHbWJIMRkwFwYD"
    "VQQLDBBBLVRydXN0LW5RdWFsLTAzMRkwFwYDVQQDDBBBLVRydXN0LW5RdWFsLTAz",
    "AWwe",
    nullptr
  },
  {
    // CN=T-TeleSec GlobalRoot Class 3,OU=T-Systems Trust Center,O=T-Systems Enterprise Services GmbH,C=DE
    "1.3.6.1.4.1.7879.13.24.1",
    "T-Systems EV OID",
    SEC_OID_UNKNOWN,
    { 0xFD, 0x73, 0xDA, 0xD3, 0x1C, 0x64, 0x4F, 0xF1, 0xB4, 0x3B, 0xEF,
      0x0C, 0xCD, 0xDA, 0x96, 0x71, 0x0B, 0x9C, 0xD9, 0x87, 0x5E, 0xCA,
      0x7E, 0x31, 0x70, 0x7A, 0xF3, 0xE9, 0x6D, 0x52, 0x2B, 0xBD },
    "MIGCMQswCQYDVQQGEwJERTErMCkGA1UECgwiVC1TeXN0ZW1zIEVudGVycHJpc2Ug"
    "U2VydmljZXMgR21iSDEfMB0GA1UECwwWVC1TeXN0ZW1zIFRydXN0IENlbnRlcjEl"
    "MCMGA1UEAwwcVC1UZWxlU2VjIEdsb2JhbFJvb3QgQ2xhc3MgMw==",
    "AQ==",
    nullptr
  },
  {
    // CN=TURKTRUST Elektronik Sertifika Hizmet Saglayicisi,O=TURKTRUST Bilgi Illetisim ve Bilisim Guvenligi Hizmetleri A.S.,C=TR
    "2.16.792.3.0.3.1.1.5",
    "TurkTrust EV OID",
    SEC_OID_UNKNOWN,
    { 0x97, 0x8C, 0xD9, 0x66, 0xF2, 0xFA, 0xA0, 0x7B, 0xA7, 0xAA, 0x95,
      0x00, 0xD9, 0xC0, 0x2E, 0x9D, 0x77, 0xF2, 0xCD, 0xAD, 0xA6, 0xAD,
      0x6B, 0xA7, 0x4A, 0xF4, 0xB9, 0x1C, 0x66, 0x59, 0x3C, 0x50 },
    "MIG/MT8wPQYDVQQDDDZUw5xSS1RSVVNUIEVsZWt0cm9uaWsgU2VydGlmaWthIEhp"
    "em1ldCBTYcSfbGF5xLFjxLFzxLExCzAJBgNVBAYTAlRSMQ8wDQYDVQQHDAZBbmth"
    "cmExXjBcBgNVBAoMVVTDnFJLVFJVU1QgQmlsZ2kgxLBsZXRpxZ9pbSB2ZSBCaWxp"
    "xZ9pbSBHw7x2ZW5sacSfaSBIaXptZXRsZXJpIEEuxZ4uIChjKSBBcmFsxLFrIDIw"
    "MDc=",
    "AQ==",
    nullptr
  },
  {
    // CN=China Internet Network Information Center EV Certificates Root,O=China Internet Network Information Center,C=CN
    "1.3.6.1.4.1.29836.1.10",
    "CNNIC EV OID",
    SEC_OID_UNKNOWN,
    { 0x1C, 0x01, 0xC6, 0xF4, 0xDB, 0xB2, 0xFE, 0xFC, 0x22, 0x55, 0x8B,
      0x2B, 0xCA, 0x32, 0x56, 0x3F, 0x49, 0x84, 0x4A, 0xCF, 0xC3, 0x2B,
      0x7B, 0xE4, 0xB0, 0xFF, 0x59, 0x9F, 0x9E, 0x8C, 0x7A, 0xF7 },
    "MIGKMQswCQYDVQQGEwJDTjEyMDAGA1UECgwpQ2hpbmEgSW50ZXJuZXQgTmV0d29y"
    "ayBJbmZvcm1hdGlvbiBDZW50ZXIxRzBFBgNVBAMMPkNoaW5hIEludGVybmV0IE5l"
    "dHdvcmsgSW5mb3JtYXRpb24gQ2VudGVyIEVWIENlcnRpZmljYXRlcyBSb290",
    "SJ8AAQ==",
    nullptr
  },
  {
    // CN=TWCA Root Certification Authority,OU=Root CA,O=TAIWAN-CA,C=TW
    "1.3.6.1.4.1.40869.1.1.22.3",
    "TWCA EV OID",
    SEC_OID_UNKNOWN,
    { 0xBF, 0xD8, 0x8F, 0xE1, 0x10, 0x1C, 0x41, 0xAE, 0x3E, 0x80, 0x1B,
      0xF8, 0xBE, 0x56, 0x35, 0x0E, 0xE9, 0xBA, 0xD1, 0xA6, 0xB9, 0xBD,
      0x51, 0x5E, 0xDC, 0x5C, 0x6D, 0x5B, 0x87, 0x11, 0xAC, 0x44 },
    "MF8xCzAJBgNVBAYTAlRXMRIwEAYDVQQKDAlUQUlXQU4tQ0ExEDAOBgNVBAsMB1Jv"
    "b3QgQ0ExKjAoBgNVBAMMIVRXQ0EgUm9vdCBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0"
    "eQ==",
    "AQ==",
    nullptr
  },
  {
    // CN=D-TRUST Root Class 3 CA 2 EV 2009,O=D-Trust GmbH,C=DE
    "1.3.6.1.4.1.4788.2.202.1",
    "D-TRUST EV OID",
    SEC_OID_UNKNOWN,
    { 0xEE, 0xC5, 0x49, 0x6B, 0x98, 0x8C, 0xE9, 0x86, 0x25, 0xB9, 0x34,
      0x09, 0x2E, 0xEC, 0x29, 0x08, 0xBE, 0xD0, 0xB0, 0xF3, 0x16, 0xC2,
      0xD4, 0x73, 0x0C, 0x84, 0xEA, 0xF1, 0xF3, 0xD3, 0x48, 0x81 },
    "MFAxCzAJBgNVBAYTAkRFMRUwEwYDVQQKDAxELVRydXN0IEdtYkgxKjAoBgNVBAMM"
    "IUQtVFJVU1QgUm9vdCBDbGFzcyAzIENBIDIgRVYgMjAwOQ==",
    "CYP0",
    nullptr
  },
  {
    // CN=Swisscom Root EV CA 2,OU=Digital Certificate Services,O=Swisscom,C=ch
    "2.16.756.1.83.21.0",
    "Swisscom  EV OID",
    SEC_OID_UNKNOWN,
    { 0xD9, 0x5F, 0xEA, 0x3C, 0xA4, 0xEE, 0xDC, 0xE7, 0x4C, 0xD7, 0x6E,
      0x75, 0xFC, 0x6D, 0x1F, 0xF6, 0x2C, 0x44, 0x1F, 0x0F, 0xA8, 0xBC,
      0x77, 0xF0, 0x34, 0xB1, 0x9E, 0x5D, 0xB2, 0x58, 0x01, 0x5D },
    "MGcxCzAJBgNVBAYTAmNoMREwDwYDVQQKEwhTd2lzc2NvbTElMCMGA1UECxMcRGln"
    "aXRhbCBDZXJ0aWZpY2F0ZSBTZXJ2aWNlczEeMBwGA1UEAxMVU3dpc3Njb20gUm9v"
    "dCBFViBDQSAy",
    "APL6ZOJ0Y9ON/RAdBB92ylg=",
    nullptr
  },
  {
    // CN=VeriSign Universal Root Certification Authority,OU="(c) 2008 VeriSign, Inc. - For authorized use only",OU=VeriSign Trust Network,O="VeriSign, Inc.",C=US
    "2.16.840.1.113733.1.7.23.6",
    "VeriSign EV OID",
    SEC_OID_UNKNOWN,
    { 0x23, 0x99, 0x56, 0x11, 0x27, 0xA5, 0x71, 0x25, 0xDE, 0x8C, 0xEF,
      0xEA, 0x61, 0x0D, 0xDF, 0x2F, 0xA0, 0x78, 0xB5, 0xC8, 0x06, 0x7F,
      0x4E, 0x82, 0x82, 0x90, 0xBF, 0xB8, 0x60, 0xE8, 0x4B, 0x3C },
    "MIG9MQswCQYDVQQGEwJVUzEXMBUGA1UEChMOVmVyaVNpZ24sIEluYy4xHzAdBgNV"
    "BAsTFlZlcmlTaWduIFRydXN0IE5ldHdvcmsxOjA4BgNVBAsTMShjKSAyMDA4IFZl"
    "cmlTaWduLCBJbmMuIC0gRm9yIGF1dGhvcml6ZWQgdXNlIG9ubHkxODA2BgNVBAMT"
    "L1ZlcmlTaWduIFVuaXZlcnNhbCBSb290IENlcnRpZmljYXRpb24gQXV0aG9yaXR5",
    "QBrEZCGzEyEDDrvkEhrFHQ==",
    nullptr
  },
  {
    // CN=GeoTrust Primary Certification Authority - G3,OU=(c) 2008 GeoTrust Inc. - For authorized use only,O=GeoTrust Inc.,C=US
    "1.3.6.1.4.1.14370.1.6",
    "GeoTrust EV OID",
    SEC_OID_UNKNOWN,
    { 0xB4, 0x78, 0xB8, 0x12, 0x25, 0x0D, 0xF8, 0x78, 0x63, 0x5C, 0x2A,
      0xA7, 0xEC, 0x7D, 0x15, 0x5E, 0xAA, 0x62, 0x5E, 0xE8, 0x29, 0x16,
      0xE2, 0xCD, 0x29, 0x43, 0x61, 0x88, 0x6C, 0xD1, 0xFB, 0xD4 },
    "MIGYMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNR2VvVHJ1c3QgSW5jLjE5MDcGA1UE"
    "CxMwKGMpIDIwMDggR2VvVHJ1c3QgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBv"
    "bmx5MTYwNAYDVQQDEy1HZW9UcnVzdCBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0"
    "aG9yaXR5IC0gRzM=",
    "FaxulBmyeUtB9iepwxgPHw==",
    nullptr
  },
  {
    // CN=thawte Primary Root CA - G3,OU="(c) 2008 thawte, Inc. - For authorized use only",OU=Certification Services Division,O="thawte, Inc.",C=US
    "2.16.840.1.113733.1.7.48.1",
    "Thawte EV OID",
    SEC_OID_UNKNOWN,
    { 0x4B, 0x03, 0xF4, 0x58, 0x07, 0xAD, 0x70, 0xF2, 0x1B, 0xFC, 0x2C,
      0xAE, 0x71, 0xC9, 0xFD, 0xE4, 0x60, 0x4C, 0x06, 0x4C, 0xF5, 0xFF,
      0xB6, 0x86, 0xBA, 0xE5, 0xDB, 0xAA, 0xD7, 0xFD, 0xD3, 0x4C },
    "MIGuMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMdGhhd3RlLCBJbmMuMSgwJgYDVQQL"
    "Ex9DZXJ0aWZpY2F0aW9uIFNlcnZpY2VzIERpdmlzaW9uMTgwNgYDVQQLEy8oYykg"
    "MjAwOCB0aGF3dGUsIEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25seTEkMCIG"
    "A1UEAxMbdGhhd3RlIFByaW1hcnkgUm9vdCBDQSAtIEcz",
    "YAGXt0an6rS0mtZLL/eQ+w==",
    nullptr
  },
  {
    // CN = Autoridad de Certificacion Firmaprofesional CIF A62634068, C = ES
    "1.3.6.1.4.1.13177.10.1.3.10",
    "Firmaprofesional EV OID",
    SEC_OID_UNKNOWN,
    { 0x04, 0x04, 0x80, 0x28, 0xBF, 0x1F, 0x28, 0x64, 0xD4, 0x8F, 0x9A,
      0xD4, 0xD8, 0x32, 0x94, 0x36, 0x6A, 0x82, 0x88, 0x56, 0x55, 0x3F,
      0x3B, 0x14, 0x30, 0x3F, 0x90, 0x14, 0x7F, 0x5D, 0x40, 0xEF },
    "MFExCzAJBgNVBAYTAkVTMUIwQAYDVQQDDDlBdXRvcmlkYWQgZGUgQ2VydGlmaWNh"
    "Y2lvbiBGaXJtYXByb2Zlc2lvbmFsIENJRiBBNjI2MzQwNjg=",
    "U+w77vuySF8=",
    nullptr
  },
  {
    // CN = TWCA Global Root CA, OU = Root CA, O = TAIWAN-CA, C = TW
    "1.3.6.1.4.1.40869.1.1.22.3",
    "TWCA EV OID",
    SEC_OID_UNKNOWN,
    { 0x59, 0x76, 0x90, 0x07, 0xF7, 0x68, 0x5D, 0x0F, 0xCD, 0x50, 0x87,
      0x2F, 0x9F, 0x95, 0xD5, 0x75, 0x5A, 0x5B, 0x2B, 0x45, 0x7D, 0x81,
      0xF3, 0x69, 0x2B, 0x61, 0x0A, 0x98, 0x67, 0x2F, 0x0E, 0x1B },
    "MFExCzAJBgNVBAYTAlRXMRIwEAYDVQQKEwlUQUlXQU4tQ0ExEDAOBgNVBAsTB1Jv"
    "b3QgQ0ExHDAaBgNVBAMTE1RXQ0EgR2xvYmFsIFJvb3QgQ0E=",
    "DL4=",
    nullptr
  },
  {
    // CN = E-Tugra Certification Authority, OU = E-Tugra Sertifikasyon Merkezi, O = E-Tuğra EBG Bilişim Teknolojileri ve Hizmetleri A.Ş., L = Ankara, C = TR
    "2.16.792.3.0.4.1.1.4",
    "ETugra EV OID",
    SEC_OID_UNKNOWN,
    { 0xB0, 0xBF, 0xD5, 0x2B, 0xB0, 0xD7, 0xD9, 0xBD, 0x92, 0xBF, 0x5D,
      0x4D, 0xC1, 0x3D, 0xA2, 0x55, 0xC0, 0x2C, 0x54, 0x2F, 0x37, 0x83,
      0x65, 0xEA, 0x89, 0x39, 0x11, 0xF5, 0x5E, 0x55, 0xF2, 0x3C },
    "MIGyMQswCQYDVQQGEwJUUjEPMA0GA1UEBwwGQW5rYXJhMUAwPgYDVQQKDDdFLVR1"
    "xJ9yYSBFQkcgQmlsacWfaW0gVGVrbm9sb2ppbGVyaSB2ZSBIaXptZXRsZXJpIEEu"
    "xZ4uMSYwJAYDVQQLDB1FLVR1Z3JhIFNlcnRpZmlrYXN5b24gTWVya2V6aTEoMCYG"
    "A1UEAwwfRS1UdWdyYSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eQ==",
    "amg+nFGby1M=",
    nullptr
  }
};

static SECOidTag
register_oid(const SECItem* oid_item, const char* oid_name)
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

#ifndef NSS_NO_LIBPKIX
static void
addToCertListIfTrusted(CERTCertList* certList, CERTCertificate* cert) {
  CERTCertTrust nssTrust;
  if (CERT_GetCertTrust(cert, &nssTrust) != SECSuccess) {
    return;
  }
  unsigned int flags = SEC_GET_TRUST_FLAGS(&nssTrust, trustSSL);

  if (flags & CERTDB_TRUSTED_CA) {
    CERT_AddCertToListTail(certList, CERT_DupCertificate(cert));
  }
}
#endif

static bool
isEVPolicy(SECOidTag policyOIDTag)
{
  for (size_t iEV = 0; iEV < PR_ARRAY_SIZE(myTrustedEVInfos); ++iEV) {
    nsMyTrustedEVInfo& entry = myTrustedEVInfos[iEV];
    if (policyOIDTag == entry.oid_tag) {
      return true;
    }
  }

  return false;
}

namespace mozilla { namespace psm {

#ifndef NSS_NO_LIBPKIX
CERTCertList*
GetRootsForOid(SECOidTag oid_tag)
{
  CERTCertList* certList = CERT_NewCertList();
  if (!certList)
    return nullptr;

  for (size_t iEV = 0; iEV < PR_ARRAY_SIZE(myTrustedEVInfos); ++iEV) {
    nsMyTrustedEVInfo& entry = myTrustedEVInfos[iEV];
    if (entry.oid_tag == oid_tag) {
      addToCertListIfTrusted(certList, entry.cert);
    }
  }

  return certList;
}
#endif

bool
CertIsAuthoritativeForEVPolicy(const CERTCertificate* cert,
                               const mozilla::pkix::CertPolicyId& policy)
{
  PR_ASSERT(cert);
  if (!cert) {
    return false;
  }

  for (size_t iEV = 0; iEV < PR_ARRAY_SIZE(myTrustedEVInfos); ++iEV) {
    nsMyTrustedEVInfo& entry = myTrustedEVInfos[iEV];
    if (entry.cert && CERT_CompareCerts(cert, entry.cert)) {
      const SECOidData* oidData = SECOID_FindOIDByTag(entry.oid_tag);
      if (oidData && oidData->oid.len == policy.numBytes &&
          !memcmp(oidData->oid.data, policy.bytes, policy.numBytes)) {
        return true;
      }
    }
  }

  return false;
}

static PRStatus
IdentityInfoInit()
{
  for (size_t iEV = 0; iEV < PR_ARRAY_SIZE(myTrustedEVInfos); ++iEV) {
    nsMyTrustedEVInfo& entry = myTrustedEVInfos[iEV];

    SECStatus rv;
    CERTIssuerAndSN ias;

    rv = ATOB_ConvertAsciiToItem(&ias.derIssuer, const_cast<char*>(entry.issuer_base64));
    PR_ASSERT(rv == SECSuccess);
    if (rv != SECSuccess) {
      return PR_FAILURE;
    }
    rv = ATOB_ConvertAsciiToItem(&ias.serialNumber,
                                 const_cast<char*>(entry.serial_base64));
    PR_ASSERT(rv == SECSuccess);
    if (rv != SECSuccess) {
      SECITEM_FreeItem(&ias.derIssuer, false);
      return PR_FAILURE;
    }

    ias.serialNumber.type = siUnsignedInteger;

    entry.cert = CERT_FindCertByIssuerAndSN(nullptr, &ias);

    SECITEM_FreeItem(&ias.derIssuer, false);
    SECITEM_FreeItem(&ias.serialNumber, false);

    // If an entry is missing in the NSS root database, it may be because the
    // root database is out of sync with what we expect (e.g. a different
    // version of system NSS is installed). We will just silently avoid
    // treating that root cert as EV.
    if (!entry.cert) {
#ifdef DEBUG
      // The debug CA info is at position 0, and is NOT on the NSS root db
      if (iEV == 0) {
        continue;
      }
#endif
      PR_NOT_REACHED("Could not find EV root in NSS storage");
      continue;
    }

    unsigned char certFingerprint[SHA256_LENGTH];
    rv = PK11_HashBuf(SEC_OID_SHA256, certFingerprint,
                      entry.cert->derCert.data,
                      static_cast<int32_t>(entry.cert->derCert.len));
    PR_ASSERT(rv == SECSuccess);
    if (rv == SECSuccess) {
      bool same = !memcmp(certFingerprint, entry.ev_root_sha256_fingerprint,
                          sizeof(certFingerprint));
      PR_ASSERT(same);
      if (same) {

        SECItem ev_oid_item;
        ev_oid_item.data = nullptr;
        ev_oid_item.len = 0;
        rv = SEC_StringToOID(nullptr, &ev_oid_item, entry.dotted_oid, 0);
        PR_ASSERT(rv == SECSuccess);
        if (rv == SECSuccess) {
          entry.oid_tag = register_oid(&ev_oid_item, entry.oid_name);
          if (entry.oid_tag == SEC_OID_UNKNOWN) {
            rv = SECFailure;
          }
          SECITEM_FreeItem(&ev_oid_item, false);
        }
      } else {
        PR_SetError(SEC_ERROR_BAD_DATA, 0);
        rv = SECFailure;
      }
    }

    if (rv != SECSuccess) {
      CERT_DestroyCertificate(entry.cert);
      entry.cert = nullptr;
      entry.oid_tag = SEC_OID_UNKNOWN;
      return PR_FAILURE;
    }
  }

  return PR_SUCCESS;
}

static PRCallOnceType sIdentityInfoCallOnce;

void
EnsureIdentityInfoLoaded()
{
  (void) PR_CallOnce(&sIdentityInfoCallOnce, IdentityInfoInit);
}

void
CleanupIdentityInfo()
{
  for (size_t iEV = 0; iEV < PR_ARRAY_SIZE(myTrustedEVInfos); ++iEV) {
    nsMyTrustedEVInfo &entry = myTrustedEVInfos[iEV];
    if (entry.cert) {
      CERT_DestroyCertificate(entry.cert);
      entry.cert = nullptr;
    }
  }

  memset(&sIdentityInfoCallOnce, 0, sizeof(PRCallOnceType));
}

// Find the first policy OID that is known to be an EV policy OID.
SECStatus
GetFirstEVPolicy(CERTCertificate* cert,
                 /*out*/ mozilla::pkix::CertPolicyId& policy,
                 /*out*/ SECOidTag& policyOidTag)
{
  if (!cert) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  if (cert->extensions) {
    for (int i=0; cert->extensions[i]; i++) {
      const SECItem* oid = &cert->extensions[i]->id;

      SECOidTag oidTag = SECOID_FindOIDTag(oid);
      if (oidTag != SEC_OID_X509_CERTIFICATE_POLICIES)
        continue;

      SECItem* value = &cert->extensions[i]->value;

      CERTCertificatePolicies* policies;
      CERTPolicyInfo** policyInfos;

      policies = CERT_DecodeCertificatePoliciesExtension(value);
      if (!policies)
        continue;

      policyInfos = policies->policyInfos;

      bool found = false;
      while (*policyInfos) {
        const CERTPolicyInfo* policyInfo = *policyInfos++;

        SECOidTag oid_tag = policyInfo->oid;
        if (oid_tag != SEC_OID_UNKNOWN && isEVPolicy(oid_tag)) {
          const SECOidData* oidData = SECOID_FindOIDByTag(oid_tag);
          PR_ASSERT(oidData);
          PR_ASSERT(oidData->oid.data);
          PR_ASSERT(oidData->oid.len > 0);
          PR_ASSERT(oidData->oid.len <= mozilla::pkix::CertPolicyId::MAX_BYTES);
          if (oidData && oidData->oid.data && oidData->oid.len > 0 &&
              oidData->oid.len <= mozilla::pkix::CertPolicyId::MAX_BYTES) {
            policy.numBytes = static_cast<uint16_t>(oidData->oid.len);
            memcpy(policy.bytes, oidData->oid.data, policy.numBytes);
            policyOidTag = oid_tag;
            found = true;
          }
          break;
        }
      }
      CERT_DestroyCertificatePoliciesExtension(policies);
      if (found) {
        return SECSuccess;
      }
    }
  }

  PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
  return SECFailure;
}

} } // namespace mozilla::psm

// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that nsIX509Cert.isBuiltInRoot works as expected. Differs from
// test_cert_isBuiltInRoot.js in that this test uses a preexisting NSS
// certificate DB that already contains some of the certificates in question.
//
// To create the necessary preexisting files, obtain the "GeoTrust Primary
// Certification Authority - G2" certificate and the "Let's Encrypt Authority
// X1" certificate (copied below for reference) and perform the following steps:
//
// `certutil -d . -N` (use an empty password)
// `certutil -d . -A -n "GeoTrust Primary Certification Authority - G2" -t ,, \
//   -a -i GeoTrust.pem`
// `certutil -d . -A -n "Let's Encrypt Authority X1" -t ,, -a \
//   -i LetsEncrypt.pem`
//
// This should create cert8.db and key3.db files for use on non-Android
// platforms. Perform the same steps with "sql:." as the argument to the "-d"
// flag to create cert9.db and key4.db for use with Android.
//
// (The crucial property of the first certificate is that it is a built-in trust
// anchor, so any replacement must also have this property. The second
// certificate is not a built-in trust anchor, so any replacement must not be a
// built-in trust anchor.)
//
// GeoTrust Primary Certification Authority - G2:
// -----BEGIN CERTIFICATE-----
// MIICrjCCAjWgAwIBAgIQPLL0SAoA4v7rJDteYD7DazAKBggqhkjOPQQDAzCBmDEL
// MAkGA1UEBhMCVVMxFjAUBgNVBAoTDUdlb1RydXN0IEluYy4xOTA3BgNVBAsTMChj
// KSAyMDA3IEdlb1RydXN0IEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25seTE2
// MDQGA1UEAxMtR2VvVHJ1c3QgUHJpbWFyeSBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0
// eSAtIEcyMB4XDTA3MTEwNTAwMDAwMFoXDTM4MDExODIzNTk1OVowgZgxCzAJBgNV
// BAYTAlVTMRYwFAYDVQQKEw1HZW9UcnVzdCBJbmMuMTkwNwYDVQQLEzAoYykgMjAw
// NyBHZW9UcnVzdCBJbmMuIC0gRm9yIGF1dGhvcml6ZWQgdXNlIG9ubHkxNjA0BgNV
// BAMTLUdlb1RydXN0IFByaW1hcnkgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkgLSBH
// MjB2MBAGByqGSM49AgEGBSuBBAAiA2IABBWx6P0DFUPlrOuHNxFi79KDNlJ9RVcL
// So17VDs6bl8VAsBQps8lL33KSLjHUGMcKiEIfJo22Av+0SbFWDEwKCXzXV2juLal
// tJLtbCyf691DiaI8S0iRHVDsJt/WYC69IaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAO
// BgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFBVfNVdRVfslsq0DafwBo/q+EVXVMAoG
// CCqGSM49BAMDA2cAMGQCMGSWWaboCd6LuvpaiIjwH5HTRqjySkwCY/tsXzjbLkGT
// qQ7mndwxHLKgpxgceeHHNgIwOlavmnRs9vuD4DPTCF+hnMJbn0bWtsuRBmOiBucz
// rD6ogRLQy7rQkgu2npaqBA+K
// -----END CERTIFICATE-----
//
// Let's Encrypt Authority X1:
// -----BEGIN CERTIFICATE-----
// MIIEqDCCA5CgAwIBAgIRAJgT9HUT5XULQ+dDHpceRL0wDQYJKoZIhvcNAQELBQAw
// PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD
// Ew5EU1QgUm9vdCBDQSBYMzAeFw0xNTEwMTkyMjMzMzZaFw0yMDEwMTkyMjMzMzZa
// MEoxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MSMwIQYDVQQD
// ExpMZXQncyBFbmNyeXB0IEF1dGhvcml0eSBYMTCCASIwDQYJKoZIhvcNAQEBBQAD
// ggEPADCCAQoCggEBAJzTDPBa5S5Ht3JdN4OzaGMw6tc1Jhkl4b2+NfFwki+3uEtB
// BaupnjUIWOyxKsRohwuj43Xk5vOnYnG6eYFgH9eRmp/z0HhncchpDpWRz/7mmelg
// PEjMfspNdxIknUcbWuu57B43ABycrHunBerOSuu9QeU2mLnL/W08lmjfIypCkAyG
// dGfIf6WauFJhFBM/ZemCh8vb+g5W9oaJ84U/l4avsNwa72sNlRZ9xCugZbKZBDZ1
// gGusSvMbkEl4L6KWTyogJSkExnTA0DHNjzE4lRa6qDO4Q/GxH8Mwf6J5MRM9LTb4
// 4/zyM2q5OTHFr8SNDR1kFjOq+oQpttQLwNh9w5MCAwEAAaOCAZIwggGOMBIGA1Ud
// EwEB/wQIMAYBAf8CAQAwDgYDVR0PAQH/BAQDAgGGMH8GCCsGAQUFBwEBBHMwcTAy
// BggrBgEFBQcwAYYmaHR0cDovL2lzcmcudHJ1c3RpZC5vY3NwLmlkZW50cnVzdC5j
// b20wOwYIKwYBBQUHMAKGL2h0dHA6Ly9hcHBzLmlkZW50cnVzdC5jb20vcm9vdHMv
// ZHN0cm9vdGNheDMucDdjMB8GA1UdIwQYMBaAFMSnsaR7LHH62+FLkHX/xBVghYkQ
// MFQGA1UdIARNMEswCAYGZ4EMAQIBMD8GCysGAQQBgt8TAQEBMDAwLgYIKwYBBQUH
// AgEWImh0dHA6Ly9jcHMucm9vdC14MS5sZXRzZW5jcnlwdC5vcmcwPAYDVR0fBDUw
// MzAxoC+gLYYraHR0cDovL2NybC5pZGVudHJ1c3QuY29tL0RTVFJPT1RDQVgzQ1JM
// LmNybDATBgNVHR4EDDAKoQgwBoIELm1pbDAdBgNVHQ4EFgQUqEpqYwR93brm0Tm3
// pkVl7/Oo7KEwDQYJKoZIhvcNAQELBQADggEBANHIIkus7+MJiZZQsY14cCoBG1hd
// v0J20/FyWo5ppnfjL78S2k4s2GLRJ7iD9ZDKErndvbNFGcsW+9kKK/TnY21hp4Dd
// ITv8S9ZYQ7oaoqs7HwhEMY9sibED4aXw09xrJZTC9zK1uIfW6t5dHQjuOWv+HHoW
// ZnupyxpsEUlEaFb+/SCI4KCSBdAsYxAcsHYI5xxEI4LutHp6s3OT2FuO90WfdsIk
// 6q78OMSdn875bNjdBYAqxUp2/LEIHfDBkLoQz0hFJmwAbYahqKaLn73PAAm1X2kj
// f1w8DdnkabOLGeOVcj9LQ+s67vBykx4anTjURkbqZslUEUsn2k5xeua2zUk=
// -----END CERTIFICATE-----

"use strict";

function run_test() {
  const isAndroid = AppConstants.platform == "android";
  const certDBName =  isAndroid ? "cert9.db" : "cert8.db";
  const keyDBName = isAndroid ? "key4.db" : "key3.db";
  let profile = do_get_profile();
  let certDBFile = do_get_file(`test_cert_isBuiltInRoot_reload/${certDBName}`);
  certDBFile.copyTo(profile, certDBName);
  let keyDBFile = do_get_file(`test_cert_isBuiltInRoot_reload/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

  // This is a built-in root, but not one that was added to the preexisting
  // certificate DB.
  const veriSignCertDBKey = `AAAAAAAAAAAAAAAQAAAAzS+A/iOM
    DiIPSGcSKJGHrLMwgcoxCzAJBgNVBAYTAlVTMRcwFQYDVQQKEw5WZXJpU2lnbiwg
    SW5jLjEfMB0GA1UECxMWVmVyaVNpZ24gVHJ1c3QgTmV0d29yazE6MDgGA1UECxMx
    KGMpIDIwMDcgVmVyaVNpZ24sIEluYy4gLSBGb3IgYXV0aG9yaXplZCB1c2Ugb25s
    eTFFMEMGA1UEAxM8VmVyaVNpZ24gQ2xhc3MgMyBQdWJsaWMgUHJpbWFyeSBDZXJ0
    aWZpY2F0aW9uIEF1dGhvcml0eSAtIEc0`;
  let veriSignCert = certdb.findCertByDBKey(veriSignCertDBKey);
  ok(veriSignCert, "Should be able to find VeriSign root");
  ok(veriSignCert.isBuiltInRoot, "VeriSign root is a built-in");

  // This is a built-in root. It was added to the preexisting certificate DB. It
  // should still be considered a built-in.
  const geoTrustCertDBKey = `AAAAAAAAAAAAAAAQAAAAmzyy9EgK
    AOL+6yQ7XmA+w2swgZgxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1HZW9UcnVzdCBJ
    bmMuMTkwNwYDVQQLEzAoYykgMjAwNyBHZW9UcnVzdCBJbmMuIC0gRm9yIGF1dGhv
    cml6ZWQgdXNlIG9ubHkxNjA0BgNVBAMTLUdlb1RydXN0IFByaW1hcnkgQ2VydGlm
    aWNhdGlvbiBBdXRob3JpdHkgLSBHMg==`;
  let geoTrustCert = certdb.findCertByDBKey(geoTrustCertDBKey);
  ok(geoTrustCert, "Should be able to find GeoTrust root");
  ok(geoTrustCert.isBuiltInRoot, "GeoTrust root is a built-in");

  // This is not a built-in root. It was added to the preexisting certificate
  // DB. It should not be considered a built-in root.
  const letsEncryptCertDBKey = `AAAAAAAAAAAAAAARAAAAQQCYE
    /R1E+V1C0PnQx6XHkS9MD8xJDAiBgNVBAoTG0RpZ2l0YWwgU2lnbmF0dXJlIFRyd
    XN0IENvLjEXMBUGA1UEAxMORFNUIFJvb3QgQ0EgWDM=`;
  let letsEncryptCert = certdb.findCertByDBKey(letsEncryptCertDBKey);
  ok(letsEncryptCert, "Should be able to find LetsEncrypt root");
  ok(!letsEncryptCert.isBuiltInRoot, "LetsEncrypt root is not a built-in");
}

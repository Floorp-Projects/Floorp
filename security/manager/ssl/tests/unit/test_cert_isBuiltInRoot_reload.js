// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that nsIX509Cert.isBuiltInRoot works as expected. Differs from
// test_cert_isBuiltInRoot.js in that this test uses a preexisting NSS
// certificate DB that already contains some of the certificates in question.
//
// To create the necessary preexisting files, obtain the "NAVER Global Root
// Certification Authority" certificate and the "Let's Encrypt Authority
// X1" certificate (copied below for reference) and perform the following steps:
//
// `certutil -d . -N` (use an empty password)
// `certutil -d . -A -n "NAVER Global Root Certification Authority" -t ,, \
//   -a -i naverrc1.pem`
// `certutil -d . -A -n "Let's Encrypt Authority X1" -t ,, -a \
//   -i LetsEncrypt.pem`
//
// This should create the cert9.db and key4.db files.
//
// To determine the new DBKey associated to the replacement root,
// one can print builtInRoot.dbKey in test_cert_isBuiltInRoot.js.
//
// (The crucial property of the first certificate is that it is a built-in trust
// anchor, so any replacement must also have this property. The second
// certificate is not a built-in trust anchor, so any replacement must not be a
// built-in trust anchor.)
//
//
// NAVER Global Root Certification Authority:
// -----BEGIN CERTIFICATE-----
// MIIFojCCA4qgAwIBAgIUAZQwHqIL3fXFMyqxQ0Rx+NZQTQ0wDQYJKoZIhvcNAQEM
// BQAwaTELMAkGA1UEBhMCS1IxJjAkBgNVBAoMHU5BVkVSIEJVU0lORVNTIFBMQVRG
// T1JNIENvcnAuMTIwMAYDVQQDDClOQVZFUiBHbG9iYWwgUm9vdCBDZXJ0aWZpY2F0
// aW9uIEF1dGhvcml0eTAeFw0xNzA4MTgwODU4NDJaFw0zNzA4MTgyMzU5NTlaMGkx
// CzAJBgNVBAYTAktSMSYwJAYDVQQKDB1OQVZFUiBCVVNJTkVTUyBQTEFURk9STSBD
// b3JwLjEyMDAGA1UEAwwpTkFWRVIgR2xvYmFsIFJvb3QgQ2VydGlmaWNhdGlvbiBB
// dXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC21PGTXLVA
// iQqrDZBbUGOukJR0F0Vy1ntlWilLp1agS7gvQnXp2XskWjFlqxcX0TM62RHcQDaH
// 38dq6SZeWYp34+hInDEW+j6RscrJo+KfziFTowI2MMtSAuXaMl3Dxeb57hHHi8lE
// HoSTGEq0n+USZGnQJoViAbbJAh2+g1G7XNr4rRVqmfeSVPc0W+m/6imBEtRTkZaz
// kVrd/pBzKPswRrXKCAfHcXLJZtM0l/aM9BhK4dA9WkW2aacp+yPOiNgSnABIqKYP
// szuSjXEOdMWLyEz59JuOuDxp7W87UC9Y7cSw0BwbagzivESq2M0UXZR4Yb8Obtoq
// vC8MC3GmsxY/nOb5zJ9TNeIDoKAYv7vxvvTWjIcNQvcGufFt7QSUqP620wbGQGHf
// nZ3zVHbOUzoBppJB7ASjjw2i1QnK1sua8e9DXcCrpUHPXFNwcMmIpi3Ua2FzUCaG
// YQ5fG8Ir4ozVu53BA0K6lNpfqbDKzE0K70dpAy8i+/Eozr9dUGWokG2zdLAIx6yo
// 0es+nPxdGoMuK8u180SdOqcXYZaicdNwlhVNt0xz7hlcxVs+Qf6sdWA7G2POAN3a
// CJBitOUt7kinaxeZVL6HSuOpXgRM6xBtVNbv8ejyYhbLgGvtPe31HzClrkvJE+2K
// AQHJuFFYwGY6sWZLxNUxAmLpdIQM201GLQIDAQABo0IwQDAdBgNVHQ4EFgQU0p+I
// 36HNLL3s9TsBAZMzJ7LrYEswDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMB
// Af8wDQYJKoZIhvcNAQEMBQADggIBADLKgLOdPVQG3dLSLvCkASELZ0jKbY7gyKoN
// qo0hV4/GPnrK21HUUrPUloSlWGB/5QuOH/XcChWB5Tu2tyIvCZwTFrFsDDUIbatj
// cu3cvuzHV+YwIHHW1xDBE1UBjCpD5EHxzzp6U5LOogMFDTjfArsQLtk70pt6wKGm
// +LUx5vR1yblTmXVHIloUFcd4G7ad6Qz4G3bxhYTeodoS76TiEJd6eN4MUZeoIUCL
// hr0N8F5OSza7OyAfikJW4Qsav3vQIkMsRIz75Sq0bBwcupTgE34h5prCy8VCZLQe
// lHsIJchxzIdFV4XTnyliIoNRlwAYl3dqmJLJfGBs32x9SuRwTMKeuB330DTHD8z7
// p/8Dvq1wkNoL3chtl1+afwkyQf3NosxabUzyqkn+Zvjp2DXrDige7kgvOtB5CTh8
// piKCk5XQA76+AqAF3SAi428diDRgxuYKuQl1C/AH6GmWNcf7I4GOODm4RStDeKLR
// LBT/DShycpWbXgnbiUSYqqFJu3FS8r/2/yehNq+4tneI3TqkbZs0kNwUXTC/t+sX
// 5Ie3cdCh13cV1ELX8vMxmV2b3RZtP+oGI/hGoiLtk/bdmuYqh7GYVPEi92tF4+KO
// dh2ajcQGjTa3FPOdVGm3jjzVpG2Tgbet9r1ke8LJaDmgkpzNNIaRkPpkUZ3+/uul
// 9XXeifdy
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
  const certDBName = "cert9.db";
  const keyDBName = "key4.db";
  let profile = do_get_profile();
  let certDBFile = do_get_file(`test_cert_isBuiltInRoot_reload/${certDBName}`);
  certDBFile.copyTo(profile, certDBName);
  let keyDBFile = do_get_file(`test_cert_isBuiltInRoot_reload/${keyDBName}`);
  keyDBFile.copyTo(profile, keyDBName);

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  // This is a built-in root, but not one that was added to the preexisting
  // certificate DB.
  // Verisign Class 1 Public Primary Certification Authority - G3
  // Certificate fingerprint (SHA1): 204285DCF7EB764195578E136BD4B7D1E98E46A5
  // https://crt.sh/?id=8984570
  const veriSignCertDBKey = `AAAAAAAAAAAAAAARAAAAzQCLW3VWhFSFCwDPrzhI
    zrGkMIHKMQswCQYDVQQGEwJVUzEXMBUGA1UEChMOVmVyaVNpZ24sIEluYy4xHzAdB
    gNVBAsTFlZlcmlTaWduIFRydXN0IE5ldHdvcmsxOjA4BgNVBAsTMShjKSAxOTk5IF
    ZlcmlTaWduLCBJbmMuIC0gRm9yIGF1dGhvcml6ZWQgdXNlIG9ubHkxRTBDBgNVBAM
    TPFZlcmlTaWduIENsYXNzIDEgUHVibGljIFByaW1hcnkgQ2VydGlmaWNhdGlvbiBB
    dXRob3JpdHkgLSBHMw==`;
  let veriSignCert = certdb.findCertByDBKey(veriSignCertDBKey);
  ok(veriSignCert, "Should be able to find VeriSign root");
  ok(veriSignCert.isBuiltInRoot, "VeriSign root is a built-in");

  // This is a built-in root. It was added to the preexisting certificate DB. It
  // should still be considered a built-in.
  const naverCertDBKey = `AAAAAAAAAAAAAAAUAAAAawGUMB6iC93
    1xTMqsUNEcfjWUE0NMGkxCzAJBgNVBAYTAktSMSYwJAYDVQQKDB1OQVZFUiBCVVN
    JTkVTUyBQTEFURk9STSBDb3JwLjEyMDAGA1UEAwwpTkFWRVIgR2xvYmFsIFJvb3Q
    gQ2VydGlmaWNhdGlvbiBBdXRob3JpdHk=`;
  let naverCert = certdb.findCertByDBKey(naverCertDBKey);
  ok(naverCert, "Should be able to find NAVER root");
  ok(naverCert.isBuiltInRoot, "NAVER root is a built-in");

  // This is not a built-in root. It was added to the preexisting certificate
  // DB. It should not be considered a built-in root.
  const letsEncryptCertDBKey = `AAAAAAAAAAAAAAARAAAAQQCYE
    /R1E+V1C0PnQx6XHkS9MD8xJDAiBgNVBAoTG0RpZ2l0YWwgU2lnbmF0dXJlIFRyd
    XN0IENvLjEXMBUGA1UEAxMORFNUIFJvb3QgQ0EgWDM=`;
  let letsEncryptCert = certdb.findCertByDBKey(letsEncryptCertDBKey);
  ok(letsEncryptCert, "Should be able to find LetsEncrypt root");
  ok(!letsEncryptCert.isBuiltInRoot, "LetsEncrypt root is not a built-in");
}

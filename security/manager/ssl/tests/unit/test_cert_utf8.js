// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile();

const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function run_test() {
  const pem =
    "MIIGZjCCBE6gAwIBAgIPB35Sk3vgFeNX8GmMy+wMMA0GCSqGSIb3DQEBBQUAMHsxCzAJBgN" +
    "VBAYTAkNPMUcwRQYDVQQKDD5Tb2NpZWRhZCBDYW1lcmFsIGRlIENlcnRpZmljYWNpw7NuIE" +
    "RpZ2l0YWwgLSBDZXJ0aWPDoW1hcmEgUy5BLjEjMCEGA1UEAwwaQUMgUmHDrXogQ2VydGljw" +
    "6FtYXJhIFMuQS4wHhcNMDYxMTI3MjA0NjI5WhcNMzAwNDAyMjE0MjAyWjB7MQswCQYDVQQG" +
    "EwJDTzFHMEUGA1UECgw+U29jaWVkYWQgQ2FtZXJhbCBkZSBDZXJ0aWZpY2FjacOzbiBEaWd" +
    "pdGFsIC0gQ2VydGljw6FtYXJhIFMuQS4xIzAhBgNVBAMMGkFDIFJhw616IENlcnRpY8OhbW" +
    "FyYSBTLkEuMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAq2uJo1PMSCMI+8PPU" +
    "ZYILrgIem08kBeGqentLhM0R7LQcNzJPNCNyu5LF6vQhbCnIwTLqKL85XXbQMpiiY9QngE9" +
    "JlsYhBzLfDe3fezTf3MZsGqy2IiKLUV0qPezuMDU2s0iiXRNWhU5cxh0T7XrmafBHoi0wpO" +
    "QY5fzp6cSsgkiBzPZkc0OnB8OIMfuuzONj8LSWKdf/WU34ojC2I+GdV75LaeHM/J4Ny+LvB" +
    "2GNzmxlPLYvEqcgxhaBvzz1NS6jBUJJfD5to0EfhcSM2tXSExP2yYe68yQ54v5aHxwD6Mq0" +
    "Do43zeX4lvegGHTgNiRg0JaTASJaBE8rF9ogEHMYELODVoqDA+bMMCm8Ibbq0nXl21Ii/kD" +
    "wFJnmxL3wvIumGVC2daa49AZMQyth9VXAnow6IYm+48jilSH5L887uvDdUhfHjlvgWJsxS3" +
    "EF1QZtzeNnDeRyPYL1epjb4OsOMLzP96a++EjYfDIJss2yKHzMI+ko6Kh3VOz3vCaMh+DkX" +
    "kwwakfU5tTohVTP92dsxA7SH2JD/ztA/X7JWR1DhcZDY8AFmd5ekD8LVkH2ZD6mq093ICK5" +
    "lw1omdMEWux+IBkAC1vImHFrEsm5VoQgpukg3s0956JkSCXjrdCx2bD0Omk1vUgjcTDlaxE" +
    "Cp1bczwmPS9KvqfJpxAe+59QafMCAwEAAaOB5jCB4zAPBgNVHRMBAf8EBTADAQH/MA4GA1U" +
    "dDwEB/wQEAwIBBjAdBgNVHQ4EFgQU0QnQ6dfOeXRU+Tows/RtLAMDG2gwgaAGA1UdIASBmD" +
    "CBlTCBkgYEVR0gADCBiTArBggrBgEFBQcCARYfaHR0cDovL3d3dy5jZXJ0aWNhbWFyYS5jb" +
    "20vZHBjLzBaBggrBgEFBQcCAjBOGkxMaW1pdGFjaW9uZXMgZGUgZ2FyYW507WFzIGRlIGVz" +
    "dGUgY2VydGlmaWNhZG8gc2UgcHVlZGVuIGVuY29udHJhciBlbiBsYSBEUEMuMA0GCSqGSIb" +
    "3DQEBBQUAA4ICAQBclLW4RZFNjmEfAygPU3zmpFmps4p6xbD/CHwso3EcIRNnoZUSQDWDg4" +
    "902zNc8El2CoFS3UnUmjIz75uny3XlesuXEpBcunvFm9+7OSPI/5jOCk0iAUgHforA1SBCl" +
    "ETvv3eiiWdIG0ADBaGJ7M9i4z0ldma/Jre7Ir5v/zlXdLp6yQGVwZVR6Kss+LGGIOk/yzVb" +
    "0hfpKv6DExdA7ohiZVvVO2Dpezy4ydV/NgIlqmjCMRW3MGXrfx1IebHPOeJCgBbT9ZMj/Ey" +
    "XyVo3bHwi2ErN0o42gzmRkBDI8ck1fj+404HGIGQatlDCIaR43NAvO2STdPCWkPHv+wlaNE" +
    "CW8DYSwaN0jJN+Qd53i+yG2dIPPy3RzECiiWZIHiCznCNZc6lEc7wkeZBWN7PGKX6jD/EpO" +
    "e9+XCgycDWs2rjIdWb8m0w5R44bb5tNAlQiM+9hup4phO9OSzNHdpdqy35f/RWmnkJDW2Za" +
    "iogN9xa5P1FlK2Zqi9E4UqLWRhH6/JocdJ6PlwsCT2TG9WjTSy3/pDceiz+/RL5hRqGEPQg" +
    "nTIEgd4kI6mdAXmwIUV80WoyWaM3X94nCHNMyAK9Sy9NgWyo6R35rMDOhYil/SrnhLecUIw" +
    "4OGEfhefwVVdCx/CVxY3UzHCMrr1zZ7Ud3YA47Dx7SwNxkBYn8eNZcLCZDqQ==";
  let cert = gCertDB.constructX509FromBase64(pem);
  // This certificate contains a string that claims to be compatible with UTF-8
  // but isn't. Getting the asn1 structure of it exercises the code path that
  // decodes this value. If we don't assert in debug builds, presumably we
  // handled this value safely.
  notEqual(cert.ASN1Structure, null,
           "accessing nsIX509Cert.ASN1Structure shouldn't assert");

  // This certificate has a number of placeholder byte sequences that we can
  // replace with invalid UTF-8 to ensure that we handle these cases safely.
  let certificateToAlterFile =
    do_get_file("test_cert_utf8/certificateToAlter.pem", false);
  let certificateBytesToAlter =
    atob(pemToBase64(readFile(certificateToAlterFile)));
  testUTF8InField("issuerName", "ISSUER CN", certificateBytesToAlter);
  testUTF8InField("issuerOrganization", "ISSUER O", certificateBytesToAlter);
  testUTF8InField("issuerOrganizationUnit", "ISSUER OU",
                  certificateBytesToAlter);
  testUTF8InField("issuerCommonName", "ISSUER CN", certificateBytesToAlter);
  testUTF8InField("organization", "SUBJECT O", certificateBytesToAlter);
  testUTF8InField("organizationalUnit", "SUBJECT OU", certificateBytesToAlter);
  testUTF8InField("subjectName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField("displayName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField("commonName", "SUBJECT CN", certificateBytesToAlter);
  testUTF8InField("emailAddress", "SUBJECT EMAILADDRESS",
                  certificateBytesToAlter);
  testUTF8InField("subjectAltNames", "SUBJECT ALT DNSNAME",
                  certificateBytesToAlter);
  testUTF8InField("subjectAltNames", "SUBJECT ALT RFC822@NAME",
                  certificateBytesToAlter);
}

// Every (issuer, serial number) pair must be unique. If NSS ever encounters two
// different (in terms of encoding) certificates with the same values for this
// pair, it will refuse to import it (even as a temporary certificate). Since
// we're creating a number of different certificates, we need to ensure this
// pair is always unique. The easiest way to do this is to change the issuer
// distinguished name each time. To make sure this doesn't introduce additional
// UTF8 issues, always use a printable ASCII value.
var gUniqueIssuerCounter = 32;

function testUTF8InField(field, replacementPrefix, certificateBytesToAlter) {
  let toReplace = `${replacementPrefix} REPLACE ME`;
  let replacement = "";
  for (let i = 0; i < toReplace.length; i++) {
    replacement += "\xEB";
  }
  let bytes = certificateBytesToAlter.replace(toReplace, replacement);
  let uniqueIssuerReplacement = "ALWAYS MAKE ME UNIQU" +
                                String.fromCharCode(gUniqueIssuerCounter);
  bytes = bytes.replace("ALWAYS MAKE ME UNIQUE", uniqueIssuerReplacement);
  ok(gUniqueIssuerCounter < 127,
     "should have enough ASCII replacements to make a unique issuer DN");
  gUniqueIssuerCounter++;
  let cert = gCertDB.constructX509(bytes);
  notEqual(cert[field], null, `accessing nsIX509Cert.${field} shouldn't fail`);
  notEqual(cert.ASN1Structure, null,
           "accessing nsIX509Cert.ASN1Structure shouldn't assert");
  notEqual(cert.getEmailAddresses({}), null,
           "calling nsIX509Cert.getEmailAddresses() shouldn't assert");
  ok(!cert.containsEmailAddress("test@test.test"),
     "calling nsIX509Cert.containsEmailAddress() shouldn't assert");
}

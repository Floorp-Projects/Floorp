// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks that various nsIX509Cert attributes correctly handle UTF-8.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function run_test() {
  let cert = certDB.constructX509FromBase64(
    "MIIF3DCCBMSgAwIBAgIEAJiZbzANBgkqhkiG9w0BAQUFADCCAQ0xYTBfBgNVBAMM" +
    "WEkuQ0EgLSBRdWFsaWZpZWQgcm9vdCBjZXJ0aWZpY2F0ZSAoa3ZhbGlmaWtvdmFu" +
    "w70gY2VydGlmaWvDoXQgcG9za3l0b3ZhdGVsZSkgLSBQU0VVRE9OWU0xCzAJBgNV" +
    "BAYTAkNaMS8wLQYDVQQHDCZQb2R2aW5uw70gbWzDvW4gMjE3OC82LCAxOTAgMDAg" +
    "UHJhaGEgOTEsMCoGA1UECgwjUHJ2bsOtIGNlcnRpZmlrYcSNbsOtIGF1dG9yaXRh" +
    "IGEucy4xPDA6BgNVBAsMM0FrcmVkaXRvdmFuw70gcG9za3l0b3ZhdGVsIGNlcnRp" +
    "ZmlrYcSNbsOtY2ggc2x1xb5lYjAeFw0wMjEyMTIxMzMzNDZaFw0wMzEyMTIxMzMz" +
    "NDZaMIIBFDELMAkGA1UEBhMCQ1oxHzAdBgNVBAMeFgBMAHUAZAEbAGsAIABSAGEB" +
    "YQBlAGsxGTAXBgNVBAgeEABWAHkAcwBvAQ0AaQBuAGExLzAtBgNVBAceJgBQAGEA" +
    "YwBvAHYALAAgAE4A4QBkAHIAYQF+AG4A7QAgADcANgA5MSUwIwYJKoZIhvcNAQkB" +
    "FhZsdWRlay5yYXNla0BjZW50cnVtLmN6MRMwEQYDVQQqHgoATAB1AGQBGwBrMQ0w" +
    "CwYDVQQrHgQATABSMR8wHQYDVQQpHhYATAB1AGQBGwBrACAAUgBhAWEAZQBrMRMw" +
    "EQYDVQQEHgoAUgBhAWEAZQBrMRcwFQYDVQQFEw5JQ0EgLSAxMDAwMzc2OTCBnzAN" +
    "BgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAxc7dGd0cNlHZ7tUUl5k30bfYlY3lnOD0" +
    "49JGbTXSt4jNFMRLj6s/777W3kcIdcIwdKxjQULBKgryDvZJ1DAWp2TwzhPDVYj3" +
    "sU4Niqb7mOUcp/4ckteUxGF6FmXtJR9+XHTuLZ+omF9HOUefheBKnXvZuqrLM16y" +
    "nbJn4sPwwdcCAwEAAaOCAbswggG3MCUGA1UdEQQeMBygGgYKKwMGAQQB3BkCAaAM" +
    "DAoxNzYyODk2ODgzMGkGA1UdHwRiMGAwHqAcoBqGGGh0dHA6Ly9xLmljYS5jei9x" +
    "aWNhLmNybDAeoBygGoYYaHR0cDovL2IuaWNhLmN6L3FpY2EuY3JsMB6gHKAahhho" +
    "dHRwOi8vci5pY2EuY3ovcWljYS5jcmwwHwYDVR0jBBgwFoAUK1oKfvvlDYUsZTBy" +
    "vGN701mca/UwHQYDVR0OBBYEFPAs70DB+LS0PnA6niPUfJ5wdQH5MIG4BgNVHSAE" +
    "gbAwga0wgaoGCysGAQQBs2EBAQQEMIGaMC8GCCsGAQUFBwIBFiNodHRwOi8vd3d3" +
    "LmljYS5jei9xY3AvY3BxcGljYTAyLnBkZjBnBggrBgEFBQcCAjBbGllUZW50byBj" +
    "ZXJ0aWZpa2F0IGplIHZ5ZGFuIGpha28gS3ZhbGlmaWtvdmFueSBjZXJ0aWZpa2F0" +
    "IHYgc291bGFkdSBzZSB6YWtvbmVtIDIyNy8yMDAwIFNiLjAYBggrBgEFBQcBAwQM" +
    "MAowCAYGBACORgEBMA4GA1UdDwEB/wQEAwIE8DANBgkqhkiG9w0BAQUFAAOCAQEA" +
    "v2V+nnYYMIgabmmgHx49CtlZIHdGS3TuWKXw130xFhbXDnNhEbx3alaskNsvjQQR" +
    "Lqs1ZwKy58yynse+eJYHqenmHDACpAfVpCF9PXC/mDarVsoQw7NTcUpsAFhSd/zT" +
    "v9jIf3twECyxx/RVzONVcob7nPePESHiKoG4FbtcuUh0wSHvCmTwRIQqPDCIuHcF" +
    "StSt3Jr9iXcbXEhe4mSccOZ8N+r7Rv3ncKcevlRl7uFfDKDTyd43SZeRS/7J8KRf" +
    "hD/h2nawrCFwc5gJW10aLJGFL/mcS7ViAIT9HCVk23j4TuBjsVmnZ0VKxB5edux+" +
    "LIEqtU428UVHZWU/I5ngLw==");

  equal(cert.emailAddress, "ludek.rasek@centrum.cz",
        "Actual and expected emailAddress should match");
  equal(cert.subjectName, "serialNumber=ICA - 10003769,SN=Rašek,name=Luděk Rašek,initials=LR,givenName=Luděk,E=ludek.rasek@centrum.cz,L=\"Pacov, Nádražní 769\",ST=Vysočina,CN=Luděk Rašek,C=CZ",
        "Actual and expected subjectName should match");
  equal(cert.commonName, "Luděk Rašek",
        "Actual and expected commonName should match");
  equal(cert.organization, "",
        "Actual and expected organization should match");
  equal(cert.organizationalUnit, "",
        "Actual and expected organizationalUnit should match");
  equal(cert.displayName, "Luděk Rašek",
        "Actual and expected displayName should match");
  equal(cert.issuerName, "OU=Akreditovaný poskytovatel certifikačních služeb,O=První certifikační autorita a.s.,L=\"Podvinný mlýn 2178/6, 190 00 Praha 9\",C=CZ,CN=I.CA - Qualified root certificate (kvalifikovaný certifikát poskytovatele) - PSEUDONYM",
        "Actual and expected issuerName should match");
  equal(cert.issuerCommonName, "I.CA - Qualified root certificate (kvalifikovaný certifikát poskytovatele) - PSEUDONYM",
        "Actual and expected issuerCommonName should match");
  equal(cert.issuerOrganization, "První certifikační autorita a.s.",
        "Actual and expected issuerOrganization should match");
  equal(cert.issuerOrganizationUnit, "Akreditovaný poskytovatel certifikačních služeb",
        "Actual and expected issuerOrganizationUnit should match");
}

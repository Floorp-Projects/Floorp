// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that we properly decode a certificate with an IPv6 address in its
// subject alternative name extension.

const gCertPEM = `
-----BEGIN CERTIFICATE-----
MIIDCzCCAfOgAwIBAgIUcTqSaVIP2P90g3m+LBEEAvOhnyowDQYJKoZIhvcNAQEL
BQAwLzEtMCsGA1UEAwwkY2VydGlmaWNhdGUgd2l0aCBJUHY2IGFkZHJlc3MgaW4g
U0FOMB4XDTE5MTIwNjE4MjUzOVoXDTIwMTIwNTE4MjUzOVowLzEtMCsGA1UEAwwk
Y2VydGlmaWNhdGUgd2l0aCBJUHY2IGFkZHJlc3MgaW4gU0FOMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA6AWZT04O/XhFN5EjFr2om0tG4XoXlLoyK1Nc
jSY4MNWMkaGnQYUBXY8hsuuIg3eoZmxAd83ZYVPoUlcblRwFhPcvcKJ6Rtvevu3N
/P49Z9SX+MPJbB2Ll7Bk2hJWjOVmn6EJH23mXEc2VZiAE8/HeJ3uYIyMfmkYILBk
EH6PcUSFO4nlvrv7N/TzaCKvXO/pJ/7mYjC+KsaAAbhrhN547ZNUJREg0dClLOXX
vPERAf9Z5oirMKVW0X3+/9SNf4PZvzHYDMk4xSgwfilc1jOIW83hUhBXWSYmBren
9ePLTvcK5qjaNrEzbiz8C4XVn1ghjpE1+W9TDT5hwOzWq4+E0wIDAQABox8wHTAb
BgNVHREEFDAShxAgAR24EAEQARAB/wHAwMDAMA0GCSqGSIb3DQEBCwUAA4IBAQDe
BLodccWrv4pN6q3WkrszPE6F84L7/T/LQtaYdMJFA6aIucoX8ZvNwWo5ZeSTAKhp
I6Nn2uplsnEW2YfKp0ESNZM3CKqqXkngo/iOFXbV/vt1o3dmYySl/c91bQ9O3qbv
Kh4eWTyzuwkBXkRlb8Jde6ue28TgUJ77En1gEYX1c8bokC6c0LkVEDl3Ago4pAol
mJ/Z7UOJvq+9ovJZrHuL60l18lFbBpwRn2EjpWh0Bv/YVfXkxbKiK8w01AEYlCRN
VziQ31eTcqpBaktFR+YDs0i2pYFbksy5P2HNXuUhEcZYfhO2kl13qQkt/ms+6Yvn
hu2lbKsYeZ8R//bzFU+C
-----END CERTIFICATE-----`;

function run_test() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let cert = certdb.constructX509FromBase64(pemToBase64(gCertPEM));
  ok(cert, "should be able to decode certificate");
  equal(
    cert.subjectAltNames,
    "2001:1db8:1001:1001:1001:ff01:c0c0:c0c0",
    "IPv6 address in subject alternative names extension should decode correctly"
  );
}

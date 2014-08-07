// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests specifying a particular client cert to use via the nsISSLSocketControl
// |clientCert| attribute prior to connecting to the server.

function run_test() {
  do_get_profile();

  // Init key token (to prevent password prompt)
  const tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
  let keyToken = tokenDB.getInternalKeyToken();
  if (keyToken.needsUserInit) {
    keyToken.initPassword("");
  }

  // Replace the UI dialog that would prompt for the following PKCS #12 file's
  // password, as well as an alert that appears after it succeeds.
  do_load_manifest("test_client_cert/cert_dialog.manifest");

  // Load the user cert and look it up in XPCOM format
  const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
  let clientCertFile = do_get_file("test_client_cert/client-cert.p12", false);
  certDB.importPKCS12File(null, clientCertFile);

  // Find the cert by its common name
  let clientCert;
  let certs = certDB.getCerts().getEnumerator();
  while (certs.hasMoreElements()) {
    let cert = certs.getNext().QueryInterface(Ci.nsIX509Cert);
    if (cert.certType === Ci.nsIX509Cert.USER_CERT &&
        cert.commonName === "client-cert") {
      clientCert = cert;
      break;
    }
  }
  ok(clientCert, "Client cert found");

  add_tls_server_setup("ClientAuthServer");

  add_connection_test("noclientauth.example.com", Cr.NS_OK);

  add_connection_test("requestclientauth.example.com", Cr.NS_OK);
  add_connection_test("requestclientauth.example.com", Cr.NS_OK,
                      null, null, transport => {
    do_print("Setting client cert on transport");
    let sslSocketControl = transport.securityInfo
                           .QueryInterface(Ci.nsISSLSocketControl);
    sslSocketControl.clientCert = clientCert;
  });

  add_connection_test("requireclientauth.example.com",
                      getXPCOMStatusFromNSS(SSL_ERROR_BAD_CERT_ALERT));
  add_connection_test("requireclientauth.example.com", Cr.NS_OK,
                      null, null, transport => {
    do_print("Setting client cert on transport");
    let sslSocketControl =
      transport.securityInfo.QueryInterface(Ci.nsISSLSocketControl);
    sslSocketControl.clientCert = clientCert;
  });

  run_next_test();
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gBugWindow;

function onLoad() {
  gBugWindow.removeEventListener("load", onLoad);
  gBugWindow.addEventListener("unload", onUnload);
  gBugWindow.close();
}

function onUnload() {
  gBugWindow.removeEventListener("unload", onUnload);
  window.focus();
  finish();
}

// This test opens and then closes the certificate viewer to test that it
// does not crash.
function test() {
  waitForExplicitFinish();
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  // If the certificate with the nickname "pgoca" is ever removed,
  // this will fail. Simply find another certificate. Any one will
  // do.
  let cert = certdb.findCertByNickname(null, "pgoca");
  ok(cert, "found a certificate to look at");
  let arg = {
    QueryInterface: function() this,
    getISupportAtIndex: function() this.cert,
    cert: cert
  };
  gBugWindow = window.openDialog("chrome://pippki/content/certViewer.xul",
                                 "", "", arg);
  gBugWindow.addEventListener("load", onLoad);
}

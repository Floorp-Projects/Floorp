// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// This test connects to ocsp-stapling-none.example.com to test that OCSP
// requests are cancelled if they're taking too long.
// ocsp-stapling-none.example.com doesn't staple an OCSP response, so
// connecting to it will cause a request to the OCSP responder. As with all of
// these tests, the OCSP AIA (i.e. the url of the responder) in the certificate
// is http://localhost:8080. Since this test opens a TCP socket listening on
// port 8080 that just accepts connections and then ignores them (with
// connect/read/write timeouts of 30 seconds), the OCSP requests should cancel
// themselves. When OCSP hard-fail is enabled, connections will be terminated.
// Otherwise, they will succeed.

let gSocketListener = {
  onSocketAccepted: function(serverSocket, socketTransport) {
    socketTransport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 30);
    socketTransport.setTimeout(Ci.nsISocketTransport.TIMEOUT_READ_WRITE, 30);
  },

  onStopListening: function(serverSocket, socketTransport) {}
};

const ua = Cc["@mozilla.org/network/protocol;1?name=http"]
             .getService(Ci.nsIHttpProtocolHandler).userAgent;
const gIsWinXP = ua.indexOf("Windows NT 5.1") != -1;

function run_test() {
  do_get_profile();

  add_tls_server_setup("OCSPStaplingServer");

  let socket = Cc["@mozilla.org/network/server-socket;1"]
                 .createInstance(Ci.nsIServerSocket);
  socket.init(8080, true, -1);
  socket.asyncListen(gSocketListener);

  add_tests_in_mode(true);
  add_tests_in_mode(false);

  add_test(function() { socket.close(); run_next_test(); });
  run_next_test();
}

function add_tests_in_mode(useHardFail) {
  let startTime;
  add_test(function () {
    Services.prefs.setBoolPref("security.OCSP.require", useHardFail);
    startTime = new Date();
    run_next_test();
  });

  add_connection_test("ocsp-stapling-none.example.com", useHardFail
                      ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_SERVER_ERROR)
                      : Cr.NS_OK, clearSessionCache);

  // Reset state
  add_test(function() {
    let endTime = new Date();
    // With OCSP hard-fail on, we timeout after 10 seconds (except if the
    // OS is Windows XP, which occasionally times out too quickly for reasons
    // unknown).
    // With OCSP soft-fail, we timeout after 2 seconds.
    if (useHardFail) {
      do_check_true((endTime - startTime) > 10000 || gIsWinXP);
    } else {
      do_check_true((endTime - startTime) > 2000);
    }
    // Make sure we didn't wait too long.
    // (Unfortunately, we probably can't have a tight upper bound on
    // how long is too long for this test, because we might be running
    // on slow hardware.)
    do_check_true((endTime - startTime) < 60000);
    clearOCSPCache();
    run_next_test();
  });
}

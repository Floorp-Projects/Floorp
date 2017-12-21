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
// is http://localhost:8888. Since this test opens a TCP socket listening on
// port 8888 that just accepts connections and then ignores them (with
// connect/read/write timeouts of 30 seconds), the OCSP requests should cancel
// themselves. When OCSP hard-fail is enabled, connections will be terminated.
// Otherwise, they will succeed.

var gSocketListener = {
  onSocketAccepted(serverSocket, socketTransport) {
    socketTransport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 30);
    socketTransport.setTimeout(Ci.nsISocketTransport.TIMEOUT_READ_WRITE, 30);
  },

  onStopListening(serverSocket, status) {}
};

function run_test() {
  do_get_profile();
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  let socket = Cc["@mozilla.org/network/server-socket;1"]
                 .createInstance(Ci.nsIServerSocket);
  socket.init(8888, true, -1);
  socket.asyncListen(gSocketListener);

  add_one_test(false, "security.OCSP.timeoutMilliseconds.soft", 1000);
  add_one_test(false, "security.OCSP.timeoutMilliseconds.soft", 2000);
  add_one_test(false, "security.OCSP.timeoutMilliseconds.soft", 4000);

  add_one_test(true, "security.OCSP.timeoutMilliseconds.hard", 3000);
  add_one_test(true, "security.OCSP.timeoutMilliseconds.hard", 10000);
  add_one_test(true, "security.OCSP.timeoutMilliseconds.hard", 15000);

  add_test(function() { socket.close(); run_next_test(); });
  run_next_test();
}

function add_one_test(useHardFail, timeoutPrefName, timeoutMilliseconds) {
  let startTime;
  add_test(function () {
    Services.prefs.setBoolPref("security.OCSP.require", useHardFail);
    Services.prefs.setIntPref(timeoutPrefName, timeoutMilliseconds);
    startTime = new Date();
    run_next_test();
  });

  add_connection_test("ocsp-stapling-none.example.com", useHardFail
                      ? SEC_ERROR_OCSP_SERVER_ERROR
                      : PRErrorCodeSuccess, clearSessionCache);

  add_test(function() {
    let endTime = new Date();
    let timeDifference = endTime - startTime;
    info(`useHardFail = ${useHardFail}`);
    info(`startTime = ${startTime.getTime()} (${startTime})`);
    info(`endTime = ${endTime.getTime()} (${endTime})`);
    info(`timeDifference = ${timeDifference}ms`);
    // Date() is not guaranteed to be monotonic, so add extra fuzz time to
    // prevent intermittent failures (this only appeared to be a problem on
    // Windows XP). See Bug 1121117.
    const FUZZ_MS = 300;
    ok(timeDifference + FUZZ_MS > timeoutMilliseconds,
       `OCSP timeout should be ~${timeoutMilliseconds}s for ` +
       `${useHardFail ? "hard" : "soft"}-fail`);
    // Make sure we didn't wait too long.
    // (Unfortunately, we probably can't have a tight upper bound on
    // how long is too long for this test, because we might be running
    // on slow hardware.)
    ok(timeDifference < 60000,
       "Automatic OCSP timeout shouldn't be more than 60s");

    // Reset state
    clearOCSPCache();
    Services.prefs.clearUserPref("security.OCSP.require");
    Services.prefs.clearUserPref(timeoutPrefName);
    run_next_test();
  });
}

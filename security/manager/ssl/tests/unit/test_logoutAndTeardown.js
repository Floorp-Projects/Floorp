/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

do_get_profile();

function connect_and_teardown() {
  let socketTransportService =
    Cc["@mozilla.org/network/socket-transport-service;1"]
      .getService(Ci.nsISocketTransportService);

  let tearDown = false;

  let reader = {
    onInputStreamReady: function(stream) {
      throws(() => stream.available(), /NS_ERROR_FAILURE/,
             "stream should be in an error state");
      ok(tearDown, "A tear down attempt should have occurred");
      run_next_test();
    }
  };

  let sink = {
    onTransportStatus: function(transport, status, progress, progressmax) {
      if (status == Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
        // Try to logout and tear down the secure decoder ring.
        // This should close and stream and notify the reader.
        // The test will time out if this fails.
        tearDown = true;
        Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing)
          .logoutAndTeardown();
      }
    }
  };

  Services.prefs.setCharPref("network.dns.localDomains",
                             "ocsp-stapling-none.example.com");
  let transport = socketTransportService.createTransport(
    ["ssl"], 1, "ocsp-stapling-none.example.com", 8443, null);
  transport.setEventSink(sink, Services.tm.currentThread);

  let inStream = transport.openInputStream(0, 0, 0)
                          .QueryInterface(Ci.nsIAsyncInputStream);
  inStream.asyncWait(reader, Ci.nsIAsyncInputStream.WAIT_CLOSURE_ONLY, 0,
                     Services.tm.currentThread);
}

function run_test() {
  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");
  add_test(connect_and_teardown);
  run_next_test();
}

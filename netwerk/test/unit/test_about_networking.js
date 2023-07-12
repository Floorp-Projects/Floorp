/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);

const gServerSocket = Cc["@mozilla.org/network/server-socket;1"].createInstance(
  Ci.nsIServerSocket
);
const gHttpServer = new HttpServer();

add_test(function test_http() {
  gDashboard.requestHttpConnections(function (data) {
    let found = false;
    for (let i = 0; i < data.connections.length; i++) {
      if (data.connections[i].host == "localhost") {
        found = true;
        break;
      }
    }
    Assert.equal(found, true);

    run_next_test();
  });
});

add_test(function test_dns() {
  gDashboard.requestDNSInfo(function (data) {
    let found = false;
    for (let i = 0; i < data.entries.length; i++) {
      if (data.entries[i].hostname == "localhost") {
        found = true;
        break;
      }
    }
    Assert.equal(found, true);

    do_test_pending();
    gHttpServer.stop(do_test_finished);

    run_next_test();
  });
});

add_test(function test_sockets() {
  // TODO: enable this test in bug 1581892.
  if (mozinfo.socketprocess_networking) {
    info("skip test_sockets");
    run_next_test();
    return;
  }

  let sts = Cc["@mozilla.org/network/socket-transport-service;1"].getService(
    Ci.nsISocketTransportService
  );
  let threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

  let transport = sts.createTransport(
    [],
    "127.0.0.1",
    gServerSocket.port,
    null,
    null
  );
  let listener = {
    onTransportStatus(aTransport, aStatus, aProgress, aProgressMax) {
      if (aStatus == Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
        gDashboard.requestSockets(function (data) {
          gServerSocket.close();
          let found = false;
          for (let i = 0; i < data.sockets.length; i++) {
            if (data.sockets[i].host == "127.0.0.1") {
              found = true;
              break;
            }
          }
          Assert.equal(found, true);

          run_next_test();
        });
      }
    },
  };
  transport.setEventSink(listener, threadManager.currentThread);

  transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
});

function run_test() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // We always resolve localhost as it's hardcoded without the following pref:
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  gHttpServer.start(-1);

  let uri = Services.io.newURI(
    "http://localhost:" + gHttpServer.identity.primaryPort
  );
  let channel = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });

  channel.open();

  gServerSocket.init(-1, true, -1);
  Services.prefs.clearUserPref("network.proxy.allow_hijacking_localhost");

  run_next_test();
}

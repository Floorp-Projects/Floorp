/* -*- Mode: Javasript; indent-tab-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://testing-common/httpd.js");

const gDashboard = Cc['@mozilla.org/network/dashboard;1']
  .getService(Ci.nsIDashboard);

const gServerSocket = Components.classes["@mozilla.org/network/server-socket;1"]
                             .createInstance(Components.interfaces.nsIServerSocket);
const gHttpServer = new HttpServer();

add_test(function test_http() {
  gDashboard.requestHttpConnections(function(data) {
    do_check_neq(data.host.indexOf("localhost"), -1);

    run_next_test();
  });
});

add_test(function test_dns() {
  gDashboard.requestDNSInfo(function(data) {
    do_check_neq(data.hostname.indexOf("localhost"), -1);

    do_test_pending();
    gHttpServer.stop(do_test_finished);

    run_next_test();
  });
});

add_test(function test_sockets() {
  let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
    .getService(Ci.nsISocketTransportService);
  let threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

  let transport = sts.createTransport(null, 0, "127.0.0.1",
                                      gServerSocket.port, null);
  let listener = {
    onTransportStatus: function(aTransport, aStatus, aProgress, aProgressMax) {
      if (aStatus == Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
        gDashboard.requestSockets(function(data) {
          gServerSocket.close();

          do_check_neq(data.host.indexOf("127.0.0.1"), -1);

          run_next_test();
        });
      }
    }
  };
  transport.setEventSink(listener, threadManager.currentThread);

  transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
});

function run_test() {
  let ioService = Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService);

  gHttpServer.start(-1);

  let uri = ioService.newURI("http://localhost:" + gHttpServer.identity.primaryPort,
                             null, null);
  let channel = ioService.newChannelFromURI(uri);

  channel.open();

  gServerSocket.init(-1, true, -1);

  run_next_test();
}


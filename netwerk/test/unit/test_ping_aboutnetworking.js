/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gDashboard = Cc["@mozilla.org/network/dashboard;1"].getService(
  Ci.nsIDashboard
);

function connectionFailed(status) {
  let status_ok = [
    "NS_NET_STATUS_RESOLVING_HOST",
    "NS_NET_STATUS_RESOLVED_HOST",
    "NS_NET_STATUS_CONNECTING_TO",
    "NS_NET_STATUS_CONNECTED_TO",
  ];
  for (let i = 0; i < status_ok.length; i++) {
    if (status == status_ok[i]) {
      return false;
    }
  }

  return true;
}

function test_sockets(serverSocket) {
  // TODO: enable this test in bug 1581892.
  if (mozinfo.socketprocess_networking) {
    info("skip test_sockets");
    do_test_finished();
    return;
  }

  do_test_pending();
  gDashboard.requestSockets(function(data) {
    let index = -1;
    info("requestSockets: " + JSON.stringify(data.sockets));
    for (let i = 0; i < data.sockets.length; i++) {
      if (data.sockets[i].host == "127.0.0.1") {
        index = i;
        break;
      }
    }
    Assert.notEqual(index, -1);
    Assert.equal(data.sockets[index].port, serverSocket.port);
    Assert.equal(data.sockets[index].type, "TCP");

    do_test_finished();
  });
}

function run_test() {
  var ps = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  // disable network changed events to avoid the the risk of having the dns
  // cache getting flushed behind our back
  ps.setBoolPref("network.notify.changed", false);
  // Localhost is hardcoded to loopback and isn't cached, disable that with this pref
  ps.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  registerCleanupFunction(function() {
    ps.clearUserPref("network.notify.changed");
    ps.clearUserPref("network.proxy.allow_hijacking_localhost");
  });

  let serverSocket = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  serverSocket.init(-1, true, -1);

  do_test_pending();
  gDashboard.requestConnection(
    "localhost",
    serverSocket.port,
    "tcp",
    15,
    function(connInfo) {
      if (connInfo.status == "NS_NET_STATUS_CONNECTED_TO") {
        do_test_pending();
        gDashboard.requestDNSInfo(function(data) {
          let found = false;
          info("requestDNSInfo: " + JSON.stringify(data.entries));
          for (let i = 0; i < data.entries.length; i++) {
            if (data.entries[i].hostname == "localhost") {
              found = true;
              break;
            }
          }
          Assert.equal(found, true);

          do_test_finished();
          test_sockets(serverSocket);
        });

        do_test_finished();
      }
      if (connectionFailed(connInfo.status)) {
        do_throw(connInfo.status);
      }
    }
  );
}

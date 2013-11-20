/* -*- Mode: Javasript; indent-tab-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const gDashboard = Cc['@mozilla.org/network/dashboard;1']
  .getService(Ci.nsIDashboard);

function connectionFailed(status) {
  let status_ok = [
                    "NS_NET_STATUS_RESOLVING_HOST"
                    ,"NS_NET_STATUS_RESOLVED_HOST"
                    ,"NS_NET_STATUS_CONNECTING_TO"
                    ,"NS_NET_STATUS_CONNECTED_TO"
                  ];
  for (let i = 0; i < status_ok.length; i++) {
    if (status == status_ok[i]) {
      return false;
    }
  }

  return true;
}

function run_test() {
  let serverSocket = Components.classes["@mozilla.org/network/server-socket;1"]
    .createInstance(Ci.nsIServerSocket);

  serverSocket.init(-1, true, -1);

  do_test_pending();
  gDashboard.requestConnection("localhost", serverSocket.port,
                               "tcp", 15, function(connInfo) {
    if (connInfo.status == "NS_NET_STATUS_CONNECTED_TO") {
      do_test_pending();
      gDashboard.requestDNSInfo(function(data) {
        let found = false;
        for (let i = 0; i < data.entries.length; i++) {
          if (data.entries[i].hostname == "localhost") {
            found = true;
            break;
          }
        }
        do_check_eq(found, true);

        do_test_finished();
      });

      do_test_pending();
      gDashboard.requestSockets(function(data) {
        let index = -1;
        for (let i = 0; i < data.sockets.length; i++) {
          if (data.sockets[i].host == "127.0.0.1") {
            index = i;
            break;
          }
        }
        do_check_neq(index, -1);
        do_check_eq(data.sockets[index].port, serverSocket.port);
        do_check_eq(data.sockets[index].tcp, 1);

        serverSocket.close();

        do_test_finished();
      });

      do_test_finished();
    }
    if (connectionFailed(connInfo.status)) {
      do_throw(connInfo.status);
    }
  });
}


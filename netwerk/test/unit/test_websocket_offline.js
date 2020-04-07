/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// checking to make sure we don't hang as per 1038304
// offline so url isn't impt
var url = "ws://localhost";
var chan;
var offlineStatus;

var listener = {
  onAcknowledge(aContext, aSize) {},
  onBinaryMessageAvailable(aContext, aMsg) {},
  onMessageAvailable(aContext, aMsg) {},
  onServerClose(aContext, aCode, aReason) {},
  onStart(aContext) {
    // onStart is not called when a connection fails
    Assert.ok(false);
  },
  onStop(aContext, aStatusCode) {
    Assert.notEqual(aStatusCode, Cr.NS_OK);
    Services.io.offline = offlineStatus;
    do_test_finished();
  },
};

function run_test() {
  offlineStatus = Services.io.offline;
  Services.io.offline = true;

  try {
    chan = Cc["@mozilla.org/network/protocol;1?name=ws"].createInstance(
      Ci.nsIWebSocketChannel
    );
    chan.initLoadInfo(
      null, // aLoadingNode
      Services.scriptSecurityManager.getSystemPrincipal(),
      null, // aTriggeringPrincipal
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      Ci.nsIContentPolicy.TYPE_WEBSOCKET
    );

    var uri = Services.io.newURI(url);
    chan.asyncOpen(uri, url, 0, listener, null);
    do_test_pending();
  } catch (x) {
    dump("throwing " + x);
    do_throw(x);
  }
}

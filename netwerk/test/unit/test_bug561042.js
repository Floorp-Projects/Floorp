/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

const SERVER_PORT = 8080;
const baseURL = "http://localhost:" + SERVER_PORT + "/";

var cookie = "";
for (let i =0; i < 10000; i++) {
    cookie += " big cookie";
}

var listener = {
  onStartRequest: function (request) {
  },

  onDataAvailable: function (request, stream) {
  },

  onStopRequest: function (request, status) {
      Assert.equal(status, Cr.NS_OK);
      server.stop(do_test_finished);
  },

};

var server = new HttpServer();
function run_test() {
    server.start(SERVER_PORT);
    server.registerPathHandler('/', function(metadata, response) {
        response.setStatusLine(metadata.httpVersion, 200, "OK");
        response.setHeader("Set-Cookie", "BigCookie=" + cookie, false);
        response.write("Hello world");
    });
    var chan = NetUtil.newChannel({uri: baseURL, loadUsingSystemPrincipal: true})
                      .QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(listener);
    do_test_pending();
}

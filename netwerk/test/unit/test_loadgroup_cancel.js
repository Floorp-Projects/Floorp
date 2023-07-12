/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function makeChan(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function request_handler(metadata, response) {
  response.processAsync();
  do_timeout(500, () => {
    const body = "some body once told me...";
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Content-Length", "" + body.length, false);
    response.bodyOutputStream.write(body, body.length);
    response.finish();
  });
}

// This test checks that when canceling a loadgroup by the time the loadgroup's
// groupObserver is sent OnStopRequest for a request, that request has been
// canceled.
add_task(async function test_cancelledInOnStop() {
  let http_server = new HttpServer();
  http_server.registerPathHandler("/test1", request_handler);
  http_server.registerPathHandler("/test2", request_handler);
  http_server.registerPathHandler("/test3", request_handler);
  http_server.start(-1);
  const port = http_server.identity.primaryPort;

  let loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(
    Ci.nsILoadGroup
  );

  let loadListener = {
    onStartRequest: aRequest => {
      info("onStartRequest");
    },
    onStopRequest: (aRequest, aStatusCode) => {
      equal(
        aStatusCode,
        Cr.NS_ERROR_ABORT,
        "aStatusCode must be the cancellation code"
      );
      equal(
        aRequest.status,
        Cr.NS_ERROR_ABORT,
        "aRequest.status must be the cancellation code"
      );
    },
    QueryInterface: ChromeUtils.generateQI([
      "nsIRequestObserver",
      "nsISupportsWeakReference",
    ]),
  };
  loadGroup.groupObserver = loadListener;

  let chan1 = makeChan(`http://localhost:${port}/test1`);
  chan1.loadGroup = loadGroup;
  let chan2 = makeChan(`http://localhost:${port}/test2`);
  chan2.loadGroup = loadGroup;
  let chan3 = makeChan(`http://localhost:${port}/test3`);
  chan3.loadGroup = loadGroup;

  await new Promise(resolve => do_timeout(500, resolve));

  let promises = [
    new Promise(resolve => {
      chan1.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
    }),
    new Promise(resolve => {
      chan2.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
    }),
    new Promise(resolve => {
      chan3.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
    }),
  ];

  loadGroup.cancel(Cr.NS_ERROR_ABORT);

  await Promise.all(promises);

  await new Promise(resolve => {
    http_server.stop(resolve);
  });
});

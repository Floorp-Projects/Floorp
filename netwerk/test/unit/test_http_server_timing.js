/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

"use strict";

const responseServerTiming = [
  { metric: "metric", duration: "123.4", description: "description" },
  { metric: "metric2", duration: "456.78", description: "description1" },
];
const trailerServerTiming = [
  { metric: "metric3", duration: "789.11", description: "description2" },
  { metric: "metric4", duration: "1112.13", description: "description3" },
];

let port;

let server;
add_task(async function setup() {
  server = new NodeHTTPServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  server.registerPathHandler("/", (req, res) => {
    res.setHeader("Content-Type", "text/plain");
    res.setHeader("Content-Length", "12");
    res.setHeader("Transfer-Encoding", "chunked");
    res.setHeader("Trailer", "Server-Timing");
    res.setHeader(
      "Server-Timing",
      "metric; dur=123.4; desc=description, metric2; dur=456.78; desc=description1"
    );
    res.write("data reached");
    res.addTrailers({
      "Server-Timing":
        "metric3; dur=789.11; desc=description2, metric4; dur=1112.13; desc=description3",
    });
    res.end();
  });
  port = server.port();
});

// Test that secure origins can use server-timing, even with plain http
add_task(async function test_localhost_origin() {
  let chan = NetUtil.newChannel({
    uri: `http://localhost:${port}/`,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((request, buffer) => {
        let channel = request.QueryInterface(Ci.nsITimedChannel);
        let headers = channel.serverTiming.QueryInterface(Ci.nsIArray);
        ok(headers.length);

        let expectedResult = responseServerTiming.concat(trailerServerTiming);
        Assert.equal(headers.length, expectedResult.length);

        for (let i = 0; i < expectedResult.length; i++) {
          let header = headers.queryElementAt(i, Ci.nsIServerTiming);
          Assert.equal(header.name, expectedResult[i].metric);
          Assert.equal(header.description, expectedResult[i].description);
          Assert.equal(header.duration, parseFloat(expectedResult[i].duration));
        }
        resolve();
      }, null)
    );
  });
});

// Test that insecure origins can't use server timing.
add_task(async function test_http_non_localhost() {
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.dns.native-is-localhost");
  });

  let chan = NetUtil.newChannel({
    uri: `http://example.org:${port}/`,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((request, buffer) => {
        let channel = request.QueryInterface(Ci.nsITimedChannel);
        let headers = channel.serverTiming.QueryInterface(Ci.nsIArray);
        Assert.equal(headers.length, 0);
        resolve();
      }, null)
    );
  });
});

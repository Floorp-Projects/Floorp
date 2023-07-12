/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

function makeChan(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

let body = "abcd";
function request_handler1(metadata, response) {
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("X-header-first: FIRSTVALUE\r\n");
  response.write("X-header-second: 1; second\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// This handler is for obs-fold
// The line that contains X-header-second starts with a space. As a consequence
// it gets folded into the previous line.
function request_handler2(metadata, response) {
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("X-header-first: FIRSTVALUE\r\n");
  // Note the space at the begining of the line
  response.write(" X-header-second: 1; second\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();
}

add_task(async function test() {
  let http_server = new HttpServer();
  http_server.registerPathHandler("/test1", request_handler1);
  http_server.registerPathHandler("/test2", request_handler2);
  http_server.start(-1);
  const port = http_server.identity.primaryPort;

  let chan1 = makeChan(`http://localhost:${port}/test1`);
  await new Promise(resolve => {
    chan1.asyncOpen(new ChannelListener(resolve));
  });
  equal(chan1.getResponseHeader("X-header-first"), "FIRSTVALUE");
  equal(chan1.getResponseHeader("X-header-second"), "1; second");

  let chan2 = makeChan(`http://localhost:${port}/test2`);
  await new Promise(resolve => {
    chan2.asyncOpen(new ChannelListener(resolve));
  });
  equal(
    chan2.getResponseHeader("X-header-first"),
    "FIRSTVALUE X-header-second: 1; second"
  );
  Assert.throws(
    () => chan2.getResponseHeader("X-header-second"),
    /NS_ERROR_NOT_AVAILABLE/
  );

  await new Promise(resolve => http_server.stop(resolve));
});

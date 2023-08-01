/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Basic functionality test, from the client programmer's POV.
 */

ChromeUtils.defineLazyGetter(this, "port", function () {
  return srv.identity.primaryPort;
});

ChromeUtils.defineLazyGetter(this, "tests", function () {
  return [
    new Test(
      "http://localhost:" + port + "/objHandler",
      null,
      start_objHandler,
      null
    ),
    new Test(
      "http://localhost:" + port + "/functionHandler",
      null,
      start_functionHandler,
      null
    ),
    new Test(
      "http://localhost:" + port + "/nonexistent-path",
      null,
      start_non_existent_path,
      null
    ),
    new Test(
      "http://localhost:" + port + "/lotsOfHeaders",
      null,
      start_lots_of_headers,
      null
    ),
  ];
});

var srv;

function run_test() {
  srv = createServer();

  // base path
  // XXX should actually test this works with a file by comparing streams!
  var path = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  srv.registerDirectory("/", path);

  // register a few test paths
  srv.registerPathHandler("/objHandler", objHandler);
  srv.registerPathHandler("/functionHandler", functionHandler);
  srv.registerPathHandler("/lotsOfHeaders", lotsOfHeadersHandler);

  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}

const HEADER_COUNT = 1000;

// TEST DATA

// common properties *always* appended by server
// or invariants for every URL in paths
function commonCheck(ch) {
  Assert.ok(ch.contentLength > -1);
  Assert.equal(ch.getResponseHeader("connection"), "close");
  Assert.ok(!ch.isNoStoreResponse());
  Assert.ok(!ch.isPrivateResponse());
}

function start_objHandler(ch) {
  commonCheck(ch);

  Assert.equal(ch.responseStatus, 200);
  Assert.ok(ch.requestSucceeded);
  Assert.equal(ch.getResponseHeader("content-type"), "text/plain");
  Assert.equal(ch.responseStatusText, "OK");

  var reqMin = {},
    reqMaj = {},
    respMin = {},
    respMaj = {};
  ch.getRequestVersion(reqMaj, reqMin);
  ch.getResponseVersion(respMaj, respMin);
  Assert.ok(reqMaj.value == respMaj.value && reqMin.value == respMin.value);
}

function start_functionHandler(ch) {
  commonCheck(ch);

  Assert.equal(ch.responseStatus, 404);
  Assert.ok(!ch.requestSucceeded);
  Assert.equal(ch.getResponseHeader("foopy"), "quux-baz");
  Assert.equal(ch.responseStatusText, "Page Not Found");

  var reqMin = {},
    reqMaj = {},
    respMin = {},
    respMaj = {};
  ch.getRequestVersion(reqMaj, reqMin);
  ch.getResponseVersion(respMaj, respMin);
  Assert.ok(reqMaj.value == 1 && reqMin.value == 1);
  Assert.ok(respMaj.value == 1 && respMin.value == 1);
}

function start_non_existent_path(ch) {
  commonCheck(ch);

  Assert.equal(ch.responseStatus, 404);
  Assert.ok(!ch.requestSucceeded);
}

function start_lots_of_headers(ch) {
  commonCheck(ch);

  Assert.equal(ch.responseStatus, 200);
  Assert.ok(ch.requestSucceeded);

  for (var i = 0; i < HEADER_COUNT; i++) {
    Assert.equal(ch.getResponseHeader("X-Header-" + i), "value " + i);
  }
}

// PATH HANDLERS

// /objHandler
var objHandler = {
  handle(metadata, response) {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);

    var body = "Request (slightly reformatted):\n\n";
    body += metadata.method + " " + metadata.path;

    Assert.equal(metadata.port, port);

    if (metadata.queryString) {
      body += "?" + metadata.queryString;
    }

    body += " HTTP/" + metadata.httpVersion + "\n";

    var headEnum = metadata.headers;
    while (headEnum.hasMoreElements()) {
      var fieldName = headEnum
        .getNext()
        .QueryInterface(Ci.nsISupportsString).data;
      body += fieldName + ": " + metadata.getHeader(fieldName) + "\n";
    }

    response.bodyOutputStream.write(body, body.length);
  },
  QueryInterface: ChromeUtils.generateQI(["nsIHttpRequestHandler"]),
};

// /functionHandler
function functionHandler(metadata, response) {
  response.setStatusLine("1.1", 404, "Page Not Found");
  response.setHeader("foopy", "quux-baz", false);

  Assert.equal(metadata.port, port);
  Assert.equal(metadata.host, "localhost");
  Assert.equal(metadata.path.charAt(0), "/");

  var body = "this is text\n";
  response.bodyOutputStream.write(body, body.length);
}

// /lotsOfHeaders
function lotsOfHeadersHandler(request, response) {
  response.setHeader("Content-Type", "text/plain", false);

  for (var i = 0; i < HEADER_COUNT; i++) {
    response.setHeader("X-Header-" + i, "value " + i, false);
  }
}

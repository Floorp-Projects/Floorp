/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Basic functionality test, from the client programmer's POV.
 */

XPCOMUtils.defineLazyGetter(this, "port", function() {
  return srv.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test("http://localhost:" + port + "/objHandler",
          null, start_objHandler, null),
    new Test("http://localhost:" + port + "/functionHandler",
          null, start_functionHandler, null),
    new Test("http://localhost:" + port + "/nonexistent-path",
          null, start_non_existent_path, null),
    new Test("http://localhost:" + port + "/lotsOfHeaders",
          null, start_lots_of_headers, null),
  ];
});

var srv;

function run_test()
{
  srv = createServer();

  // base path
  // XXX should actually test this works with a file by comparing streams!
  var dirServ = Cc["@mozilla.org/file/directory_service;1"]
                  .getService(Ci.nsIProperties);
  var path = dirServ.get("CurProcD", Ci.nsILocalFile);
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
function commonCheck(ch)
{
  do_check_true(ch.contentLength > -1);
  do_check_eq(ch.getResponseHeader("connection"), "close");
  do_check_false(ch.isNoStoreResponse());
}

function start_objHandler(ch, cx)
{
  commonCheck(ch);

  do_check_eq(ch.responseStatus, 200);
  do_check_true(ch.requestSucceeded);
  do_check_eq(ch.getResponseHeader("content-type"), "text/plain");
  do_check_eq(ch.responseStatusText, "OK");

  var reqMin = {}, reqMaj = {}, respMin = {}, respMaj = {};
  ch.getRequestVersion(reqMaj, reqMin);
  ch.getResponseVersion(respMaj, respMin);
  do_check_true(reqMaj.value == respMaj.value &&
                reqMin.value == respMin.value);
}

function start_functionHandler(ch, cx)
{
  commonCheck(ch);

  do_check_eq(ch.responseStatus, 404);
  do_check_false(ch.requestSucceeded);
  do_check_eq(ch.getResponseHeader("foopy"), "quux-baz");
  do_check_eq(ch.responseStatusText, "Page Not Found");

  var reqMin = {}, reqMaj = {}, respMin = {}, respMaj = {};
  ch.getRequestVersion(reqMaj, reqMin);
  ch.getResponseVersion(respMaj, respMin);
  do_check_true(reqMaj.value == 1 && reqMin.value == 1);
  do_check_true(respMaj.value == 1 && respMin.value == 1);
}

function start_non_existent_path(ch, cx)
{
  commonCheck(ch);

  do_check_eq(ch.responseStatus, 404);
  do_check_false(ch.requestSucceeded);
}

function start_lots_of_headers(ch, cx)
{
  commonCheck(ch);

  do_check_eq(ch.responseStatus, 200);
  do_check_true(ch.requestSucceeded);

  for (var i = 0; i < HEADER_COUNT; i++)
    do_check_eq(ch.getResponseHeader("X-Header-" + i), "value " + i);
}

// PATH HANDLERS

// /objHandler
var objHandler =
  {
    handle: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/plain", false);

      var body = "Request (slightly reformatted):\n\n";
      body += metadata.method + " " + metadata.path;

      do_check_eq(metadata.port, port);

      if (metadata.queryString)
        body +=  "?" + metadata.queryString;

      body += " HTTP/" + metadata.httpVersion + "\n";

      var headEnum = metadata.headers;
      while (headEnum.hasMoreElements())
      {
        var fieldName = headEnum.getNext()
                                .QueryInterface(Ci.nsISupportsString)
                                .data;
        body += fieldName + ": " + metadata.getHeader(fieldName) + "\n";
      }

      response.bodyOutputStream.write(body, body.length);
    },
    QueryInterface: function(id)
    {
      if (id.equals(Ci.nsISupports) || id.equals(Ci.nsIHttpRequestHandler))
        return this;
      throw Cr.NS_ERROR_NOINTERFACE;
    }
  };

// /functionHandler
function functionHandler(metadata, response)
{
  response.setStatusLine("1.1", 404, "Page Not Found");
  response.setHeader("foopy", "quux-baz", false);

  do_check_eq(metadata.port, port);
  do_check_eq(metadata.host, "localhost");
  do_check_eq(metadata.path.charAt(0), "/");

  var body = "this is text\n";
  response.bodyOutputStream.write(body, body.length);
}

// /lotsOfHeaders
function lotsOfHeadersHandler(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);

  for (var i = 0; i < HEADER_COUNT; i++)
    response.setHeader("X-Header-" + i, "value " + i, false);
}

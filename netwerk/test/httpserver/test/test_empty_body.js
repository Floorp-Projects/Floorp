/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// in its original incarnation, the server didn't like empty response-bodies;
// see the comment in _end for details

var srv;

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test("http://localhost:" + srv.identity.primaryPort + "/empty-body-unwritten",
             null, ensureEmpty, null),
    new Test("http://localhost:" + srv.identity.primaryPort + "/empty-body-written",
             null, ensureEmpty, null),
  ];
});

function run_test()
{
  srv = createServer();

  // register a few test paths
  srv.registerPathHandler("/empty-body-unwritten", emptyBodyUnwritten);
  srv.registerPathHandler("/empty-body-written", emptyBodyWritten);

  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}

// TEST DATA

function ensureEmpty(ch, cx)
{
  do_check_true(ch.contentLength == 0);
}

// PATH HANDLERS

// /empty-body-unwritten
function emptyBodyUnwritten(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
}

// /empty-body-written
function emptyBodyWritten(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
  var body = "";
  response.bodyOutputStream.write(body, body.length);
}

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// test that special headers are sent as an array of headers with the same name

var srv;

function run_test()
{
  srv;

  srv = createServer();
  srv.registerPathHandler("/path-handler", pathHandler);
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}


/************
 * HANDLERS *
 ************/

function pathHandler(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  response.setHeader("Proxy-Authenticate", "First line 1", true);
  response.setHeader("Proxy-Authenticate", "Second line 1", true);
  response.setHeader("Proxy-Authenticate", "Third line 1", true);

  response.setHeader("WWW-Authenticate", "Not merged line 1", false);
  response.setHeader("WWW-Authenticate", "Not merged line 2", true);

  response.setHeader("WWW-Authenticate", "First line 2", false);
  response.setHeader("WWW-Authenticate", "Second line 2", true);
  response.setHeader("WWW-Authenticate", "Third line 2", true);

  response.setHeader("X-Single-Header-Merge", "Single 1", true);
  response.setHeader("X-Single-Header-Merge", "Single 2", true);
}

/***************
 * BEGIN TESTS *
 ***************/

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test("http://localhost:" + srv.identity.primaryPort + "/path-handler",
             null, check)
  ];
});

function check(ch, cx)
{
  var headerValue;

  headerValue = ch.getResponseHeader("Proxy-Authenticate");
  do_check_eq(headerValue, "First line 1\nSecond line 1\nThird line 1");
  headerValue = ch.getResponseHeader("WWW-Authenticate");
  do_check_eq(headerValue, "First line 2\nSecond line 2\nThird line 2");
  headerValue = ch.getResponseHeader("X-Single-Header-Merge");
  do_check_eq(headerValue, "Single 1,Single 2");
}

/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Verify the presence of explicit QueryInterface methods on XPCOM objects
 * exposed by httpd.js, rather than allowing QueryInterface to be implicitly
 * created by XPConnect.
 */

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test("http://localhost:" + srv.identity.primaryPort + "/test",
        null, start_test, null),
    new Test("http://localhost:" + srv.identity.primaryPort + "/sjs/qi.sjs",
        null, start_sjs_qi, null),
  ];
});

var srv;

function run_test()
{
  srv = createServer();

  var qi;
  try
  {
    qi = srv.identity.QueryInterface(Ci.nsIHttpServerIdentity);
  }
  catch (e)
  {
    var exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    do_throw("server identity didn't QI: " + exstr);
    return;
  }

  srv.registerPathHandler("/test", testHandler);
  srv.registerDirectory("/", do_get_file("data/"));
  srv.registerContentType("sjs", "sjs");
  srv.start(-1);

  runHttpTests(tests, testComplete(srv));
}


// TEST DATA

function start_test(ch, cx)
{
  do_check_eq(ch.responseStatusText, "QI Tests Passed");
  do_check_eq(ch.responseStatus, 200);
}

function start_sjs_qi(ch, cx)
{
  do_check_eq(ch.responseStatusText, "SJS QI Tests Passed");
  do_check_eq(ch.responseStatus, 200);
}


function testHandler(request, response)
{
  var exstr;
  var qid;

  response.setStatusLine(request.httpVersion, 500, "FAIL");

  var passed = false;
  try
  {
    qid = request.QueryInterface(Ci.nsIHttpRequest);
    passed = qid === request;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "request doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "request QI'd wrongly?");
    return;
  }

  passed = false;
  try
  {
    qid = response.QueryInterface(Ci.nsIHttpResponse);
    passed = qid === response;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "response doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "response QI'd wrongly?");
    return;
  }

  response.setStatusLine(request.httpVersion, 200, "QI Tests Passed");
}

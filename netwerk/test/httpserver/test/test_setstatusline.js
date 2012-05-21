/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// exercise nsIHttpResponse.setStatusLine, ensure its atomicity, and ensure the
// specified behavior occurs if it's not called

var srv;

function run_test()
{
  srv = createServer();

  srv.registerPathHandler("/no/setstatusline", noSetstatusline);
  srv.registerPathHandler("/http1_0", http1_0);
  srv.registerPathHandler("/http1_1", http1_1);
  srv.registerPathHandler("/invalidVersion", invalidVersion);
  srv.registerPathHandler("/invalidStatus", invalidStatus);
  srv.registerPathHandler("/invalidDescription", invalidDescription);
  srv.registerPathHandler("/crazyCode", crazyCode);
  srv.registerPathHandler("/nullVersion", nullVersion);

  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


/*************
 * UTILITIES *
 *************/

function checkStatusLine(channel, httpMaxVer, httpMinVer, httpCode, statusText)
{
  do_check_eq(channel.responseStatus, httpCode);
  do_check_eq(channel.responseStatusText, statusText);

  var respMaj = {}, respMin = {};
  channel.getResponseVersion(respMaj, respMin);
  do_check_eq(respMaj.value, httpMaxVer);
  do_check_eq(respMin.value, httpMinVer);
}


/*********
 * TESTS *
 *********/

var tests = [];
var test;

// /no/setstatusline
function noSetstatusline(metadata, response)
{
}
test = new Test("http://localhost:4444/no/setstatusline",
                null, startNoSetStatusLine, stop);
tests.push(test);
function startNoSetStatusLine(ch, cx)
{
  checkStatusLine(ch, 1, 1, 200, "OK");
}
function stop(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
}


// /http1_0
function http1_0(metadata, response)
{
  response.setStatusLine("1.0", 200, "OK");
}
test = new Test("http://localhost:4444/http1_0",
                null, startHttp1_0, stop);
tests.push(test);
function startHttp1_0(ch, cx)
{
  checkStatusLine(ch, 1, 0, 200, "OK");
}


// /http1_1
function http1_1(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
}
test = new Test("http://localhost:4444/http1_1",
                null, startHttp1_1, stop);
tests.push(test);
function startHttp1_1(ch, cx)
{
  checkStatusLine(ch, 1, 1, 200, "OK");
}


// /invalidVersion
function invalidVersion(metadata, response)
{
  try
  {
    response.setStatusLine(" 1.0", 200, "FAILED");
  }
  catch (e)
  {
    response.setHeader("Passed", "true", false);
  }
}
test = new Test("http://localhost:4444/invalidVersion",
                null, startPassedTrue, stop);
tests.push(test);
function startPassedTrue(ch, cx)
{
  checkStatusLine(ch, 1, 1, 200, "OK");
  do_check_eq(ch.getResponseHeader("Passed"), "true");
}


// /invalidStatus
function invalidStatus(metadata, response)
{
  try
  {
    response.setStatusLine("1.0", 1000, "FAILED");
  }
  catch (e)
  {
    response.setHeader("Passed", "true", false);
  }
}
test = new Test("http://localhost:4444/invalidStatus",
                null, startPassedTrue, stop);
tests.push(test);


// /invalidDescription
function invalidDescription(metadata, response)
{
  try
  {
    response.setStatusLine("1.0", 200, "FAILED\x01");
  }
  catch (e)
  {
    response.setHeader("Passed", "true", false);
  }
}
test = new Test("http://localhost:4444/invalidDescription",
                null, startPassedTrue, stop);
tests.push(test);


// /crazyCode
function crazyCode(metadata, response)
{
  response.setStatusLine("1.1", 617, "Crazy");
}
test = new Test("http://localhost:4444/crazyCode",
                null, startCrazy, stop);
tests.push(test);
function startCrazy(ch, cx)
{
  checkStatusLine(ch, 1, 1, 617, "Crazy");
}


// /nullVersion
function nullVersion(metadata, response)
{
  response.setStatusLine(null, 255, "NULL");
}
test = new Test("http://localhost:4444/nullVersion",
                null, startNullVersion, stop);
tests.push(test);
function startNullVersion(ch, cx)
{
  // currently, this server implementation defaults to 1.1
  checkStatusLine(ch, 1, 1, 255, "NULL");
}

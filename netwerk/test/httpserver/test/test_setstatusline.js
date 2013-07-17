/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// exercise nsIHttpResponse.setStatusLine, ensure its atomicity, and ensure the
// specified behavior occurs if it's not called

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + srv.identity.primaryPort;
});

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

  srv.start(-1);

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

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test(URL + "/no/setstatusline", null, startNoSetStatusLine, stop),
    new Test(URL + "/http1_0", null, startHttp1_0, stop),
    new Test(URL + "/http1_1", null, startHttp1_1, stop),
    new Test(URL + "/invalidVersion", null, startPassedTrue, stop),
    new Test(URL + "/invalidStatus", null, startPassedTrue, stop),
    new Test(URL + "/invalidDescription", null, startPassedTrue, stop),
    new Test(URL + "/crazyCode", null, startCrazy, stop),
    new Test(URL + "/nullVersion", null, startNullVersion, stop)
  ];
});


// /no/setstatusline
function noSetstatusline(metadata, response)
{
}
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
function startHttp1_0(ch, cx)
{
  checkStatusLine(ch, 1, 0, 200, "OK");
}


// /http1_1
function http1_1(metadata, response)
{
  response.setStatusLine("1.1", 200, "OK");
}
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


// /crazyCode
function crazyCode(metadata, response)
{
  response.setStatusLine("1.1", 617, "Crazy");
}
function startCrazy(ch, cx)
{
  checkStatusLine(ch, 1, 1, 617, "Crazy");
}


// /nullVersion
function nullVersion(metadata, response)
{
  response.setStatusLine(null, 255, "NULL");
}
function startNullVersion(ch, cx)
{
  // currently, this server implementation defaults to 1.1
  checkStatusLine(ch, 1, 1, 255, "NULL");
}

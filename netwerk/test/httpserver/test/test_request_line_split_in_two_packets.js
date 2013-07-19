/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that even when an incoming request's data for the Request-Line doesn't
 * all fit in a single onInputStreamReady notification, the request is handled
 * properly.
 */

var srv = createServer();
srv.start(-1);
const PORT = srv.identity.primaryPort;

function run_test()
{
  srv.registerPathHandler("/lots-of-leading-blank-lines",
                          lotsOfLeadingBlankLines);
  srv.registerPathHandler("/very-long-request-line",
                          veryLongRequestLine);

  runRawTests(tests, testComplete(srv));
}


/***************
 * BEGIN TESTS *
 ***************/

var test, data, str;
var tests = [];


function veryLongRequestLine(request, response)
{
  writeDetails(request, response);
  response.setStatusLine(request.httpVersion, 200, "TEST PASSED");
}

var path = "/very-long-request-line?";
var reallyLong = "0123456789ABCDEF0123456789ABCDEF"; // 32
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 128
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 512
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 2048
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 8192
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 32768
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 131072
reallyLong = reallyLong + reallyLong + reallyLong + reallyLong; // 524288
if (reallyLong.length !== 524288)
  throw new TypeError("generated length not as long as expected");
str = "GET /very-long-request-line?" + reallyLong + " HTTP/1.1\r\n" +
      "Host: localhost:" + PORT + "\r\n" +
      "\r\n";
data = [];
for (var i = 0; i < str.length; i += 16384)
  data.push(str.substr(i, 16384));

function checkVeryLongRequestLine(data)
{
  var iter = LineIterator(data);

  print("data length: " + data.length);
  print("iter object: " + iter);

  // Status-Line
  do_check_eq(iter.next(), "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /very-long-request-line",
     "Query:   " + reallyLong,
     "Version: 1.1",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    " + PORT,
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, checkVeryLongRequestLine),
tests.push(test);


function lotsOfLeadingBlankLines(request, response)
{
  writeDetails(request, response);
  response.setStatusLine(request.httpVersion, 200, "TEST PASSED");
}

var blankLines = "\r\n";
for (var i = 0; i < 14; i++)
  blankLines += blankLines;
str = blankLines +
      "GET /lots-of-leading-blank-lines HTTP/1.1\r\n" +
      "Host: localhost:" + PORT + "\r\n" +
      "\r\n";
data = [];
for (var i = 0; i < str.length; i += 100)
  data.push(str.substr(i, 100));

function checkLotsOfLeadingBlankLines(data)
{
  var iter = LineIterator(data);

  // Status-Line
  print("data length: " + data.length);
  print("iter object: " + iter);

  do_check_eq(iter.next(), "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /lots-of-leading-blank-lines",
     "Query:   ",
     "Version: 1.1",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    " + PORT,
    ];

  expectLines(iter, body);
}

test = new RawTest("localhost", PORT, data, checkLotsOfLeadingBlankLines),
tests.push(test);

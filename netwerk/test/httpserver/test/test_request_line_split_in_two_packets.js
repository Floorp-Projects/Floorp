/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Tests that even when an incoming request's data for the Request-Line doesn't
 * all fit in a single onInputStreamReady notification, the request is handled
 * properly.
 */

const PORT = 4444;

var srv;

function run_test()
{
  srv = createServer();
  srv.registerPathHandler("/lots-of-leading-blank-lines",
                          lotsOfLeadingBlankLines);
  srv.registerPathHandler("/very-long-request-line",
                          veryLongRequestLine);
  srv.start(PORT);

  runRawTests(tests, function() { srv.stop(); });
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
var gibberish = "dfsasdbfjkbnsldkjnewiunfasjkn";
for (var i = 0; i < 10; i++)
  gibberish += gibberish;
str = "GET /very-long-request-line?" + gibberish + " HTTP/1.1\r\n" +
      "Host: localhost:4444\r\n" +
      "\r\n";
data = [];
for (var i = 0; i < str.length; i += 50)
  data.push(str.substr(i, 50));
function checkVeryLongRequestLine(data)
{
  var iter = LineIterator(data);

  // Status-Line
  do_check_eq(iter.next(), "HTTP/1.1 200 TEST PASSED");

  skipHeaders(iter);

  // Okay, next line must be the data we expected to be written
  var body =
    [
     "Method:  GET",
     "Path:    /very-long-request-line",
     "Query:   " + gibberish,
     "Version: 1.1",
     "Scheme:  http",
     "Host:    localhost",
     "Port:    4444",
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
      "Host: localhost:4444\r\n" +
      "\r\n";
data = [];
for (var i = 0; i < str.length; i += 100)
  data.push(str.substr(i, 100));
function checkLotsOfLeadingBlankLines(data)
{
  var iter = LineIterator(data);

  // Status-Line
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
     "Port:    4444",
    ];

  expectLines(iter, body);
}
test = new RawTest("localhost", PORT, data, checkLotsOfLeadingBlankLines),
tests.push(test);

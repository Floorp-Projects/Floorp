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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

/*
 * Tests that the seizePower API works correctly.
 */

const PORT = 4444;

var srv;

function run_test()
{
  srv = createServer();

  srv.registerPathHandler("/raw-data", handleRawData);
  srv.registerPathHandler("/called-too-late", handleTooLate);
  srv.registerPathHandler("/exceptions", handleExceptions);
  srv.registerPathHandler("/async-seizure", handleAsyncSeizure);
  srv.registerPathHandler("/seize-after-async", handleSeizeAfterAsync);

  srv.start(PORT);

  runRawTests(tests, testComplete(srv));
}


function checkException(fun, err, msg)
{
  try
  {
    fun();
  }
  catch (e)
  {
    if (e !== err && e.result !== err)
      do_throw(msg);
    return;
  }
  do_throw(msg);
}

function callASAPLater(fun)
{
  gThreadManager.currentThread.dispatch({
    run: function()
    {
      fun();
    }
  }, Ci.nsIThread.DISPATCH_NORMAL);
}


/*****************
 * PATH HANDLERS *
 *****************/

function handleRawData(request, response)
{
  response.seizePower();
  response.write("Raw data!");
  response.finish();
}

function handleTooLate(request, response)
{
  response.write("DO NOT WANT");
  var output = response.bodyOutputStream;

  response.seizePower();

  if (response.bodyOutputStream !== output)
    response.write("bodyOutputStream changed!");
  else
    response.write("too-late passed");
  response.finish();
}

function handleExceptions(request, response)
{
  response.seizePower();
  checkException(function() { response.setStatusLine("1.0", 500, "ISE"); },
                 Cr.NS_ERROR_NOT_AVAILABLE,
                 "setStatusLine should throw not-available after seizePower");
  checkException(function() { response.setHeader("X-Fail", "FAIL", false); },
                 Cr.NS_ERROR_NOT_AVAILABLE,
                 "setHeader should throw not-available after seizePower");
  checkException(function() { response.processAsync(); },
                 Cr.NS_ERROR_NOT_AVAILABLE,
                 "processAsync should throw not-available after seizePower");
  var out = response.bodyOutputStream;
  var data = "exceptions test passed";
  out.write(data, data.length);
  response.seizePower(); // idempotency test of seizePower
  response.finish();
  response.finish(); // idempotency test of finish after seizePower
  checkException(function() { response.seizePower(); },
                 Cr.NS_ERROR_UNEXPECTED,
                 "seizePower should throw unexpected after finish");
}

function handleAsyncSeizure(request, response)
{
  response.seizePower();
  callLater(1, function()
  {
    response.write("async seizure passed");
    response.bodyOutputStream.close();
    callLater(1, function()
    {
      response.finish();
    });
  });
}

function handleSeizeAfterAsync(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "async seizure pass");
  response.processAsync();
  checkException(function() { response.seizePower(); },
                 Cr.NS_ERROR_NOT_AVAILABLE,
                 "seizePower should throw not-available after processAsync");
  callLater(1, function()
  {
    response.finish();
  });
}


/***************
 * BEGIN TESTS *
 ***************/

var test, data;
var tests = [];

data = "GET /raw-data HTTP/1.0\r\n" +
       "\r\n";
function checkRawData(data)
{
  do_check_eq(data, "Raw data!");
}
test = new RawTest("localhost", PORT, data, checkRawData),
tests.push(test);

data = "GET /called-too-late HTTP/1.0\r\n" +
       "\r\n";
function checkTooLate(data)
{
  do_check_eq(LineIterator(data).next(), "too-late passed");
}
test = new RawTest("localhost", PORT, data, checkTooLate),
tests.push(test);

data = "GET /exceptions HTTP/1.0\r\n" +
       "\r\n";
function checkExceptions(data)
{
  do_check_eq("exceptions test passed", data);
}
test = new RawTest("localhost", PORT, data, checkExceptions),
tests.push(test);

data = "GET /async-seizure HTTP/1.0\r\n" +
       "\r\n";
function checkAsyncSeizure(data)
{
  do_check_eq(data, "async seizure passed");
}
test = new RawTest("localhost", PORT, data, checkAsyncSeizure),
tests.push(test);

data = "GET /seize-after-async HTTP/1.0\r\n" +
       "\r\n";
function checkSeizeAfterAsync(data)
{
  do_check_eq(LineIterator(data).next(), "HTTP/1.0 200 async seizure pass");
}
test = new RawTest("localhost", PORT, data, checkSeizeAfterAsync),
tests.push(test);

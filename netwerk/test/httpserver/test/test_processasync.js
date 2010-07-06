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
 *   Jeff Walden <jwalden+code@mit.edu>
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
 * Tests for correct behavior of asynchronous responses.
 */

const PORT = 4444;
const PREPATH = "http://localhost:" + PORT;

function run_test()
{
  var srv = createServer();
  for (var path in handlers)
    srv.registerPathHandler(path, handlers[path]);
  srv.start(PORT);

  runHttpTests(tests, testComplete(srv));
}


/***************
 * BEGIN TESTS *
 ***************/

var test;
var tests = [];
var handlers = {};

function handleSync(request, response)
{
  response.setStatusLine(request.httpVersion, 500, "handleSync fail");

  try
  {
    response.finish();
    do_throw("finish called on sync response");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_UNEXPECTED);
  }

  response.setStatusLine(request.httpVersion, 200, "handleSync pass");
}
handlers["/handleSync"] = handleSync;

function start_handleSync(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.responseStatusText, "handleSync pass");
}

test = new Test(PREPATH + "/handleSync",
                null, start_handleSync, null),
tests.push(test);


function handleAsync1(request, response)
{
  response.setStatusLine(request.httpVersion, 500, "Old status line!");
  response.setHeader("X-Foo", "old value", false);

  response.processAsync();

  response.setStatusLine(request.httpVersion, 200, "New status line!");
  response.setHeader("X-Foo", "new value", false);

  response.finish();

  try
  {
    response.setStatusLine(request.httpVersion, 500, "Too late!");
    do_throw("late setStatusLine didn't throw");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }

  try
  {
    response.setHeader("X-Foo", "late value", false);
    do_throw("late setHeader didn't throw");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }

  try
  {
    response.bodyOutputStream;
    do_throw("late bodyOutputStream get didn't throw");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }

  try
  {
    response.write("fugly");
    do_throw("late write() didn't throw");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }
}
handlers["/handleAsync1"] = handleAsync1;

function start_handleAsync1(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.responseStatusText, "New status line!");
  do_check_eq(ch.getResponseHeader("X-Foo"), "new value");
}

function stop_handleAsync1(ch, cx, status, data)
{
  do_check_eq(data.length, 0);
}

test = new Test(PREPATH + "/handleAsync1",
                null, start_handleAsync1, stop_handleAsync1),
tests.push(test);


const startToHeaderDelay = 500;
const startToFinishedDelay = 750;

function handleAsync2(request, response)
{
  response.processAsync();

  response.setStatusLine(request.httpVersion, 200, "Status line");
  response.setHeader("X-Custom-Header", "value", false);

  callLater(startToHeaderDelay, function()
  {
    var body = "BO";
    response.bodyOutputStream.write(body, body.length);

    try
    {
      response.setStatusLine(request.httpVersion, 500, "after body write");
      do_throw("setStatusLine succeeded");
    }
    catch (e)
    {
      isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
    }

    try
    {
      response.setHeader("X-Custom-Header", "new 1", false);
    }
    catch (e)
    {
      isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
    }

    callLater(startToFinishedDelay - startToHeaderDelay, function()
    {
      var body = "DY";
      response.bodyOutputStream.write(body, body.length);

      response.finish();
      response.finish(); // idempotency

      try
      {
        response.setStatusLine(request.httpVersion, 500, "after finish");
      }
      catch (e)
      {
        isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
      }

      try
      {
        response.setHeader("X-Custom-Header", "new 2", false);
      }
      catch (e)
      {
        isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
      }

      try
      {
        response.write("EVIL");
      }
      catch (e)
      {
        isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
      }
    });
  });
}
handlers["/handleAsync2"] = handleAsync2;

var startTime_handleAsync2;

function init_handleAsync2(ch)
{
  var now = startTime_handleAsync2 = Date.now();
  dumpn("*** init_HandleAsync2: start time " + now);
}

function start_handleAsync2(ch, cx)
{
  var now = Date.now();
  dumpn("*** start_handleAsync2: onStartRequest time " + now + ", " +
        (now - startTime_handleAsync2) + "ms after start time");
  do_check_true(now >= startTime_handleAsync2 + startToHeaderDelay);

  do_check_eq(ch.responseStatus, 200);
  do_check_eq(ch.responseStatusText, "Status line");
  do_check_eq(ch.getResponseHeader("X-Custom-Header"), "value");
}

function stop_handleAsync2(ch, cx, status, data)
{
  var now = Date.now();
  dumpn("*** stop_handleAsync2: onStopRequest time " + now + ", " +
        (now - startTime_handleAsync2) + "ms after header time");
  do_check_true(now >= startTime_handleAsync2 + startToFinishedDelay);

  do_check_eq(String.fromCharCode.apply(null, data), "BODY");
}

test = new Test(PREPATH + "/handleAsync2",
                init_handleAsync2, start_handleAsync2, stop_handleAsync2);
tests.push(test);


/*
 * Tests that accessing output stream *before* calling processAsync() works
 * correctly, sending written data immediately as it is written, not buffering
 * until finish() is called -- which for this much data would mean we would all
 * but certainly deadlock, since we're trying to read/write all this data in one
 * process on a single thread.
 */
function handleAsyncOrdering(request, response)
{
  var out = new BinaryOutputStream(response.bodyOutputStream);

  var data = [];
  for (var i = 0; i < 65536; i++)
    data[i] = 0;
  var count = 20;

  var writeData =
    {
      run: function()
      {
        if (count-- === 0)
        {
          response.finish();
          return;
        }

        try
        {
          out.writeByteArray(data, data.length);
          step();
        }
        catch (e)
        {
          try
          {
            do_throw("error writing data: " + e);
          }
          finally
          {
            response.finish();
          }
        }
      }
    };
  function step()
  {
    // Use gThreadManager here because it's expedient, *not* because it's
    // intended for public use!  If you do this in client code, expect me to
    // knowingly break your code by changing the variable name.  :-P
    gThreadManager.currentThread
                  .dispatch(writeData, Ci.nsIThread.DISPATCH_NORMAL);
  }
  step();
  response.processAsync();
}
handlers["/handleAsyncOrdering"] = handleAsyncOrdering;

function stop_handleAsyncOrdering(ch, cx, status, data)
{
  do_check_eq(data.length, 20 * 65536);
  data.forEach(function(v, index)
  {
    if (v !== 0)
      do_throw("value " + v + " at index " + index + " should be zero");
  });
}

test = new Test(PREPATH + "/handleAsyncOrdering",
                null, null, stop_handleAsyncOrdering);
tests.push(test);

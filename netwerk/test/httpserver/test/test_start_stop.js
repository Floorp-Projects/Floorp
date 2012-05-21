/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests for correct behavior of the server start() and stop() methods.
 */

const PORT = 4444;
const PREPATH = "http://localhost:" + PORT;

var srv, srv2;

function run_test()
{
  if ("@mozilla.org/windows-registry-key;1" in Components.classes)
  {
    dumpn("*** not running test_start_stop.js on Windows for now, because " +
          "Windows is dumb");
    return;
  }

  dumpn("*** run_test");

  srv = createServer();
  srv.start(PORT);

  try
  {
    srv.start(PORT);
    do_throw("starting a started server");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_ALREADY_INITIALIZED);
  }

  try
  {
    srv.stop();
    do_throw("missing argument to stop");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NULL_POINTER);
  }

  try
  {
    srv.stop(null);
    do_throw("null argument to stop");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NULL_POINTER);
  }

  do_test_pending();
  srv.stop(function()
  {
    try
    {
      do_test_pending();
      run_test_2();
    }
    finally
    {
      do_test_finished();
    }
  });
}

function run_test_2()
{
  dumpn("*** run_test_2");

  do_test_finished();

  srv.start(PORT);
  srv2 = createServer();

  try
  {
    srv2.start(PORT);
    do_throw("two servers on one port?");
  }
  catch (e)
  {
    isException(e, Cr.NS_ERROR_NOT_AVAILABLE);
  }

  do_test_pending();
  try
  {
    srv.stop({onStopped: function()
              {
                try
                {
                  do_test_pending();
                  run_test_3();
                }
                finally
                {
                  do_test_finished();
                }
              }
             });
  }
  catch (e)
  {
    do_throw("error stopping with an object: " + e);
  }
}

function run_test_3()
{
  dumpn("*** run_test_3");

  do_test_finished();

  srv.registerPathHandler("/handle", handle);
  srv.start(PORT);

  // Don't rely on the exact (but implementation-constant) sequence of events
  // as it currently exists by making either run_test_4 or serverStopped handle
  // the final shutdown.
  do_test_pending();

  runHttpTests([new Test(PREPATH + "/handle")], run_test_4);
}

var testsComplete = false;

function run_test_4()
{
  dumpn("*** run_test_4");

  testsComplete = true;
  if (stopped)
    do_test_finished();
}


const INTERVAL = 500;

function handle(request, response)
{
  response.processAsync();

  dumpn("*** stopping server...");
  srv.stop(serverStopped);

  callLater(INTERVAL, function()
  {
    do_check_false(stopped);

    callLater(INTERVAL, function()
    {
      do_check_false(stopped);
      response.finish();

      try
      {
        response.processAsync();
        do_throw("late processAsync didn't throw?");
      }
      catch (e)
      {
        isException(e, Cr.NS_ERROR_UNEXPECTED);
      }
    });
  });
}

var stopped = false;
function serverStopped()
{
  dumpn("*** server really, fully shut down now");
  stopped = true;
  if (testsComplete)
    do_test_finished();
}

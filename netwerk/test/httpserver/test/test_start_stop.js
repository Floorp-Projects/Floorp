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

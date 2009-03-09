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

// exercises the server's state-preservation API

const PORT = 4444;

var srv;

function run_test()
{
  srv = createServer();
  var sjsDir = do_get_file("netwerk/test/httpserver/test/data/sjs/");
  srv.registerDirectory("/", sjsDir);
  srv.registerContentType("sjs", "sjs");
  srv.registerPathHandler("/path-handler", pathHandler);
  srv.start(PORT);

  function done()
  {
    do_check_eq(srv.getSharedState("shared-value"), "done!");
    do_check_eq(srv.getState("/path-handler", "private-value"),
                "pathHandlerPrivate2");
    do_check_eq(srv.getState("/state1.sjs", "private-value"),
                "");
    do_check_eq(srv.getState("/state2.sjs", "private-value"),
                "newPrivate5");
    srv.stop();
  }

  runHttpTests(tests, done);
}


/************
 * HANDLERS *
 ************/

var firstTime = true;

function pathHandler(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  response.setHeader("X-Old-Shared-Value", srv.getSharedState("shared-value"),
                     false);
  response.setHeader("X-Old-Private-Value", srv.getState("/path-handler", "private-value"),
                     false);

  var privateValue, sharedValue;
  if (firstTime)
  {
    firstTime = false;
    privateValue = "pathHandlerPrivate";
    sharedValue = "pathHandlerShared";
  }
  else
  {
    privateValue = "pathHandlerPrivate2";
    sharedValue = "";
  }

  srv.setState("/path-handler", "private-value", privateValue);
  srv.setSharedState("shared-value", sharedValue);

  response.setHeader("X-New-Private-Value", privateValue, false);
  response.setHeader("X-New-Shared-Value", sharedValue, false);
}


/***************
 * BEGIN TESTS *
 ***************/

var test;
var tests = [];

/* Hack around bug 474845 for now. */
function getHeaderFunction(ch)
{
  function getHeader(name)
  {
    try
    {
      return ch.getResponseHeader(name);
    }
    catch (e)
    {
      if (e.result !== Cr.NS_ERROR_NOT_AVAILABLE)
        throw e;
    }
    return "";
  }
  return getHeader;
}

function expectValues(ch, oldShared, newShared, oldPrivate, newPrivate)
{
  var getHeader = getHeaderFunction(ch);

  do_check_eq(ch.responseStatus, 200);
  do_check_eq(getHeader("X-Old-Shared-Value"), oldShared);
  do_check_eq(getHeader("X-New-Shared-Value"), newShared);
  do_check_eq(getHeader("X-Old-Private-Value"), oldPrivate);
  do_check_eq(getHeader("X-New-Private-Value"), newPrivate);
}


test = new Test("http://localhost:4444/state1.sjs?" +
                "newShared=newShared&newPrivate=newPrivate",
                null, start_initial, null);
tests.push(test);

function start_initial(ch, cx)
{
dumpn("XXX start_initial");
  expectValues(ch, "", "newShared", "", "newPrivate");
}


test = new Test("http://localhost:4444/state1.sjs?" +
                "newShared=newShared2&newPrivate=newPrivate2",
                null, start_overwrite, null);
tests.push(test);

function start_overwrite(ch, cx)
{
  expectValues(ch, "newShared", "newShared2", "newPrivate", "newPrivate2");
}


test = new Test("http://localhost:4444/state1.sjs?" +
                "newShared=&newPrivate=newPrivate3",
                null, start_remove, null);
tests.push(test);

function start_remove(ch, cx)
{
  expectValues(ch, "newShared2", "", "newPrivate2", "newPrivate3");
}


test = new Test("http://localhost:4444/path-handler",
                null, start_handler, null);
tests.push(test);

function start_handler(ch, cx)
{
  expectValues(ch, "", "pathHandlerShared", "", "pathHandlerPrivate");
}


test = new Test("http://localhost:4444/path-handler",
                null, start_handler_again, null);
tests.push(test);

function start_handler_again(ch, cx)
{
  expectValues(ch, "pathHandlerShared", "",
               "pathHandlerPrivate", "pathHandlerPrivate2");
}


test = new Test("http://localhost:4444/state2.sjs?" +
                "newShared=newShared4&newPrivate=newPrivate4",
                null, start_other_initial, null);
tests.push(test);

function start_other_initial(ch, cx)
{
  expectValues(ch, "", "newShared4", "", "newPrivate4");
}


test = new Test("http://localhost:4444/state2.sjs?" +
                "newShared=",
                null, start_other_remove_ignore, null);
tests.push(test);

function start_other_remove_ignore(ch, cx)
{
  expectValues(ch, "newShared4", "", "newPrivate4", "");
}


test = new Test("http://localhost:4444/state2.sjs?" +
                "newShared=newShared5&newPrivate=newPrivate5",
                null, start_other_set_new, null);
tests.push(test);

function start_other_set_new(ch, cx)
{
  expectValues(ch, "", "newShared5", "newPrivate4", "newPrivate5");
}


test = new Test("http://localhost:4444/state1.sjs?" +
                "newShared=done!&newPrivate=",
                null, start_set_remove_original, null);
tests.push(test);

function start_set_remove_original(ch, cx)
{
  expectValues(ch, "newShared5", "done!", "newPrivate3", "");
}

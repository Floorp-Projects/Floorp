/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// exercises the server's state-preservation API

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + srv.identity.primaryPort;
});

var srv;

function run_test()
{
  srv = createServer();
  var sjsDir = do_get_file("data/sjs/");
  srv.registerDirectory("/", sjsDir);
  srv.registerContentType("sjs", "sjs");
  srv.registerPathHandler("/path-handler", pathHandler);
  srv.start(-1);

  function done()
  {
    do_check_eq(srv.getSharedState("shared-value"), "done!");
    do_check_eq(srv.getState("/path-handler", "private-value"),
                "pathHandlerPrivate2");
    do_check_eq(srv.getState("/state1.sjs", "private-value"),
                "");
    do_check_eq(srv.getState("/state2.sjs", "private-value"),
                "newPrivate5");
    do_test_pending();
    srv.stop(function() { do_test_finished(); });
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

XPCOMUtils.defineLazyGetter(this, "tests", function() {
  return [
    new Test(URL + "/state1.sjs?" +
                    "newShared=newShared&newPrivate=newPrivate",
                    null, start_initial, null),
    new Test(URL + "/state1.sjs?" +
                    "newShared=newShared2&newPrivate=newPrivate2",
                    null, start_overwrite, null),
    new Test(URL + "/state1.sjs?" +
                    "newShared=&newPrivate=newPrivate3",
                    null, start_remove, null),
    new Test(URL + "/path-handler",
                    null, start_handler, null),
    new Test(URL + "/path-handler",
                    null, start_handler_again, null),
    new Test(URL + "/state2.sjs?" +
                    "newShared=newShared4&newPrivate=newPrivate4",
                    null, start_other_initial, null),
    new Test(URL + "/state2.sjs?" +
                    "newShared=",
                    null, start_other_remove_ignore, null),
    new Test(URL + "/state2.sjs?" +
                    "newShared=newShared5&newPrivate=newPrivate5",
                    null, start_other_set_new, null),
    new Test(URL + "/state1.sjs?" +
                    "newShared=done!&newPrivate=",
                    null, start_set_remove_original, null)
  ];
});

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

function start_initial(ch, cx)
{
dumpn("XXX start_initial");
  expectValues(ch, "", "newShared", "", "newPrivate");
}

function start_overwrite(ch, cx)
{
  expectValues(ch, "newShared", "newShared2", "newPrivate", "newPrivate2");
}

function start_remove(ch, cx)
{
  expectValues(ch, "newShared2", "", "newPrivate2", "newPrivate3");
}

function start_handler(ch, cx)
{
  expectValues(ch, "", "pathHandlerShared", "", "pathHandlerPrivate");
}

function start_handler_again(ch, cx)
{
  expectValues(ch, "pathHandlerShared", "",
               "pathHandlerPrivate", "pathHandlerPrivate2");
}

function start_other_initial(ch, cx)
{
  expectValues(ch, "", "newShared4", "", "newPrivate4");
}

function start_other_remove_ignore(ch, cx)
{
  expectValues(ch, "newShared4", "", "newPrivate4", "");
}

function start_other_set_new(ch, cx)
{
  expectValues(ch, "", "newShared5", "newPrivate4", "newPrivate5");
}

function start_set_remove_original(ch, cx)
{
  expectValues(ch, "newShared5", "done!", "newPrivate3", "");
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that channels with different
 *  AppIds/inBrowserElements/usePrivateBrowsing (from nsILoadContext callback)
 *  are stored in separate namespaces ("cookie jars")
 */ 

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
var httpserver = new HttpServer();

var cookieSetPath = "/setcookie";
var cookieCheckPath = "/checkcookie";

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

// Test array:
//  - element 0: name for cookie, used both to set and later to check 
//  - element 1: loadContext (determines cookie namespace)
//
// TODO: bug 722850: make private browsing work per-app, and add tests.  For now
// all values are 'false' for PB.

var tests = [
  { cookieName: 'LCC_App0_BrowF_PrivF', 
    loadContext: new LoadContextCallback(0, false, false, 1) }, 
  { cookieName: 'LCC_App0_BrowT_PrivF', 
    loadContext: new LoadContextCallback(0, true,  false, 1) }, 
  { cookieName: 'LCC_App1_BrowF_PrivF', 
    loadContext: new LoadContextCallback(1, false, false, 1) }, 
  { cookieName: 'LCC_App1_BrowT_PrivF', 
    loadContext: new LoadContextCallback(1, true,  false, 1) }, 
];

// test number: index into 'tests' array
var i = 0;

function setupChannel(path)
{
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444" + path, "", null);
  chan.notificationCallbacks = tests[i].loadContext;
  chan.QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function setCookie() {
  var channel = setupChannel(cookieSetPath);
  channel.setRequestHeader("foo-set-cookie", tests[i].cookieName, false);
  channel.asyncOpen(new ChannelListener(setNextCookie, null), null);
}

function setNextCookie(request, data, context) 
{
  if (++i == tests.length) {
    // all cookies set: switch to checking them
    i = 0;
    checkCookie();
  } else {
    do_print("setNextCookie:i=" + i);
    setCookie();
  }
}

// Open channel that should send one and only one correct Cookie: header to
// server, corresponding to it's namespace
function checkCookie()
{
  var channel = setupChannel(cookieCheckPath);
  channel.asyncOpen(new ChannelListener(completeCheckCookie, null), null);
}

function completeCheckCookie(request, data, context) {
  // Look for all cookies in what the server saw: fail if we see any besides the
  // one expected cookie for each namespace;
  var expectedCookie = tests[i].cookieName;
  request.QueryInterface(Ci.nsIHttpChannel);
  var cookiesSeen = request.getResponseHeader("foo-saw-cookies");

  var j;
  for (j = 0; j < tests.length; j++) {
    var cookieToCheck = tests[j].cookieName;
    found = (cookiesSeen.indexOf(cookieToCheck) != -1);
    if (found && expectedCookie != cookieToCheck) {
      do_throw("test index " + i + ": found unexpected cookie '" 
          + cookieToCheck + "': in '" + cookiesSeen + "'");
    } else if (!found && expectedCookie == cookieToCheck) {
      do_throw("test index " + i + ": missing expected cookie '" 
          + expectedCookie + "': in '" + cookiesSeen + "'");
    }
  }
  // If we get here we're good.
  do_print("Saw only correct cookie '" + expectedCookie + "'");
  do_check_true(true);


  if (++i == tests.length) {
    // end of tests
    httpserver.stop(do_test_finished);
  } else {
    checkCookie();
  }
}

function run_test()
{
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess())
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  httpserver.registerPathHandler(cookieSetPath, cookieSetHandler);
  httpserver.registerPathHandler(cookieCheckPath, cookieCheckHandler);
  httpserver.start(4444);

  setCookie();
  do_test_pending();
}

function cookieSetHandler(metadata, response)
{
  var cookieName = metadata.getHeader("foo-set-cookie");

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Set-Cookie", cookieName + "=1; Path=/", false);
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

function cookieCheckHandler(metadata, response)
{
  var cookies = metadata.getHeader("Cookie");

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("foo-saw-cookies", cookies, false);
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write("Ok", "Ok".length);
}


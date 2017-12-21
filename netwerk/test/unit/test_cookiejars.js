/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that channels with different LoadInfo
 *  are stored in separate namespaces ("cookie jars")
 */ 

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

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
//  - element 1: loadInfo (determines cookie namespace)
//
// TODO: bug 722850: make private browsing work per-app, and add tests.  For now
// all values are 'false' for PB.

var tests = [
  { cookieName: 'LCC_App0_BrowF_PrivF', 
    originAttributes: new OriginAttributes(0, false, 0) },
  { cookieName: 'LCC_App0_BrowT_PrivF', 
    originAttributes: new OriginAttributes(0, true, 0) },
  { cookieName: 'LCC_App1_BrowF_PrivF', 
    originAttributes: new OriginAttributes(1, false, 0) },
  { cookieName: 'LCC_App1_BrowT_PrivF', 
    originAttributes: new OriginAttributes(1, true, 0) },
];

// test number: index into 'tests' array
var i = 0;

function setupChannel(path)
{
  var chan = NetUtil.newChannel({uri: URL + path, loadUsingSystemPrincipal: true});
  chan.loadInfo.originAttributes = tests[i].originAttributes;
  chan.QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function setCookie() {
  var channel = setupChannel(cookieSetPath);
  channel.setRequestHeader("foo-set-cookie", tests[i].cookieName, false);
  channel.asyncOpen2(new ChannelListener(setNextCookie, null));
}

function setNextCookie(request, data, context) 
{
  if (++i == tests.length) {
    // all cookies set: switch to checking them
    i = 0;
    checkCookie();
  } else {
    info("setNextCookie:i=" + i);
    setCookie();
  }
}

// Open channel that should send one and only one correct Cookie: header to
// server, corresponding to it's namespace
function checkCookie()
{
  var channel = setupChannel(cookieCheckPath);
  channel.asyncOpen2(new ChannelListener(completeCheckCookie, null));
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
  info("Saw only correct cookie '" + expectedCookie + "'");
  Assert.ok(true);


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
  httpserver.start(-1);

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


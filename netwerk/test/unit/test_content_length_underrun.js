/*
 * Test Content-Length underrun behavior
 */

////////////////////////////////////////////////////////////////////////////////
// Test infrastructure

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var index = 0;
var test_flags = new Array();
var testPathBase = "/cl_hdrs";

var prefs;
var enforcePrefStrict;
var enforcePrefSoft;

function run_test()
{
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  enforcePrefStrict = prefs.getBoolPref("network.http.enforce-framing.http1");
  enforcePrefSoft = prefs.getBoolPref("network.http.enforce-framing.soft");
  prefs.setBoolPref("network.http.enforce-framing.http1", true);

  httpserver.start(-1);

  do_test_pending();
  run_test_number(1);
}

function run_test_number(num)
{
  testPath = testPathBase + num;
  httpserver.registerPathHandler(testPath, eval("handler" + num));

  var channel = setupChannel(testPath);
  flags = test_flags[num];   // OK if flags undefined for test
  channel.asyncOpen(new ChannelListener(eval("completeTest" + num),
                                        channel, flags), null);
}

function setupChannel(url)
{
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel2(URL + url,
                             "",
                             null,
                             null,      // aLoadingNode
                             Services.scriptSecurityManager.getSystemPrincipal(),
                             null,      // aTriggeringPrincipal
                             Ci.nsILoadInfo.SEC_NORMAL,
                             Ci.nsIContentPolicy.TYPE_OTHER);
  var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  return httpChan;
}

function endTests()
{
  // restore the prefs to pre-test values
  prefs.setBoolPref("network.http.enforce-framing.http1", enforcePrefStrict);
  prefs.setBoolPref("network.http.enforce-framing.soft", enforcePrefSoft);
  httpserver.stop(do_test_finished);
}

////////////////////////////////////////////////////////////////////////////////
// Test 1: FAIL because of Content-Length underrun with HTTP 1.1
test_flags[1] = CL_EXPECT_LATE_FAILURE;

function handler1(metadata, response)
{
  var body = "blablabla";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 556677\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest1(request, data, ctx)
{
  do_check_eq(request.status, Components.results.NS_ERROR_NET_PARTIAL_TRANSFER);

  run_test_number(2);
}

////////////////////////////////////////////////////////////////////////////////
// Test 2: Succeed because Content-Length underrun is with HTTP 1.0

test_flags[2] = CL_IGNORE_CL;

function handler2(metadata, response)
{
  var body = "short content";

  response.seizePower();
  response.write("HTTP/1.0 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 12345678\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest2(request, data, ctx)
{
  do_check_eq(request.status, Components.results.NS_OK);

  // test 3 requires the enforce-framing prefs to be false
  prefs.setBoolPref("network.http.enforce-framing.http1", false);
  prefs.setBoolPref("network.http.enforce-framing.soft", false);
  run_test_number(3);
}

////////////////////////////////////////////////////////////////////////////////
// Test 3: SUCCEED with bad Content-Length because pref allows it
test_flags[3] = CL_IGNORE_CL;

function handler3(metadata, response)
{
  var body = "blablabla";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 556677\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest3(request, data, ctx)
{
  // reset the strict pref in case we add more tests
  prefs.setBoolPref("network.http.enforce-framing.http1", true);
  do_check_eq(request.status, Components.results.NS_OK);
  endTests();
}

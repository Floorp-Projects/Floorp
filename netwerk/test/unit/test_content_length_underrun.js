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

function run_gzip_test(num)
{
  testPath = testPathBase + num;
  httpserver.registerPathHandler(testPath, eval("handler" + num));

  var channel = setupChannel(testPath);

  function StreamListener() {}

  StreamListener.prototype = {
    QueryInterface: function(aIID) {
      if (aIID.equals(Components.interfaces.nsIStreamListener) ||
          aIID.equals(Components.interfaces.nsIRequestObserver) ||
          aIID.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    onStartRequest: function(aRequest, aContext) {},

    onStopRequest: function(aRequest, aContext, aStatusCode) {
      // Make sure we catch the error NS_ERROR_NET_PARTIAL_TRANSFER here.
      do_check_eq(aStatusCode, Components.results.NS_ERROR_NET_PARTIAL_TRANSFER);
      //  do_test_finished();
        endTests();
    },

    onDataAvailable: function(request, context, stream, offset, count) {}
  };

  let listener = new StreamListener();
 
  channel.asyncOpen(listener, null);

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

  run_test_number(11);
}

////////////////////////////////////////////////////////////////////////////////
// Test 11: PASS because of Content-Length underrun with HTTP 1.1 but non 2xx
test_flags[11] = CL_IGNORE_CL;

function handler11(metadata, response)
{
  var body = "blablabla";

  response.seizePower();
  response.write("HTTP/1.1 404 NotOK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 556677\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest11(request, data, ctx)
{
  do_check_eq(request.status, Components.results.NS_OK);
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
  do_check_eq(request.status, Components.results.NS_OK);
  prefs.setBoolPref("network.http.enforce-framing.soft", true);
  run_test_number(4);
}

////////////////////////////////////////////////////////////////////////////////
// Test 4: Succeed because a cut off deflate stream can't be detected
test_flags[4] = CL_IGNORE_CL;

function handler4(metadata, response)
{
    // this is the beginning of a deflate compressed response body
    
  var body = "\xcd\x57\xcd\x6e\x1b\x37\x10\xbe\x07\xc8\x3b\x0c\x36\x68\x72\xd1" +
    "\xbf\x92\x22\xb1\x57\x0a\x64\x4b\x6a\x0c\x28\xb6\x61\xa9\x41\x73" +
    "\x2a\xb8\xbb\x94\x44\x98\xfb\x03\x92\x92\xec\x06\x7d\x97\x1e\xeb" +
    "\xbe\x86\x5e\xac\xc3\x25\x97\xa2\x64\xb9\x75\x0b\x14\xe8\x69\x87" +
    "\x33\x9c\x1f\x7e\x33\x9c\xe1\x86\x9f\x66\x9f\x27\xfd\x97\x2f\x20" +
    "\xfc\x34\x1a\x0c\x35\x01\xa1\x62\x8a\xd3\xfe\xf5\xcd\xd5\xe5\xd5" +
    "\x6c\x54\x83\x49\xbe\x60\x31\xa3\x1c\x12\x0a\x0b\x2a\x15\xcb\x33" +
    "\x4d\xae\x19\x05\x19\xe7\x9c\x30\x41\x1b\x61\xd3\x28\x95\xfa\x29" +
    "\x55\x04\x32\x92\xd2\x5e\x90\x50\x19\x0b\x56\x68\x9d\x00\xe2\x3c" +
    "\x53\x34\x53\xbd\xc0\x99\x56\xf9\x4a\x51\xe0\x64\xcf\x18\x24\x24" +
    "\x93\xb0\xca\x40\xd2\x15\x07\x6e\xbd\x37\x60\x82\x3b\x8f\x86\x22" +
    "\x21\xcb\x15\x95\x35\x20\x91\xa4\x59\xac\xa9\x62\x95\x31\xed\x14" +
    "\xc9\x98\x2c\x19\x15\x3a\x62\x45\xef\x70\x1b\x50\x05\xa4\x28\xc4" +
    "\xf6\x21\x66\xa4\xdc\x83\x32\x09\x85\xc8\xe7\x54\xa2\x4b\x81\x74" +
    "\xbe\x12\xc0\x91\xb9\x7d\x50\x24\xe2\x0c\xd9\x29\x06\x2e\xdd\x79";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 553677\r\n");
  response.write("Content-Encoding: deflate\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

function completeTest4(request, data, ctx)
{
  do_check_eq(request.status, Components.results.NS_OK);

  prefs.setBoolPref("network.http.enforce-framing.http1", true);
  run_gzip_test(99);
}

////////////////////////////////////////////////////////////////////////////////
// Test 99: FAIL because a cut off gzip stream CAN be detected

// Note that test 99 here is run completely different than the other tests in
// this file so if you add more tests here, consider adding them before this.

function handler99(metadata, response)
{
  // this is the beginning of a gzip compressed response body
    
  var body = "\x1f\x8b\x08\x00\x80\xb9\x25\x53\x00\x03\xd4\xd9\x79\xb8\x8e\xe5" +
    "\xba\x00\xf0\x65\x19\x33\x24\x15\x29\xf3\x50\x52\xc6\xac\x85\x10" +
    "\x8b\x12\x22\x45\xe6\xb6\x21\x9a\x96\x84\x4c\x69\x32\xec\x84\x92" +
    "\xcc\x99\x6a\xd9\x32\xa5\xd0\x40\xd9\xc6\x14\x15\x95\x28\x62\x9b" +
    "\x09\xc9\x70\x4a\x25\x53\xec\x8e\x9c\xe5\x1c\x9d\xeb\xfe\x9d\x73" +
    "\x9d\x3f\xf6\x1f\xe7\xbd\xae\xcf\xf3\xbd\xbf\xef\x7e\x9f\xeb\x79" +
    "\xef\xf7\x99\xde\xe5\xee\x6e\xdd\x3b\x75\xeb\xd1\xb5\x6c\xb3\xd4" +
    "\x47\x1f\x48\xf8\x17\x1d\x15\xce\x1d\x55\x92\x93\xcf\x97\xe7\x8e" +
    "\x8b\xca\xe4\xca\x55\x92\x2a\x54\x4e\x4e\x4e\x4a\xa8\x78\x53\xa5" +
    "\x8a\x15\x2b\x55\x4a\xfa\xe3\x7b\x85\x8a\x37\x55\x48\xae\x92\x50" +
    "\xb4\xc2\xbf\xaa\x41\x17\x1f\xbd\x7b\xf6\xba\xaf\x47\xd1\xa2\x09" +
    "\x3d\xba\x75\xeb\xf5\x3f\xc5\xfd\x6f\xbf\xff\x3f\x3d\xfa\xd7\x6d" +
    "\x74\x7b\x62\x86\x0c\xff\x79\x9e\x98\x50\x33\xe1\x8f\xb3\x01\xef" +
    "\xb6\x38\x7f\x9e\x92\xee\xf9\xa7\xee\xcb\x74\x21\x26\x25\xa1\x6a" +
    "\x42\xf6\x73\xff\x96\x4c\x28\x91\x90\xe5\xdc\x79\xa6\x8b\xe2\x52" +
    "\xd2\xbf\x5d\x28\x2b\x24\x26\xfc\xa9\xcc\x96\x1e\x97\x31\xfd\xba" +
    "\xee\xe9\xde\x3d\x31\xe5\x4f\x65\xc1\xf4\xb8\x0b\x65\x86\x8b\xca";
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Content-Length: 553677\r\n");
  response.write("Content-Encoding: gzip\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

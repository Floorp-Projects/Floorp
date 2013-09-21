/*
 * Test whether the rewrite-requests-from-script API implemented here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=765934 is functioning
 * correctly
 *
 * The test has the following components:
 *
 * testViaXHR() checks that internal redirects occur correctly for requests
 * made with nsIXMLHttpRequest objects.
 *
 * testViaAsyncOpen() checks that internal redirects occur correctly when made
 * with nsIHTTPChannel.asyncOpen().
 *
 * Both of the above functions tests four requests:
 *
 * Test 1: a simple case that redirects within a server;
 * Test 2: a second that redirects to a second webserver;
 * Test 3: internal script redirects in response to a server-side 302 redirect;
 * Test 4: one internal script redirects in response to another's redirect.
 *
 * The successful redirects are confirmed by the presence of a custom response
 * header.
 *
 */

Cu.import("resource://testing-common/httpd.js");

// the topic we observe to use the API.  http-on-opening-request might also
// work for some purposes.
redirectHook = "http-on-modify-request";

var httpServer = null, httpServer2 = null;

XPCOMUtils.defineLazyGetter(this, "port1", function() {
  return httpServer.identity.primaryPort;
});

XPCOMUtils.defineLazyGetter(this, "port2", function() {
  return httpServer2.identity.primaryPort;
});

// Test Part 1: a cross-path redirect on a single HTTP server
// http://localhost:port1/bait -> http://localhost:port1/switch
var baitPath = "/bait";
XPCOMUtils.defineLazyGetter(this, "baitURI", function() {
  return "http://localhost:" + port1 + baitPath;
});
var baitText = "you got the worm";

var redirectedPath = "/switch";
XPCOMUtils.defineLazyGetter(this, "redirectedURI", function() {
  return "http://localhost:" + port1 + redirectedPath;
});
var redirectedText = "worms are not tasty";

// Test Part 2: Now, a redirect to a different server
// http://localhost:port1/bait2 -> http://localhost:port2/switch
var bait2Path = "/bait2";
XPCOMUtils.defineLazyGetter(this, "bait2URI", function() {
  return "http://localhost:" + port1 + bait2Path;
});

XPCOMUtils.defineLazyGetter(this, "redirected2URI", function() {
  return "http://localhost:" + port2 + redirectedPath;
});

// Test Part 3, begin with a serverside redirect that itself turns into an instance
// of Test Part 1
var bait3Path = "/bait3";
XPCOMUtils.defineLazyGetter(this, "bait3URI", function() {
  return "http://localhost:" + port1 + bait3Path;
});

// Test Part 4, begin with this client-side redirect and which then redirects
// to an instance of Test Part 1
var bait4Path = "/bait4";
XPCOMUtils.defineLazyGetter(this, "bait4URI", function() {
  return "http://localhost:" + port1 + bait4Path;
});

var testHeaderName = "X-Redirected-By-Script"
var testHeaderVal = "Success";
var testHeaderVal2 = "Success on server 2";

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

function baitHandler(metadata, response)
{
  // Content-Type required: https://bugzilla.mozilla.org/show_bug.cgi?id=748117
  response.setHeader("Content-Type", "text/html", false);
  response.bodyOutputStream.write(baitText, baitText.length);
}

function redirectedHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/html", false);
  response.bodyOutputStream.write(redirectedText, redirectedText.length);
  response.setHeader(testHeaderName, testHeaderVal);
}

function redirected2Handler(metadata, response)
{
  response.setHeader("Content-Type", "text/html", false);
  response.bodyOutputStream.write(redirectedText, redirectedText.length);
  response.setHeader(testHeaderName, testHeaderVal2);
}

function bait3Handler(metadata, response)
{
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", baitURI);
}

function Redirector()
{
  this.register();
}

Redirector.prototype = {
  // This class observes an event and uses that to
  // trigger a redirectTo(uri) redirect using the new API
  register: function()
  {
    Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService).
      addObserver(this, redirectHook, true);
  },

  QueryInterface: function(iid)
  {
    if (iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsISupportsWeakReference) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  observe: function(subject, topic, data)
  {
    if (topic == redirectHook) {
      if (!(subject instanceof Ci.nsIHttpChannel))
        do_throw(redirectHook + " observed a non-HTTP channel");
      var channel = subject.QueryInterface(Ci.nsIHttpChannel);
      var ioservice = Cc["@mozilla.org/network/io-service;1"].
                        getService(Ci.nsIIOService);
      var target = null;
      if (channel.URI.spec == baitURI)  target = redirectedURI;
      if (channel.URI.spec == bait2URI) target = redirected2URI;
      if (channel.URI.spec == bait4URI) target = baitURI;
       // if we have a target, redirect there
      if (target) {
        var tURI = ioservice.newURI(target, null, null);
        try {
          channel.redirectTo(tURI);
        } catch (e) {
          do_throw("Exception in redirectTo " + e + "\n");
        }
      }
    }
  }
}

function makeAsyncTest(uri, headerValue, nextTask)
{
  // Make a test to check a redirect that is created with channel.asyncOpen()

  // Produce a callback function which checks for the presence of headerValue,
  // and then continues to the next async test task
  var verifier = function(req, buffer)
  {
    if (!(req instanceof Ci.nsIHttpChannel))
      do_throw(req + " is not an nsIHttpChannel, catastrophe imminent!");

    var httpChannel = req.QueryInterface(Ci.nsIHttpChannel);
    do_check_eq(httpChannel.getResponseHeader(testHeaderName), headerValue);
    do_check_eq(buffer, redirectedText);
    nextTask();
  };

  // Produce a function to run an asyncOpen test using the above verifier
  var test = function()
  {
    var chan = make_channel(uri);
    chan.asyncOpen(new ChannelListener(verifier), null);
  };
  return test;
}

// will be defined in run_test because of the lazy getters,
// since the server's port is defined dynamically
var testViaAsyncOpen4 = null;
var testViaAsyncOpen3 = null;
var testViaAsyncOpen2 = null;
var testViaAsyncOpen  = null;

function testViaXHR()
{
  runXHRTest(baitURI,  testHeaderVal);
  runXHRTest(bait2URI, testHeaderVal2);
  runXHRTest(bait3URI, testHeaderVal);
  runXHRTest(bait4URI, testHeaderVal);
}

function runXHRTest(uri, headerValue)
{
  // Check that making an XHR request for uri winds up redirecting to a result with the
  // appropriate headerValue
  var xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"];

  var req = xhr.createInstance(Ci.nsIXMLHttpRequest);
  req.open("GET", uri, false);
  req.send();
  do_check_eq(req.getResponseHeader(testHeaderName), headerValue);
  do_check_eq(req.response, redirectedText);
}

function done()
{
  httpServer.stop(
    function ()
    {
      httpServer2.stop(do_test_finished);
    }
  );
}

var redirector = new Redirector();

function run_test()
{
  httpServer = new HttpServer();
  httpServer.registerPathHandler(baitPath, baitHandler);
  httpServer.registerPathHandler(bait2Path, baitHandler);
  httpServer.registerPathHandler(bait3Path, bait3Handler);
  httpServer.registerPathHandler(bait4Path, baitHandler);
  httpServer.registerPathHandler(redirectedPath, redirectedHandler);
  httpServer.start(-1);
  httpServer2 = new HttpServer();
  httpServer2.registerPathHandler(redirectedPath, redirected2Handler);
  httpServer2.start(-1);

  // The tests depend on each other, and therefore need to be defined in the
  // reverse of the order they are called in.  It is therefore best to read this
  // stanza backwards!
  testViaAsyncOpen4 = makeAsyncTest(bait4URI, testHeaderVal, done);
  testViaAsyncOpen3 = makeAsyncTest(bait3URI, testHeaderVal, testViaAsyncOpen4);
  testViaAsyncOpen2 = makeAsyncTest(bait2URI, testHeaderVal2, testViaAsyncOpen3);
  testViaAsyncOpen  = makeAsyncTest(baitURI,  testHeaderVal, testViaAsyncOpen2);

  testViaXHR();
  testViaAsyncOpen();  // will call done() asynchronously for cleanup

  do_test_pending();
}

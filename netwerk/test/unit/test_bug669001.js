"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpServer = null;
var path = "/bug699001";

XPCOMUtils.defineLazyGetter(this, "URI", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + path;
});

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var fetched;

// The test loads a resource that expires in one year, has an etag and varies only by User-Agent
// First we load it, then check we load it only from the cache w/o even checking with the server
// Then we modify our User-Agent and try it again
// We have to get a new content (even though with the same etag) and again on next load only from
// cache w/o accessing the server
// Goal is to check we've updated User-Agent request header in cache after we've got 304 response
// from the server

var tests = [
  {
    prepare() {},
    test(response) {
      Assert.ok(fetched);
    },
  },
  {
    prepare() {},
    test(response) {
      Assert.ok(!fetched);
    },
  },
  {
    prepare() {
      setUA("A different User Agent");
    },
    test(response) {
      Assert.ok(fetched);
    },
  },
  {
    prepare() {},
    test(response) {
      Assert.ok(!fetched);
    },
  },
  {
    prepare() {
      setUA("And another User Agent");
    },
    test(response) {
      Assert.ok(fetched);
    },
  },
  {
    prepare() {},
    test(response) {
      Assert.ok(!fetched);
    },
  },
];

function handler(metadata, response) {
  if (metadata.hasHeader("If-None-Match")) {
    response.setStatusLine(metadata.httpVersion, 304, "Not modified");
  } else {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain");

    var body = "body";
    response.bodyOutputStream.write(body, body.length);
  }

  fetched = true;

  response.setHeader("Expires", getDateString(+1));
  response.setHeader("Cache-Control", "private");
  response.setHeader("Vary", "User-Agent");
  response.setHeader("ETag", "1234");
}

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler(path, handler);
  httpServer.start(-1);

  do_test_pending();

  nextTest();
}

function nextTest() {
  fetched = false;
  tests[0].prepare();

  dump("Testing with User-Agent: " + getUA() + "\n");
  var chan = make_channel(URI);

  // Give the old channel a chance to close the cache entry first.
  // XXX This is actually a race condition that might be considered a bug...
  executeSoon(function() {
    chan.asyncOpen(new ChannelListener(checkAndShiftTest, null));
  });
}

function checkAndShiftTest(request, response) {
  tests[0].test(response);

  tests.shift();
  if (tests.length == 0) {
    httpServer.stop(tearDown);
    return;
  }

  nextTest();
}

function tearDown() {
  setUA("");
  do_test_finished();
}

// Helpers

function getUA() {
  var httphandler = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  );
  return httphandler.userAgent;
}

function setUA(value) {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.setCharPref("general.useragent.override", value);
}

function getDateString(yearDelta) {
  var months = [
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
  ];
  var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

  var d = new Date();
  return (
    days[d.getUTCDay()] +
    ", " +
    d.getUTCDate() +
    " " +
    months[d.getUTCMonth()] +
    " " +
    (d.getUTCFullYear() + yearDelta) +
    " " +
    d.getUTCHours() +
    ":" +
    d.getUTCMinutes() +
    ":" +
    d.getUTCSeconds() +
    " UTC"
  );
}

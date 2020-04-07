/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests async handling of a channel suspended in
// notifying http-on-examine-merged-response observers.
// Note that this test is developed based on test_bug482601.js.
"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserv = null;
var test_nr = 0;
var buffer = "";
var observerCalled = false;
var channelResumed = false;

var observer = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    if (
      topic === "http-on-examine-merged-response" &&
      subject instanceof Ci.nsIHttpChannel
    ) {
      var chan = subject.QueryInterface(Ci.nsIHttpChannel);
      executeSoon(() => {
        Assert.equal(channelResumed, false);
        channelResumed = true;
        chan.resume();
      });
      Assert.equal(observerCalled, false);
      observerCalled = true;
      chan.suspend();
    }
  },
};

var listener = {
  onStartRequest(request) {
    buffer = "";
  },

  onDataAvailable(request, stream, offset, count) {
    buffer = buffer.concat(read_stream(stream, count));
  },

  onStopRequest(request, status) {
    Assert.equal(status, Cr.NS_OK);
    Assert.equal(buffer, "0123456789");
    Assert.equal(channelResumed, true);
    Assert.equal(observerCalled, true);
    test_nr++;
    do_timeout(0, do_test);
  },
};

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/path/partial", path_partial);
  httpserv.registerPathHandler("/path/cached", path_cached);
  httpserv.start(-1);

  var obs = Cc["@mozilla.org/observer-service;1"].getService(
    Ci.nsIObserverService
  );
  obs.addObserver(observer, "http-on-examine-merged-response");

  do_timeout(0, do_test);
  do_test_pending();
}

function do_test() {
  if (test_nr < tests.length) {
    tests[test_nr]();
  } else {
    var obs = Cc["@mozilla.org/observer-service;1"].getService(
      Ci.nsIObserverService
    );
    obs.removeObserver(observer, "http-on-examine-merged-response");
    httpserv.stop(do_test_finished);
  }
}

var tests = [test_partial, test_cached];

function makeChan(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function storeCache(aCacheEntry, aResponseHeads, aContent) {
  aCacheEntry.setMetaDataElement("request-method", "GET");
  aCacheEntry.setMetaDataElement("response-head", aResponseHeads);
  aCacheEntry.setMetaDataElement("charset", "ISO-8859-1");

  var oStream = aCacheEntry.openOutputStream(0, aContent.length);
  var written = oStream.write(aContent, aContent.length);
  if (written != aContent.length) {
    do_throw(
      "oStream.write has not written all data!\n" +
        "  Expected: " +
        written +
        "\n" +
        "  Actual: " +
        aContent.length +
        "\n"
    );
  }
  oStream.close();
  aCacheEntry.close();
}

function test_partial() {
  observerCalled = false;
  channelResumed = false;
  asyncOpenCacheEntry(
    "http://localhost:" + httpserv.identity.primaryPort + "/path/partial",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    test_partial2
  );
}

function test_partial2(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  storeCache(
    entry,
    "HTTP/1.1 200 OK\r\n" +
      "Date: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
      "Server: httpd.js\r\n" +
      "Last-Modified: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
      "Accept-Ranges: bytes\r\n" +
      "Content-Length: 10\r\n" +
      "Content-Type: text/plain\r\n",
    "0123"
  );

  observerCalled = false;

  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/path/partial"
  );
  chan.asyncOpen(listener);
}

function test_cached() {
  observerCalled = false;
  channelResumed = false;
  asyncOpenCacheEntry(
    "http://localhost:" + httpserv.identity.primaryPort + "/path/cached",
    "disk",
    Ci.nsICacheStorage.OPEN_NORMALLY,
    null,
    test_cached2
  );
}

function test_cached2(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  storeCache(
    entry,
    "HTTP/1.1 200 OK\r\n" +
      "Date: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
      "Server: httpd.js\r\n" +
      "Last-Modified: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
      "Accept-Ranges: bytes\r\n" +
      "Content-Length: 10\r\n" +
      "Content-Type: text/plain\r\n",
    "0123456789"
  );

  observerCalled = false;

  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/path/cached"
  );
  chan.loadFlags = Ci.nsIRequest.VALIDATE_ALWAYS;
  chan.asyncOpen(listener);
}

// PATHS

// /path/partial
function path_partial(metadata, response) {
  Assert.ok(metadata.hasHeader("If-Range"));
  Assert.equal(metadata.getHeader("If-Range"), "Thu, 1 Jan 2009 00:00:00 GMT");
  Assert.ok(metadata.hasHeader("Range"));
  Assert.equal(metadata.getHeader("Range"), "bytes=4-");

  response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
  response.setHeader("Content-Range", "bytes 4-9/10", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Last-Modified", "Thu, 1 Jan 2009 00:00:00 GMT");

  var body = "456789";
  response.bodyOutputStream.write(body, body.length);
}

// /path/cached
function path_cached(metadata, response) {
  Assert.ok(metadata.hasHeader("If-Modified-Since"));
  Assert.equal(
    metadata.getHeader("If-Modified-Since"),
    "Thu, 1 Jan 2009 00:00:00 GMT"
  );

  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
}

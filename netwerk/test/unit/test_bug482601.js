const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserv = null;
var test_nr = 0;
var observers_called = "";
var handlers_called = "";
var buffer = "";

var observer = {
  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsISupports) ||
        aIID.equals(Ci.nsIObserver))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  observe: function(subject, topic, data) {
    if (observers_called.length)
      observers_called += ",";

    observers_called += topic;
  }
};

var listener = {
  onStartRequest: function (request) {
    buffer = "";
  },

  onDataAvailable: function (request, stream, offset, count) {
    buffer = buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function (request, status) {
    Assert.equal(status, Cr.NS_OK);
    Assert.equal(buffer, "0123456789");
    Assert.equal(observers_called, results[test_nr]);
    test_nr++;
    do_timeout(0, do_test);
  }
};

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/bug482601/nocache", bug482601_nocache);
  httpserv.registerPathHandler("/bug482601/partial", bug482601_partial);
  httpserv.registerPathHandler("/bug482601/cached", bug482601_cached);
  httpserv.registerPathHandler("/bug482601/only_from_cache", bug482601_only_from_cache);
  httpserv.start(-1);

  var obs = Cc["@mozilla.org/observer-service;1"].getService();
  obs = obs.QueryInterface(Ci.nsIObserverService);
  obs.addObserver(observer, "http-on-examine-response");
  obs.addObserver(observer, "http-on-examine-merged-response");
  obs.addObserver(observer, "http-on-examine-cached-response");

  do_timeout(0, do_test);
  do_test_pending();
}

function do_test() {
  if (test_nr < tests.length) {
    tests[test_nr]();
  }
  else {
    Assert.equal(handlers_called, "nocache,partial,cached");
    httpserv.stop(do_test_finished);
  }
}

var tests = [test_nocache,
             test_partial,
             test_cached,
             test_only_from_cache];

var results = ["http-on-examine-response",
               "http-on-examine-response,http-on-examine-merged-response",
               "http-on-examine-response,http-on-examine-merged-response",
               "http-on-examine-cached-response"];

function makeChan(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

function storeCache(aCacheEntry, aResponseHeads, aContent) {
  aCacheEntry.setMetaDataElement("request-method", "GET");
  aCacheEntry.setMetaDataElement("response-head", aResponseHeads);
  aCacheEntry.setMetaDataElement("charset", "ISO-8859-1");

  var oStream = aCacheEntry.openOutputStream(0, aContent.length);
  var written = oStream.write(aContent, aContent.length);
  if (written != aContent.length) {
    do_throw("oStream.write has not written all data!\n" +
             "  Expected: " + written  + "\n" +
             "  Actual: " + aContent.length + "\n");
  }
  oStream.close();
  aCacheEntry.close();
}

function test_nocache() {
  observers_called = "";

  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort +
                      "/bug482601/nocache");
  chan.asyncOpen(listener);
}

function test_partial() {
   asyncOpenCacheEntry("http://localhost:" + httpserv.identity.primaryPort +
                       "/bug482601/partial",
                       "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                       test_partial2);
}

function test_partial2(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  storeCache(entry,
             "HTTP/1.1 200 OK\r\n" +
             "Date: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Server: httpd.js\r\n" +
             "Last-Modified: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Accept-Ranges: bytes\r\n" +
             "Content-Length: 10\r\n" +
             "Content-Type: text/plain\r\n",
             "0123");

  observers_called = "";

  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort +
                      "/bug482601/partial");
  chan.asyncOpen(listener);
}

function test_cached() {
   asyncOpenCacheEntry("http://localhost:" + httpserv.identity.primaryPort +
                       "/bug482601/cached",
                       "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                       test_cached2);
}

function test_cached2(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  storeCache(entry,
             "HTTP/1.1 200 OK\r\n" +
             "Date: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Server: httpd.js\r\n" +
             "Last-Modified: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Accept-Ranges: bytes\r\n" +
             "Content-Length: 10\r\n" +
             "Content-Type: text/plain\r\n",
             "0123456789");

  observers_called = "";

  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort +
                      "/bug482601/cached");
  chan.loadFlags = Ci.nsIRequest.VALIDATE_ALWAYS;
  chan.asyncOpen(listener);
}

function test_only_from_cache() {
   asyncOpenCacheEntry("http://localhost:" + httpserv.identity.primaryPort +
                       "/bug482601/only_from_cache",
                       "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
                       test_only_from_cache2);
}

function test_only_from_cache2(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  storeCache(entry,
             "HTTP/1.1 200 OK\r\n" +
             "Date: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Server: httpd.js\r\n" +
             "Last-Modified: Thu, 1 Jan 2009 00:00:00 GMT\r\n" +
             "Accept-Ranges: bytes\r\n" +
             "Content-Length: 10\r\n" +
             "Content-Type: text/plain\r\n",
             "0123456789");

  observers_called = "";

  var chan = makeChan("http://localhost:" + httpserv.identity.primaryPort +
                      "/bug482601/only_from_cache");
  chan.loadFlags = Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE;
  chan.asyncOpen(listener);
}


// PATHS

// /bug482601/nocache
function bug482601_nocache(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  var body = "0123456789";
  response.bodyOutputStream.write(body, body.length);
  handlers_called += "nocache";
}

// /bug482601/partial
function bug482601_partial(metadata, response) {
  Assert.ok(metadata.hasHeader("If-Range"));
  Assert.equal(metadata.getHeader("If-Range"),
               "Thu, 1 Jan 2009 00:00:00 GMT");
  Assert.ok(metadata.hasHeader("Range"));
  Assert.equal(metadata.getHeader("Range"), "bytes=4-");

  response.setStatusLine(metadata.httpVersion, 206, "Partial Content");
  response.setHeader("Content-Range", "bytes 4-9/10", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Last-Modified", "Thu, 1 Jan 2009 00:00:00 GMT");

  var body = "456789";
  response.bodyOutputStream.write(body, body.length);
  handlers_called += ",partial";
}

// /bug482601/cached
function bug482601_cached(metadata, response) {
  Assert.ok(metadata.hasHeader("If-Modified-Since"));
  Assert.equal(metadata.getHeader("If-Modified-Since"),
               "Thu, 1 Jan 2009 00:00:00 GMT");

  response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
  handlers_called += ",cached";
}

// /bug482601/only_from_cache
function bug482601_only_from_cache(metadata, response) {
  do_throw("This should not be reached");
}

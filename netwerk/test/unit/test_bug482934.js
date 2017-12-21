"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var response_code;
var response_body;

var request_time;
var response_time;

var cache_storage;

var httpserver = new HttpServer();
httpserver.start(-1);

var base_url = "http://localhost:" + httpserver.identity.primaryPort;
var resource = "/resource";
var resource_url = base_url + resource;

// Test flags
var hit_server = false;

function make_channel(aUrl)
{
  // Reset test global status
  hit_server = false;

  var req = NetUtil.newChannel({uri: aUrl, loadUsingSystemPrincipal: true});
  req.QueryInterface(Ci.nsIHttpChannel);
  req.setRequestHeader("If-Modified-Since", request_time, false);
  return req;
}

function make_uri(aUrl)
{
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(aUrl);
}

function resource_handler(aMetadata, aResponse)
{
  hit_server = true;
  Assert.ok(aMetadata.hasHeader("If-Modified-Since"));
  Assert.equal(aMetadata.getHeader("If-Modified-Since"), request_time);

  if (response_code == "200") {
    aResponse.setStatusLine(aMetadata.httpVersion, 200, "OK");
    aResponse.setHeader("Content-Type", "text/plain", false);
    aResponse.setHeader("Last-Modified", response_time, false);

    aResponse.bodyOutputStream.write(response_body, response_body.length);
  } else if (response_code == "304") {
    aResponse.setStatusLine(aMetadata.httpVersion, 304, "Not Modified");
    aResponse.setHeader("Returned-From-Handler", "1");
  }
}

function check_cached_data(aCachedData, aCallback)
{
  asyncOpenCacheEntry(resource_url, "disk", Ci.nsICacheStorage.OPEN_READONLY, null,
    function(aStatus, aEntry) {
      Assert.equal(aStatus, Cr.NS_OK);
      pumpReadStream(aEntry.openInputStream(0), function(aData) {
        Assert.equal(aData, aCachedData);
        aCallback();
      });
    }
  );
}

function run_test()
{
  do_get_profile();
  evict_cache_entries();

  do_test_pending();

  cache_storage = getCacheStorage("disk");
  httpserver.registerPathHandler(resource, resource_handler);

  wait_for_cache_index(run_next_test);
}

// 1. send custom conditional request when we don't have an entry
//    server returns 304 -> client receives 304
add_test(() => {
  response_code = "304";
  response_body = "";
  request_time = "Thu, 1 Jan 2009 00:00:00 GMT";
  response_time = "Thu, 1 Jan 2009 00:00:00 GMT";

  var ch = make_channel(resource_url);
  ch.asyncOpen2(new ChannelListener(function(aRequest, aData) {
    Assert.ok(hit_server);
    Assert.equal(aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus, 304);
    Assert.ok(!cache_storage.exists(make_uri(resource_url), ""));
    Assert.equal(aRequest.getResponseHeader("Returned-From-Handler"), "1");

    run_next_test();
  }, null));
});

// 2. send custom conditional request when we don't have an entry
//    server returns 200 -> result is cached
add_test(() => {
  response_code = "200";
  response_body = "content_body";
  request_time = "Thu, 1 Jan 2009 00:00:00 GMT";
  response_time = "Fri, 2 Jan 2009 00:00:00 GMT";

  var ch = make_channel(resource_url);
  ch.asyncOpen2(new ChannelListener(function(aRequest, aData) {
    Assert.ok(hit_server);
    Assert.equal(aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
    Assert.ok(cache_storage.exists(make_uri(resource_url), ""));

    check_cached_data(response_body, run_next_test);
  }, null));
});

// 3. send custom conditional request when we have an entry
//    server returns 304 -> client receives 304 and cached entry is unchanged
add_test(() => {
  response_code = "304";
  var cached_body = response_body;
  response_body = "";
  request_time = "Fri, 2 Jan 2009 00:00:00 GMT";
  response_time = "Fri, 2 Jan 2009 00:00:00 GMT";

  var ch = make_channel(resource_url);
  ch.asyncOpen2(new ChannelListener(function(aRequest, aData) {
    Assert.ok(hit_server);
    Assert.equal(aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus, 304);
    Assert.ok(cache_storage.exists(make_uri(resource_url), ""));
    Assert.equal(aRequest.getResponseHeader("Returned-From-Handler"), "1");
    Assert.equal(aData, "");

    // Check the cache data is not changed
    check_cached_data(cached_body, run_next_test);
  }, null));
});

// 4. send custom conditional request when we have an entry
//    server returns 200 -> result is cached
add_test(() => {
  response_code = "200";
  response_body = "updated_content_body";
  request_time = "Fri, 2 Jan 2009 00:00:00 GMT";
  response_time = "Sat, 3 Jan 2009 00:00:00 GMT";
  var ch = make_channel(resource_url);
  ch.asyncOpen2(new ChannelListener(function(aRequest, aData) {
    Assert.ok(hit_server);
    Assert.equal(aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
    Assert.ok(cache_storage.exists(make_uri(resource_url), ""));

    // Check the cache data is updated
    check_cached_data(response_body, () => {
      run_next_test();
      httpserver.stop(do_test_finished);
    });
  }, null));
});

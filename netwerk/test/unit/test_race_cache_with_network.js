/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var httpserver = new HttpServer();
httpserver.start(-1);
const PORT = httpserver.identity.primaryPort;

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true
  }).QueryInterface(Components.interfaces.nsIHttpChannel);
}

let gResponseBody = "blahblah";
let g200Counter = 0;
let g304Counter = 0;
function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("ETag", "test-etag1");

  try {
    var etag = metadata.getHeader("If-None-Match");
  } catch(ex) {
    var etag = "";
  }

  if (etag == "test-etag1") {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    g304Counter++;
  } else {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.bodyOutputStream.write(gResponseBody, gResponseBody.length);
    g200Counter++;
  }
}

function cached_handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "Cache-Control: max-age=3600");
  response.setHeader("ETag", "test-etag1");

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(gResponseBody, gResponseBody.length);

  g200Counter++;
}

let gResponseCounter = 0;
let gIsFromCache = 0;
function checkContent(request, buffer, context, isFromCache)
{
  Assert.equal(buffer, gResponseBody);
  gResponseCounter++;
  if (isFromCache) {
    gIsFromCache++;
  }
  do_execute_soon(() => { testGenerator.next(); });
}

function run_test() {
  do_get_profile();
  // In this test, we manually use |TriggerNetwork| to prove we could send
  // net and cache reqeust simultaneously. Therefore we should disable
  // racing in the HttpChannel first.
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  httpserver.registerPathHandler("/rcwn", test_handler);
  httpserver.registerPathHandler("/rcwn_cached", cached_handler);
  testGenerator.next();
  do_test_pending();
}

let testGenerator = testSteps();
function *testSteps() {
  // Initial request. Stores the response in the cache.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gResponseCounter, 1);
  equal(g200Counter, 1, "check number of 200 responses");
  equal(g304Counter, 0, "check number of 304 responses");

  // Checks that response is returned from the cache, after a 304 response.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gResponseCounter, 2);
  equal(g200Counter, 1, "check number of 200 responses");
  equal(g304Counter, 1, "check number of 304 responses");

  // Checks that delaying the response from the cache works.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(200);
  let startTime = Date.now();
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  greater(Date.now() - startTime, 200, "Check that timer works properly");
  equal(gResponseCounter, 3);
  equal(g200Counter, 1, "check number of 200 responses");
  equal(g304Counter, 2, "check number of 304 responses");

  // Checks that we can trigger the cache open immediately, even if the cache delay is set very high.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  do_timeout(50, function() {
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerDelayedOpenCacheEntry();
  });
  yield undefined;
  equal(gResponseCounter, 4);
  equal(g200Counter, 1, "check number of 200 responses");
  equal(g304Counter, 3, "check number of 304 responses");

  // Sets a high delay for the cache fetch, and triggers the network activity.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  // Trigger network after 50 ms.
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(50);
  yield undefined;
  equal(gResponseCounter, 5);
  equal(g200Counter, 2, "check number of 200 responses");
  equal(g304Counter, 3, "check number of 304 responses");

  // Sets a high delay for the cache fetch, and triggers the network activity.
  // While the network response is produced, we trigger the cache fetch.
  // Because the network response was the first, a non-conditional request is sent.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  do_timeout(50, function() {
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(0);
    do_execute_soon(() => { channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerDelayedOpenCacheEntry(); });
  });
  yield undefined;
  equal(gResponseCounter, 6);
  equal(g200Counter, 3, "check number of 200 responses");
  equal(g304Counter, 3, "check number of 304 responses");

  // Triggers cache open before triggering network.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  do_timeout(50, function() {
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerDelayedOpenCacheEntry();
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(0);
  });
  yield undefined;
  equal(gResponseCounter, 7);
  equal(g200Counter, 3, "check number of 200 responses");
  equal(g304Counter, 4, "check number of 304 responses");

  // Load the cached handler so we don't need to revalidate
  var channel = make_channel("http://localhost:" + PORT + "/rcwn_cached");
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gResponseCounter, 8);
  equal(g200Counter, 4, "check number of 200 responses");
  equal(g304Counter, 4, "check number of 304 responses");

  // Make sure response is loaded from the cache, not the network
  var channel = make_channel("http://localhost:" + PORT + "/rcwn_cached");
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gResponseCounter, 9);
  equal(g200Counter, 4, "check number of 200 responses");
  equal(g304Counter, 4, "check number of 304 responses");

  // Cache times out, so we trigger the network
  gIsFromCache = 0;
  var channel = make_channel("http://localhost:" + PORT + "/rcwn_cached");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  // trigger network after 50 ms
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(50);
  yield undefined;
  equal(gResponseCounter, 10);
  equal(gIsFromCache, 0, "should be from the network");
  equal(g200Counter, 5, "check number of 200 responses");
  equal(g304Counter, 4, "check number of 304 responses");

  // Cache callback comes back right after network is triggered.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn_cached");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  do_timeout(50, function() {
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(0);
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerDelayedOpenCacheEntry();
  });
  yield undefined;
  equal(gResponseCounter, 11);
  do_print("IsFromCache: " + gIsFromCache + "\n");
  do_print("Number of 200 responses: " + g200Counter + "\n");
  equal(g304Counter, 4, "check number of 304 responses");

  // Set an increasingly high timeout to trigger opening the cache entry
  // This way we ensure that some of the entries we will get from the network,
  // and some we will get from the cache.
  gIsFromCache = 0;
  for (var i = 0; i < 50; i++) {
    var channel = make_channel("http://localhost:" + PORT + "/rcwn_cached");
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
    channel.asyncOpen2(new ChannelListener(checkContent, null));
    channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(10);
    // This may be racy. The delay was chosen because the distribution of net-cache
    // results was around 25-25 on my machine.
    do_timeout(i*100, function() {
      try {
        channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerDelayedOpenCacheEntry();
      } catch (e) {}

    });

    yield undefined;
  }

  greater(gIsFromCache, 0, "Some of the responses should be from the cache");
  less(gIsFromCache, 50, "Some of the responses should be from the net");

  httpserver.stop(do_test_finished);
}

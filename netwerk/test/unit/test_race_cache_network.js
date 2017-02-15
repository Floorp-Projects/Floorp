/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

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

let gResponseCounter = 0;
function checkContent(request, buffer)
{
  do_check_eq(buffer, gResponseBody);
  gResponseCounter++;
  testGenerator.next();
}

function run_test() {
  httpserver.registerPathHandler("/rcwn", test_handler);
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
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(1000);
  let startTime = Date.now();
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  yield undefined;
  greater(Date.now() - startTime, 1000, "Check that timer works properly");
  equal(gResponseCounter, 3);
  equal(g200Counter, 1, "check number of 200 responses");
  equal(g304Counter, 2, "check number of 304 responses");

  // Checks that we can trigger the cache open immediately, even if the cache delay is set very high.
  var channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen2(new ChannelListener(checkContent, null));
  do_timeout(500, function() {
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
  // Trigger network after 500 ms.
  channel.QueryInterface(Components.interfaces.nsIRaceCacheWithNetwork).test_triggerNetwork(500);
  yield undefined;
  equal(gResponseCounter, 5);
  equal(g200Counter, 2, "check number of 200 responses");
  equal(g304Counter, 3, "check number of 304 responses");

  httpserver.stop(do_test_finished);
}

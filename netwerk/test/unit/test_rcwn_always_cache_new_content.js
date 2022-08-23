/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
httpserver.start(-1);
const PORT = httpserver.identity.primaryPort;

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

let gFirstResponseBody = "first version";
let gSecondResponseBody = "second version";
let gRequestCounter = 0;

function test_handler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "max-age=3600");
  response.setHeader("ETag", "test-etag1");

  switch (gRequestCounter) {
    case 0:
      response.bodyOutputStream.write(
        gFirstResponseBody,
        gFirstResponseBody.length
      );
      break;
    case 1:
      response.bodyOutputStream.write(
        gSecondResponseBody,
        gSecondResponseBody.length
      );
      break;
    default:
      do_throw("Unexpected request");
  }
  response.setStatusLine(metadata.httpVersion, 200, "OK");
}

function checkContent(request, buffer, context, isFromCache) {
  let isRacing = request.QueryInterface(Ci.nsICacheInfoChannel).isRacing();
  switch (gRequestCounter) {
    case 0:
      Assert.equal(buffer, gFirstResponseBody);
      Assert.ok(!isFromCache);
      Assert.ok(!isRacing);
      break;
    case 1:
      Assert.equal(buffer, gSecondResponseBody);
      Assert.ok(!isFromCache);
      Assert.ok(isRacing);
      break;
    case 2:
      Assert.equal(buffer, gSecondResponseBody);
      Assert.ok(isFromCache);
      Assert.ok(!isRacing);
      break;
    default:
      do_throw("Unexpected request");
  }

  gRequestCounter++;
  executeSoon(() => {
    testGenerator.next();
  });
}

function run_test() {
  do_get_profile();
  // In this test, we race the requests manually using |TriggerNetwork|,
  // therefore we should disable racing in the HttpChannel first.
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  httpserver.registerPathHandler("/rcwn", test_handler);
  testGenerator.next();
  do_test_pending();
}

let testGenerator = testSteps();
function* testSteps() {
  // Store first version of the content in the cache.
  let channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.asyncOpen(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gRequestCounter, 1);

  // Simulate the network victory by setting high delay for the cache fetch and
  // triggering the network.
  channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel
    .QueryInterface(Ci.nsIRaceCacheWithNetwork)
    .test_delayCacheEntryOpeningBy(100000);
  channel.asyncOpen(new ChannelListener(checkContent, null));
  // Trigger network after 50 ms.
  channel.QueryInterface(Ci.nsIRaceCacheWithNetwork).test_triggerNetwork(50);
  yield undefined;
  equal(gRequestCounter, 2);

  // Simulate navigation back by specifying VALIDATE_NEVER flag.
  channel = make_channel("http://localhost:" + PORT + "/rcwn");
  channel.loadFlags = Ci.nsIRequest.VALIDATE_NEVER;
  channel.asyncOpen(new ChannelListener(checkContent, null));
  yield undefined;
  equal(gRequestCounter, 3);

  httpserver.stop(do_test_finished);
}

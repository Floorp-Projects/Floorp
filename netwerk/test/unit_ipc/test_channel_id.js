const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

/*
 * Test that when doing HTTP requests, the nsIHttpChannel is detected in
 * both parent and child and shares the same channelId across processes.
 */

let httpserver;
let port;

function startHttpServer() {
  httpserver = new HttpServer();

  httpserver.registerPathHandler("/resource", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.bodyOutputStream.write("data", 4);
  });

  httpserver.registerPathHandler("/redirect", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 302, "Redirect");
    response.setHeader("Location", "/resource", false);
    response.setHeader("Cache-Control", "no-cache", false);
  });

  httpserver.start(-1);
  port = httpserver.identity.primaryPort;
}

function stopHttpServer(next) {
  httpserver.stop(next);
}

let expectedParentChannels = [];
let expectedChildMessages = [];

let maybeFinishWaitForParentChannels;
let parentChannelsDone = new Promise(resolve => {
  maybeFinishWaitForParentChannels = () => {
    if (!expectedParentChannels.length) {
      dump("All expected parent channels were detected\n");
      resolve();
    }
  };
});

function observer(subject, topic, data) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);

  let uri = channel.URI.spec;
  let origUri = channel.originalURI.spec;
  let id = channel.channelId;
  dump(`Parent detected channel: ${uri} (orig=${origUri}): channelId=${id}\n`);

  // did we expect a new channel?
  let expected = expectedParentChannels.shift();
  Assert.ok(!!expected);

  // Start waiting for the messages about request/response from child
  for (let event of expected) {
    let message = `${event}:${id}`;
    dump(`Expecting message from child: ${message}\n`);

    let messagePromise = do_await_remote_message(message).then(() => {
      dump(`Expected message from child arrived: ${message}\n`);
    });
    expectedChildMessages.push(messagePromise);
  }

  // If we don't expect any further parent channels, finish the parent wait
  maybeFinishWaitForParentChannels();
}

function run_test() {
  startHttpServer();
  Services.obs.addObserver(observer, "http-on-modify-request");
  run_test_in_child("child_channel_id.js", makeRequests);
}

function makeRequests() {
  // First, a normal request without any redirect. Expect one channel detected
  // in parent, used by both request and response.
  expectedParentChannels.push(["request", "response"]);
  sendCommand(`makeRequest("http://localhost:${port}/resource");`);

  // Second request will be redirected. Expect two channels, one with the
  // original request, then the redirected one which gets the final response.
  expectedParentChannels.push(["request"], ["response"]);
  sendCommand(`makeRequest("http://localhost:${port}/redirect");`);

  waitForParentChannels();
}

function waitForParentChannels() {
  parentChannelsDone.then(waitForChildMessages);
}

function waitForChildMessages() {
  dump(`Waiting for ${expectedChildMessages.length} child messages\n`);
  Promise.all(expectedChildMessages).then(finish);
}

function finish() {
  Services.obs.removeObserver(observer, "http-on-modify-request");
  sendCommand("finish();", () => stopHttpServer(do_test_finished));
}

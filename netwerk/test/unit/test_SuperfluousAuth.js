/*

Create two http requests with the same URL in which has a user name. We allow
first http request to be loaded and saved in the cache, so the second request
will be served from the cache. However, we disallow loading by returning 1
in the prompt service. In the end, the second request will be failed.

*/

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

var httpProtocolHandler = Cc[
  "@mozilla.org/network/protocol;1?name=http"
].getService(Ci.nsIHttpProtocolHandler);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://foo@localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;

const gMockPromptService = {
  firstTimeCalled: false,
  confirmExBC() {
    if (!this.firstTimeCalled) {
      this.firstTimeCalled = true;
      return 0;
    }

    return 1;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
};

var gMockPromptServiceCID = MockRegistrar.register(
  "@mozilla.org/prompter;1",
  gMockPromptService
);

registerCleanupFunction(() => {
  MockRegistrar.unregister(gMockPromptServiceCID);
});

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

const responseBody = "body";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("ETag", "Just testing");
  response.setHeader("Cache-Control", "max-age=99999");
  response.setHeader("Content-Length", "" + responseBody.length);
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

function run_test() {
  do_get_profile();

  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  httpProtocolHandler.EnsureHSTSDataReady().then(function () {
    var chan1 = makeChan(URL + "/content");
    chan1.asyncOpen(new ChannelListener(firstTimeThrough, null));
    var chan2 = makeChan(URL + "/content");
    chan2.asyncOpen(
      new ChannelListener(secondTimeThrough, null, CL_EXPECT_FAILURE)
    );
  });

  do_test_pending();
}

function firstTimeThrough(request, buffer) {
  Assert.equal(buffer, responseBody);
  Assert.ok(gMockPromptService.firstTimeCalled, "Prompt service invoked");
}

function secondTimeThrough(request, buffer) {
  Assert.equal(request.status, Cr.NS_ERROR_SUPERFLUOS_AUTH);
  httpServer.stop(do_test_finished);
}

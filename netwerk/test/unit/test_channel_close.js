"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpProtocolHandler = Cc[
  "@mozilla.org/network/protocol;1?name=http"
].getService(Ci.nsIHttpProtocolHandler);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var testpath = "/simple";
var httpbody = "0123456789";

var live_channels = [];

function run_test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  httpProtocolHandler.EnsureHSTSDataReady().then(function () {
    var local_channel;

    // Opened channel that has no remaining references on shutdown
    local_channel = setupChannel(testpath);
    local_channel.asyncOpen(new ChannelListener(checkRequest, local_channel));

    // Opened channel that has no remaining references after being opened
    setupChannel(testpath).asyncOpen(new ChannelListener(function () {}, null));

    // Unopened channel that has remaining references on shutdown
    live_channels.push(setupChannel(testpath));

    // Opened channel that has remaining references on shutdown
    live_channels.push(setupChannel(testpath));
    live_channels[1].asyncOpen(
      new ChannelListener(checkRequestFinish, live_channels[1])
    );
  });

  do_test_pending();
}

function setupChannel(path) {
  var chan = NetUtil.newChannel({
    uri: URL + path,
    loadUsingSystemPrincipal: true,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function checkRequest(request, data, context) {
  Assert.equal(data, httpbody);
}

function checkRequestFinish(request, data, context) {
  checkRequest(request, data, context);
  httpserver.stop(do_test_finished);
}

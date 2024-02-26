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

add_task(async function test() {
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);
  registerCleanupFunction(async () => {
    if (httpserver) {
      await httpserver.stop();
    }
  });

  await httpProtocolHandler.EnsureHSTSDataReady();

  // Opened channel that has no remaining references on shutdown
  let local_channel = setupChannel(testpath);
  local_channel.asyncOpen(new SimpleChannelListener());

  // Opened channel that has no remaining references after being opened
  setupChannel(testpath).asyncOpen(new SimpleChannelListener());

  // Unopened channel that has remaining references on shutdown
  live_channels.push(setupChannel(testpath));

  // Opened channel that has remaining references on shutdown
  live_channels.push(setupChannel(testpath));
  await new Promise(resolve => {
    live_channels[1].asyncOpen(
      new SimpleChannelListener((req, data) => {
        Assert.equal(data, httpbody);
        resolve();
      })
    );
  });

  await httpserver.stop();
  httpserver = null;
});

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

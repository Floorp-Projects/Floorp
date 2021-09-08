"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
var httpServer = null;

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  return chan;
}

function makeURL(host) {
  return `http://${host}:${httpServer.identity.primaryPort}/`;
}

add_task(async function test_localhost_offline() {
  Services.io.offline = true;
  Services.prefs.setBoolPref("network.disable-localhost-when-offline", false);
  let chan = makeChan(makeURL("127.0.0.1"));
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });

  chan = makeChan(makeURL("localhost"));
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });

  Services.prefs.setBoolPref("network.disable-localhost-when-offline", true);

  chan = makeChan(makeURL("127.0.0.1"));
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  chan = makeChan(makeURL("localhost"));
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });

  Services.prefs.clearUserPref("network.disable-localhost-when-offline");
  Services.io.offline = false;
});

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/", response => {
    response.seizePower();
    response.write("HTTP/1.1 200 OK\r\n");
    response.write("\r\n");
    response.write("Hello, world!");
    response.finish();
  });
  httpServer.start(-1);
  run_next_test();
}

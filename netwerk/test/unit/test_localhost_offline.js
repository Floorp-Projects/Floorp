"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
var httpServer = null;
const body = "Hello";

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

function makeURL(host) {
  return `http://${host}:${httpServer.identity.primaryPort}/`;
}

add_task(async function test_localhost_offline() {
  Services.io.offline = true;
  Services.prefs.setBoolPref("network.disable-localhost-when-offline", false);
  let chan = makeChan(makeURL("127.0.0.1"));
  let [, resp] = await channelOpenPromise(chan);
  Assert.equal(resp, body, "Should get correct response");

  chan = makeChan(makeURL("localhost"));
  [, resp] = await channelOpenPromise(chan);
  Assert.equal(resp, body, "Should get response");

  Services.prefs.setBoolPref("network.disable-localhost-when-offline", true);

  chan = makeChan(makeURL("127.0.0.1"));
  let [req] = await channelOpenPromise(
    chan,
    CL_ALLOW_UNKNOWN_CL | CL_EXPECT_FAILURE
  );
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.status, Cr.NS_ERROR_OFFLINE);

  chan = makeChan(makeURL("localhost"));
  [req] = await channelOpenPromise(
    chan,
    CL_ALLOW_UNKNOWN_CL | CL_EXPECT_FAILURE
  );
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.status, Cr.NS_ERROR_OFFLINE);

  Services.prefs.clearUserPref("network.disable-localhost-when-offline");
  Services.io.offline = false;
});

function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/", (request, response) => {
    response.seizePower();
    response.write("HTTP/1.1 200 OK\r\n");
    response.write("Content-Length: " + body.length + "\r\n");
    response.write("\r\n");
    response.write(body);
    response.finish();
  });
  httpServer.start(-1);
  run_next_test();
}

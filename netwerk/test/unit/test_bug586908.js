"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

var httpserv = null;

ChromeUtils.defineLazyGetter(this, "systemSettings", function () {
  return {
    QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),

    mainThreadOnly: true,
    PACURI: "http://localhost:" + httpserv.identity.primaryPort + "/redirect",
    getProxyForURI(aURI) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    },
  };
});

function checkValue(request, data, ctx) {
  Assert.ok(called);
  Assert.equal("ok", data);
  httpserv.stop(do_test_finished);
}

function makeChan(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/redirect", redirect);
  httpserv.registerPathHandler("/pac", pac);
  httpserv.registerPathHandler("/target", target);
  httpserv.start(-1);

  MockRegistrar.register(
    "@mozilla.org/system-proxy-settings;1",
    systemSettings
  );

  // Ensure we're using system-properties
  Services.prefs.setIntPref(
    "network.proxy.type",
    Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM
  );

  // clear cache
  evict_cache_entries();

  var chan = makeChan(
    "http://localhost:" + httpserv.identity.primaryPort + "/target"
  );
  chan.asyncOpen(new ChannelListener(checkValue, null));

  do_test_pending();
}

var called = false,
  failed = false;
function redirect(metadata, response) {
  // If called second time, just return the PAC but set failed-flag
  if (called) {
    failed = true;
    pac(metadata, response);
    return;
  }

  called = true;
  response.setStatusLine(metadata.httpVersion, 302, "Found");
  response.setHeader("Location", "/pac", false);
  var body = "Moved\n";
  response.bodyOutputStream.write(body, body.length);
}

function pac(metadata, response) {
  var PAC = 'function FindProxyForURL(url, host) { return "DIRECT"; }';
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader(
    "Content-Type",
    "application/x-ns-proxy-autoconfig",
    false
  );
  response.bodyOutputStream.write(PAC, PAC.length);
}

function target(metadata, response) {
  var retval = "ok";
  if (failed) {
    retval = "failed";
  }

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(retval, retval.length);
}

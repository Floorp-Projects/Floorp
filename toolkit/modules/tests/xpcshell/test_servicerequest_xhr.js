/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ServiceRequest } = ChromeUtils.importESModule(
  "resource://gre/modules/ServiceRequest.sys.mjs"
);

add_task(async function test_tls_conservative() {
  const request = new ServiceRequest();
  request.open("GET", "http://example.com", false);

  const sr_channel = request.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  ok("beConservative" in sr_channel, "TLS setting is present in SR channel");
  ok(
    sr_channel.beConservative,
    "TLS setting in request channel is set to conservative for SR"
  );

  const xhr = new XMLHttpRequest();
  xhr.open("GET", "http://example.com", false);

  const xhr_channel = xhr.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  ok("beConservative" in xhr_channel, "TLS setting is present in XHR channel");
  ok(
    !xhr_channel.beConservative,
    "TLS setting in request channel is not set to conservative for XHR"
  );
});

add_task(async function test_bypassProxy_default() {
  const request = new ServiceRequest();
  request.open("GET", "http://example.com", true);
  const sr_channel = request.channel.QueryInterface(Ci.nsIHttpChannelInternal);

  ok(!sr_channel.bypassProxy, "bypassProxy is false on SR channel");
  ok(!request.bypassProxy, "bypassProxy is false for SR");
});

add_task(async function test_bypassProxy_true() {
  const request = new ServiceRequest();
  request.open("GET", "http://example.com", { bypassProxy: true });
  const sr_channel = request.channel.QueryInterface(Ci.nsIHttpChannelInternal);

  ok(sr_channel.bypassProxy, "bypassProxy is true on SR channel");
  ok(request.bypassProxy, "bypassProxy is true for SR");
});

add_task(async function test_bypassProxy_disabled_by_pref() {
  const request = new ServiceRequest();

  ok(request.bypassProxyEnabled, "bypassProxyEnabled is true");
  Services.prefs.setBoolPref("network.proxy.allow_bypass", false);
  ok(!request.bypassProxyEnabled, "bypassProxyEnabled is false");

  request.open("GET", "http://example.com", { bypassProxy: true });
  const sr_channel = request.channel.QueryInterface(Ci.nsIHttpChannelInternal);

  ok(!sr_channel.bypassProxy, "bypassProxy is false on SR channel");
  ok(!request.bypassProxy, "bypassProxy is false for SR");
});

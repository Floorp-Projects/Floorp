/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ServiceRequest.jsm");

add_task(async function test_tls_conservative() {
  const request = new ServiceRequest();
  request.open("GET", "http://example.com", false);

  const sr_channel = request.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  ok(("beConservative" in sr_channel), "TLS setting is present in SR channel");
  ok(sr_channel.beConservative, "TLS setting in request channel is set to conservative for SR");

  const xhr = new XMLHttpRequest();
  xhr.open("GET", "http://example.com", false);

  const xhr_channel = xhr.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  ok(("beConservative" in xhr_channel), "TLS setting is present in XHR channel");
  ok(!xhr_channel.beConservative, "TLS setting in request channel is not set to conservative for XHR");

});

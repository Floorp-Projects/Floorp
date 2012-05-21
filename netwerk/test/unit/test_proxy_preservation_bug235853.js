/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

do_load_httpd_js();

var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);

var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch);

function make_channel(url) {
  return ios.newChannel(url, null, null)
                .QueryInterface(Components.interfaces.nsIHttpChannel);
}

var httpserv = null;

// respond with the value of the header
function responseHandler(request, response) {
  response.setHeader("Content-Type", "text/plain", false);

  var value = request.hasHeader("SomeHeader") ? request.getHeader("SomeHeader") : "";
  response.write("SomeHeader: " + value);
}

function run_test() {
  httpserv = new nsHttpServer();
  httpserv.start(4444);
  httpserv.registerPathHandler("/test", responseHandler);
  // setup an identity so we can use the server with a different name when using
  // the server as a proxy
  httpserv.identity.add("http", "foo", 80);

  // cache key on channel creation
  var orig_key;

  // setup the properties that we want to be preserved
  function setup_channel(chan) {
    chan.setRequestHeader("SomeHeader", "Someval", false);

    // set cache key to something other than 0
    orig_key = chan.QueryInterface(Ci.nsICachingChannel)
                      .cacheKey.QueryInterface(Ci.nsISupportsPRUint32);
    orig_key.data = 0x32;
    chan.QueryInterface(Ci.nsICachingChannel).cacheKey = orig_key;
  }

  // check that these properties are preserved
  function check_response(request, data) {
    // check that headers are preserved
    do_check_eq(data, "SomeHeader: Someval");

    // check that the cacheKey is preserved
    var key = request.QueryInterface(Ci.nsICachingChannel)
                        .cacheKey.QueryInterface(Ci.nsISupportsPRUint32);
    do_check_eq(key.data, orig_key.data);
  }

  function setup_noproxy() {
    var chan = make_channel("http://localhost:4444/test");
    setup_channel(chan);
    chan.asyncOpen(new ChannelListener(test_noproxy, null), null);
  }

  function test_noproxy(request, data, ctx) {
    check_response(request, data);

    setup_with_proxy();
  }

  function setup_with_proxy() {
    // Setup a PAC rule using the server we setup as the proxy
    var pac = 'data:text/plain,' +
      'function FindProxyForURL(url, host) {' +
      '  return "PROXY localhost:4444";' +
      '}';

    // Configure PAC
    prefs.setIntPref("network.proxy.type", 2);
    prefs.setCharPref("network.proxy.autoconfig_url", pac);

    var chan = make_channel("http://foo/test");

    setup_channel(chan);

    chan.asyncOpen(new ChannelListener(test_with_proxy, null), null);
  }

  function test_with_proxy(request, data, ctx) {
    check_response(request, data);

    // cleanup PAC
    prefs.setCharPref("network.proxy.autoconfig_url", "");
    prefs.setIntPref("network.proxy.type", 0);

    httpserv.stop(do_test_finished);
  }

  setup_noproxy();

  do_test_pending();
}

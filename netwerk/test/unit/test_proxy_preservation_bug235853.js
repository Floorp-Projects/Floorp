/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jeff Muizelaar <jmuizelaar@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

do_import_script("netwerk/test/httpserver/httpd.js");

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

    httpserv.stop();
    do_test_finished();
  }

  setup_noproxy();

  do_test_pending();
}

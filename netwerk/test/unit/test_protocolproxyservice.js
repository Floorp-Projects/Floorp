/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This testcase exercises the Protocol Proxy Service

var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
var pps = Components.classes["@mozilla.org/network/protocol-proxy-service;1"]
                    .getService();

/**
 * Test nsIProtocolHandler that allows proxying, but doesn't allow HTTP
 * proxying.
 */
function TestProtocolHandler() {
}
TestProtocolHandler.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolHandler) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  scheme: "moz-test",
  defaultPort: -1,
  protocolFlags: Components.interfaces.nsIProtocolHandler.URI_NOAUTH |
                 Components.interfaces.nsIProtocolHandler.URI_NORELATIVE |
                 Components.interfaces.nsIProtocolHandler.ALLOWS_PROXY |
                 Components.interfaces.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD,
  newURI: function(spec, originCharset, baseURI) {
    var uri = Components.classes["@mozilla.org/network/simple-uri;1"]
                        .createInstance(Components.interfaces.nsIURI);
    uri.spec = spec;
    return uri;
  },
  newChannel: function(uri) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  allowPort: function(port, scheme) {
    return true;
  }
};

function TestProtocolHandlerFactory() {
}
TestProtocolHandlerFactory.prototype = {
  createInstance: function(delegate, iid) {
    return new TestProtocolHandler().QueryInterface(iid);
  },
  lockFactory: function(lock) {
  }
};

function register_test_protocol_handler() {
  var reg = Components.manager.QueryInterface(
      Components.interfaces.nsIComponentRegistrar);
  reg.registerFactory(Components.ID("{4ea7dd3a-8cae-499c-9f18-e1de773ca25b}"),
                      "TestProtocolHandler",
                      "@mozilla.org/network/protocol;1?name=moz-test",
                      new TestProtocolHandlerFactory());
}

function check_proxy(pi, type, host, port, flags, timeout, hasNext) {
  do_check_neq(pi, null);
  do_check_eq(pi.type, type);
  do_check_eq(pi.host, host);
  do_check_eq(pi.port, port);
  if (flags != -1)
    do_check_eq(pi.flags, flags);
  if (timeout != -1)
    do_check_eq(pi.failoverTimeout, timeout);
  if (hasNext)
    do_check_neq(pi.failoverProxy, null);
  else
    do_check_eq(pi.failoverProxy, null);
}

function TestFilter(type, host, port, flags, timeout) {
  this._type = type;
  this._host = host;
  this._port = port;
  this._flags = flags;
  this._timeout = timeout;
}
TestFilter.prototype = {
  _type: "",
  _host: "",
  _port: -1,
  _flags: 0,
  _timeout: 0,
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolProxyFilter) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  applyFilter: function(pps, uri, pi) {
    var pi_tail = pps.newProxyInfo(this._type, this._host, this._port,
                                   this._flags, this._timeout, null);
    if (pi)
      pi.failoverProxy = pi_tail;
    else
      pi = pi_tail;
    return pi;
  }
};

function BasicFilter() {}
BasicFilter.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolProxyFilter) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  applyFilter: function(pps, uri, pi) {
    return pps.newProxyInfo("http", "localhost", 8080, 0, 10,
           pps.newProxyInfo("direct", "", -1, 0, 0, null));
  }
};

function run_filter_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Verify initial state

  var pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);

  // Push a filter and verify the results

  var filter1 = new BasicFilter();
  var filter2 = new BasicFilter();
  pps.registerFilter(filter1, 10);
  pps.registerFilter(filter2, 20);

  pi = pps.resolve(uri, 0);
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  pps.unregisterFilter(filter2);
  pi = pps.resolve(uri, 0);
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  // Remove filter and verify that we return to the initial state

  pps.unregisterFilter(filter1);
  pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);
}

function run_filter_test2() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Verify initial state

  var pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);

  // Push a filter and verify the results

  var filter1 = new TestFilter("http", "foo", 8080, 0, 10);
  var filter2 = new TestFilter("http", "bar", 8090, 0, 10);
  pps.registerFilter(filter1, 20);
  pps.registerFilter(filter2, 10);

  pi = pps.resolve(uri, 0);
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter2);
  pi = pps.resolve(uri, 0);
  check_proxy(pi, "http", "foo", 8080, 0, 10, false);

  // Remove filter and verify that we return to the initial state

  pps.unregisterFilter(filter1);
  pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);
}

function run_pref_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

  // Verify 'direct' setting

  prefs.setIntPref("network.proxy.type", 0);

  var pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);

  // Verify 'manual' setting

  prefs.setIntPref("network.proxy.type", 1);

  // nothing yet configured
  pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);

  // try HTTP configuration
  prefs.setCharPref("network.proxy.http", "foopy");
  prefs.setIntPref("network.proxy.http_port", 8080);

  pi = pps.resolve(uri, 0);
  check_proxy(pi, "http", "foopy", 8080, 0, -1, false);

  prefs.setCharPref("network.proxy.http", "");
  prefs.setIntPref("network.proxy.http_port", 0);

  // try SOCKS configuration
  prefs.setCharPref("network.proxy.socks", "barbar");
  prefs.setIntPref("network.proxy.socks_port", 1203);

  pi = pps.resolve(uri, 0);
  check_proxy(pi, "socks", "barbar", 1203, 0, -1, false);
}

function run_protocol_handler_test() {
  var uri = ios.newURI("moz-test:foopy", null, null);

  var pi = pps.resolve(uri, 0);
  do_check_eq(pi, null);
}

function TestResolveCallback() {
}
TestResolveCallback.prototype = {
  QueryInterface:
  function TestResolveCallback_QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolProxyCallback) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onProxyAvailable:
  function TestResolveCallback_onProxyAvailable(req, uri, pi, status) {
    dump("*** uri=" + uri.spec + ", status=" + status + "\n");

    do_check_neq(req, null);
    do_check_neq(uri, null);
    do_check_eq(status, 0);
    do_check_neq(pi, null);

    check_proxy(pi, "http", "foopy", 8080, 0, -1, true);
    check_proxy(pi.failoverProxy, "direct", "", -1, -1, -1, false);

    // verify direct query now that we know the PAC file is loaded
    pi = pps.resolve(ios.newURI("http://bazbat.com/", null, null), 0);
    do_check_neq(pi, null);
    check_proxy(pi, "http", "foopy", 8080, 0, -1, true);
    check_proxy(pi.failoverProxy, "direct", "", -1, -1, -1, false);

    run_protocol_handler_test();

    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    prefs.setCharPref("network.proxy.autoconfig_url", "");
    prefs.setIntPref("network.proxy.type", 0);

    run_test_continued();
    do_test_finished();
  }
};

function run_pac_test() {
  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            '  return "PROXY foopy:8080; DIRECT";' +
            '}';
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

  // Configure PAC

  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  // Test it out (we expect an "unknown" result since the PAC load is async)
  var pi = pps.resolve(uri, 0);
  do_check_neq(pi, null);
  do_check_eq(pi.type, "unknown");

  // We expect the NON_BLOCKING flag to trigger an exception here since
  // we have configured the PPS to use PAC.
  var hit_exception = false;
  try {
    pps.resolve(uri, pps.RESOLVE_NON_BLOCKING);
  } catch (e) {
    hit_exception = true;
  }
  do_check_eq(hit_exception, true);

  var req = pps.asyncResolve(uri, 0, new TestResolveCallback());
  do_test_pending();
}

function TestResolveCancelationCallback() {
}
TestResolveCancelationCallback.prototype = {
  QueryInterface:
  function TestResolveCallback_QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolProxyCallback) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onProxyAvailable:
  function TestResolveCancelationCallback_onProxyAvailable(req, uri, pi, status) {
    dump("*** uri=" + uri.spec + ", status=" + status + "\n");

    do_check_neq(req, null);
    do_check_neq(uri, null);
    do_check_eq(status, Components.results.NS_ERROR_ABORT);
    do_check_eq(pi, null);

    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    prefs.setCharPref("network.proxy.autoconfig_url", "");
    prefs.setIntPref("network.proxy.type", 0);

    run_test_continued_2();
    do_test_finished();
  }
};

function run_pac_cancel_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Configure PAC
  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            '  return "PROXY foopy:8080; DIRECT";' +
            '}';
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var req = pps.asyncResolve(uri, 0, new TestResolveCancelationCallback());
  req.cancel(Components.results.NS_ERROR_ABORT);
  do_test_pending();
}

function check_host_filters(hostList, bShouldBeFiltered) {
  var uri;
  var proxy;
  for (var i=0; i<hostList.length; i++) {
    dump("*** uri=" + hostList[i] + " bShouldBeFiltered=" + bShouldBeFiltered + "\n");
    uri = ios.newURI(hostList, null, null);
    proxy = pps.resolve(uri, 0); 
    if (bShouldBeFiltered) {
      do_check_eq(proxy, null);
    } else {
      do_check_neq(proxy, null);
      // Just to be sure, let's check that the proxy is correct
      // - this should match the proxy setup in the calling function
      check_proxy(proxy, "http", "foopy", 8080, 0, -1, false);
    }
  }
}


// Verify that hists in the host filter list are not proxied
// refers to "network.proxy.no_proxies_on"

function run_proxy_host_filters_test() {
  // Get prefs object from DOM
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);
  // Setup a basic HTTP proxy configuration
  // - pps.resolve() needs this to return proxy info for non-filtered hosts
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setCharPref("network.proxy.http", "foopy");
  prefs.setIntPref("network.proxy.http_port", 8080);

  // Setup host filter list string for "no_proxies_on"
  var hostFilterList = "www.mozilla.org, www.google.com, www.apple.com, "
                       + ".domain, .domain2.org"
  prefs.setCharPref("network.proxy.no_proxies_on", hostFilterList);
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"), hostFilterList);
  
  var rv;
  // Check the hosts that should be filtered out
  var uriStrFilterList = [ "http://www.mozilla.org/",
                           "http://www.google.com/",
                           "http://www.apple.com/",
                           "http://somehost.domain/",
                           "http://someotherhost.domain/",
                           "http://somehost.domain2.org/",
                           "http://somehost.subdomain.domain2.org/" ];
  check_host_filters(uriStrFilterList, true);

  // Check the hosts that should be proxied
  var uriStrUseProxyList = [ "http://www.mozilla.com/",
                             "http://mail.google.com/",
                             "http://somehost.domain.co.uk/",
                             "http://somelocalhost/" ];  
  check_host_filters(uriStrUseProxyList, false);
  
  // Set no_proxies_on to include local hosts
  prefs.setCharPref("network.proxy.no_proxies_on", hostFilterList + ", <local>");
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"),
              hostFilterList + ", <local>");

  // Amend lists - move local domain to filtered list
  uriStrFilterList.push(uriStrUseProxyList.pop());
  check_host_filters(uriStrFilterList, true);
  check_host_filters(uriStrUseProxyList, false);

  // Cleanup
  prefs.setCharPref("network.proxy.no_proxies_on", "");
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"), "");  

  do_test_finished();
}

function run_test() {
  register_test_protocol_handler();
  run_filter_test();
  run_filter_test2();
  run_pref_test();
  run_pac_test();
  // additional tests may be added to run_test_continued
}

function run_test_continued() {
  run_pac_cancel_test();
  // additional tests may be added to run_test_continued_2
}

function run_test_continued_2() {
  run_proxy_host_filters_test();
}

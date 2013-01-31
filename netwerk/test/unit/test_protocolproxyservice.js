/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This testcase exercises the Protocol Proxy Service

// These are the major sub tests:
// run_filter_test();
// run_filter_test2()
// run_filter_test3()
// run_pref_test();
// run_pac_test();
// run_pac_cancel_test();
// run_proxy_host_filters_test();
// run_myipaddress_test();
// run_failed_script_test();
// run_isresolvable_test();

var ios = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
var pps = Components.classes["@mozilla.org/network/protocol-proxy-service;1"]
                    .getService();
var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);

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

function resolveCallback() { }
resolveCallback.prototype = {
  nextFunction: null,

  QueryInterface : function (iid) {
    const interfaces = [Components.interfaces.nsIProtocolProxyCallback,
                        Components.interfaces.nsISupports];
    if (!interfaces.some( function(v) { return iid.equals(v) } ))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  },

  onProxyAvailable : function (req, uri, pi, status) {
    this.nextFunction(pi);
  }
};

function run_filter_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Verify initial state
  var cb = new resolveCallback();
  cb.nextFunction = filter_test0_1;
  var req = pps.asyncResolve(uri, 0, cb);
}

var filter01;
var filter02;

function filter_test0_1(pi) {
  do_check_eq(pi, null);

  // Push a filter and verify the results

  filter01 = new BasicFilter();
  filter02 = new BasicFilter();
  pps.registerFilter(filter01, 10);
  pps.registerFilter(filter02, 20);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test0_2;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test0_2(pi)
{
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  pps.unregisterFilter(filter02);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test0_3;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test0_3(pi)
{
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  // Remove filter and verify that we return to the initial state

  pps.unregisterFilter(filter01);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test0_4;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test0_4(pi)
{
  do_check_eq(pi, null);
  run_filter_test2();
}

var filter11;
var filter12;

function run_filter_test2() {
  // Push a filter and verify the results

  filter11 = new TestFilter("http", "foo", 8080, 0, 10);
  filter12 = new TestFilter("http", "bar", 8090, 0, 10);
  pps.registerFilter(filter11, 20);
  pps.registerFilter(filter12, 10);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_1;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test1_1(pi) {
  check_proxy(pi, "http", "bar", 8090, 0, 10, true);
  check_proxy(pi.failoverProxy, "http", "foo", 8080, 0, 10, false);

  pps.unregisterFilter(filter12);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_2;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test1_2(pi) {
  check_proxy(pi, "http", "foo", 8080, 0, 10, false);

  // Remove filter and verify that we return to the initial state

  pps.unregisterFilter(filter11);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test1_3;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test1_3(pi) {
  do_check_eq(pi, null);
  run_filter_test3();
}

var filter_3_1;

function run_filter_test3() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Push a filter and verify the results asynchronously

  filter_3_1 = new TestFilter("http", "foo", 8080, 0, 10);
  pps.registerFilter(filter_3_1, 20);

  var cb = new resolveCallback();
  cb.nextFunction = filter_test3_1;
  var req = pps.asyncResolve(uri, 0, cb);
}

function filter_test3_1(pi) {
  check_proxy(pi, "http", "foo", 8080, 0, 10, false);
  pps.unregisterFilter(filter_3_1);
  run_pref_test();
}

function run_pref_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Verify 'direct' setting

  prefs.setIntPref("network.proxy.type", 0);

  var cb = new resolveCallback();
  cb.nextFunction = pref_test1_1;
  var req = pps.asyncResolve(uri, 0, cb);
}

function pref_test1_1(pi)
{
  do_check_eq(pi, null);

  // Verify 'manual' setting
  prefs.setIntPref("network.proxy.type", 1);

  var cb = new resolveCallback();
  cb.nextFunction = pref_test1_2;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function pref_test1_2(pi)
{
  // nothing yet configured
  do_check_eq(pi, null);

  // try HTTP configuration
  prefs.setCharPref("network.proxy.http", "foopy");
  prefs.setIntPref("network.proxy.http_port", 8080);

  var cb = new resolveCallback();
  cb.nextFunction = pref_test1_3;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function pref_test1_3(pi)
{
  check_proxy(pi, "http", "foopy", 8080, 0, -1, false);

  prefs.setCharPref("network.proxy.http", "");
  prefs.setIntPref("network.proxy.http_port", 0);

  // try SOCKS configuration
  prefs.setCharPref("network.proxy.socks", "barbar");
  prefs.setIntPref("network.proxy.socks_port", 1203);

  var cb = new resolveCallback();
  cb.nextFunction = pref_test1_4;
  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  var req = pps.asyncResolve(uri, 0, cb);
}

function pref_test1_4(pi)
{
  check_proxy(pi, "socks", "barbar", 1203, 0, -1, false);
  run_pac_test();
}

function run_protocol_handler_test() {
  var uri = ios.newURI("moz-test:foopy", null, null);

  var cb = new resolveCallback();
  cb.nextFunction = protocol_handler_test_1;
  var req = pps.asyncResolve(uri, 0, cb);
}

function protocol_handler_test_1(pi)
{
  do_check_eq(pi, null);
  prefs.setCharPref("network.proxy.autoconfig_url", "");
  prefs.setIntPref("network.proxy.type", 0);

  run_pac_cancel_test();
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

    run_protocol_handler_test();
  }
};

function run_pac_test() {
  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            '  return "PROXY foopy:8080; DIRECT";' +
            '}';
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Configure PAC

  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var req = pps.asyncResolve(uri, 0, new TestResolveCallback());
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

    prefs.setCharPref("network.proxy.autoconfig_url", "");
    prefs.setIntPref("network.proxy.type", 0);

    run_proxy_host_filters_test();
  }
};

function run_pac_cancel_test() {
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  // Configure PAC
  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            '  return "PROXY foopy:8080; DIRECT";' +
            '}';
  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var req = pps.asyncResolve(uri, 0, new TestResolveCancelationCallback());
  req.cancel(Components.results.NS_ERROR_ABORT);
}

var hostList;
var hostIDX;
var bShouldBeFiltered;
var hostNextFX;

function check_host_filters(hl, shouldBe, nextFX) {
  hostList = hl;
  hostIDX = 0;
  bShouldBeFiltered = shouldBe;
  hostNextFX = nextFX;

  if (hostList.length > hostIDX)
    check_host_filter(hostIDX);
}

function check_host_filters_cb()
{
  hostIDX++;
  if (hostList.length > hostIDX)
    check_host_filter(hostIDX);
  else
    hostNextFX();
}

function check_host_filter(i) {
  var uri;
  dump("*** uri=" + hostList[i] + " bShouldBeFiltered=" + bShouldBeFiltered + "\n");
  uri = ios.newURI(hostList[i], null, null);

  var cb = new resolveCallback();
  cb.nextFunction = host_filter_cb;
  var req = pps.asyncResolve(uri, 0, cb);
}

function host_filter_cb(proxy)
{
  if (bShouldBeFiltered) {
    do_check_eq(proxy, null);
  } else {
    do_check_neq(proxy, null);
    // Just to be sure, let's check that the proxy is correct
    // - this should match the proxy setup in the calling function
    check_proxy(proxy, "http", "foopy", 8080, 0, -1, false);
  }
  check_host_filters_cb();
}


// Verify that hists in the host filter list are not proxied
// refers to "network.proxy.no_proxies_on"

var uriStrUseProxyList;
var uriStrUseProxyList;
var hostFilterList;

function run_proxy_host_filters_test() {
  // Get prefs object from DOM
  // Setup a basic HTTP proxy configuration
  // - pps.resolve() needs this to return proxy info for non-filtered hosts
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setCharPref("network.proxy.http", "foopy");
  prefs.setIntPref("network.proxy.http_port", 8080);

  // Setup host filter list string for "no_proxies_on"
  hostFilterList = "www.mozilla.org, www.google.com, www.apple.com, "
                       + ".domain, .domain2.org"
  prefs.setCharPref("network.proxy.no_proxies_on", hostFilterList);
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"), hostFilterList);

  var rv;
  // Check the hosts that should be filtered out
  uriStrFilterList = [ "http://www.mozilla.org/",
                           "http://www.google.com/",
                           "http://www.apple.com/",
                           "http://somehost.domain/",
                           "http://someotherhost.domain/",
                           "http://somehost.domain2.org/",
                           "http://somehost.subdomain.domain2.org/" ];
  check_host_filters(uriStrFilterList, true, host_filters_1);
}

function host_filters_1()
{
  // Check the hosts that should be proxied
  uriStrUseProxyList = [ "http://www.mozilla.com/",
                             "http://mail.google.com/",
                             "http://somehost.domain.co.uk/",
                             "http://somelocalhost/" ];  
  check_host_filters(uriStrUseProxyList, false, host_filters_2);
}

function host_filters_2()
{
  // Set no_proxies_on to include local hosts
  prefs.setCharPref("network.proxy.no_proxies_on", hostFilterList + ", <local>");
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"),
              hostFilterList + ", <local>");
  // Amend lists - move local domain to filtered list
  uriStrFilterList.push(uriStrUseProxyList.pop());
  check_host_filters(uriStrFilterList, true, host_filters_3);
}

function host_filters_3()
{
  check_host_filters(uriStrUseProxyList, false, host_filters_4);
}

function host_filters_4()
{
  // Cleanup
  prefs.setCharPref("network.proxy.no_proxies_on", "");
  do_check_eq(prefs.getCharPref("network.proxy.no_proxies_on"), "");  

  run_myipaddress_test();
}

function run_myipaddress_test()
{
  // This test makes sure myIpAddress() comes up with some valid
  // IP address other than localhost. The DUT must be configured with
  // an Internet route for this to work - though no Internet traffic
  // should be created.

  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            ' return "PROXY " + myIpAddress() + ":1234";' +
            '}';

  // no traffic to this IP is ever sent, it is just a public IP that
  // does not require DNS to determine a route.
  var uri = ios.newURI("http://192.0.43.10/", null, null);

  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var cb = new resolveCallback();
  cb.nextFunction = myipaddress_callback;
  var req = pps.asyncResolve(uri, 0, cb);
}

function myipaddress_callback(pi)
{
  do_check_neq(pi, null);
  do_check_eq(pi.type, "http");
  do_check_eq(pi.port, 1234);

  // make sure we didn't return localhost
  do_check_neq(pi.host, null);
  do_check_neq(pi.host, "127.0.0.1");
  do_check_neq(pi.host, "::1");
  
  run_myipaddress_test_2();
}

function run_myipaddress_test_2()
{
  // test that myIPAddress() can be used outside of the scope of
  // FindProxyForURL(). bug 829646.

  var pac = 'data:text/plain,' +
            'var myaddr = myIpAddress(); ' +
            'function FindProxyForURL(url, host) {' +
            ' return "PROXY " + myaddr + ":5678";' +
            '}';

  var uri = ios.newURI("http://www.mozilla.org/", null, null);
  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var cb = new resolveCallback();
  cb.nextFunction = myipaddress2_callback;
  var req = pps.asyncResolve(uri, 0, cb);
}

function myipaddress2_callback(pi)
{
  do_check_neq(pi, null);
  do_check_eq(pi.type, "http");
  do_check_eq(pi.port, 5678);

  // make sure we didn't return localhost
  do_check_neq(pi.host, null);
  do_check_neq(pi.host, "127.0.0.1");
  do_check_neq(pi.host, "::1");
  
  run_failed_script_test();
}

function run_failed_script_test()
{
  // test to make sure we go direct with invalid PAC
  var pac = 'data:text/plain,' +
            '\nfor(;\n';

  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var cb = new resolveCallback();
  cb.nextFunction = failed_script_callback;
  var req = pps.asyncResolve(uri, 0, cb);
}

var directFilter;

function failed_script_callback(pi)
{
  // we should go direct
  do_check_eq(pi, null);

  // test that we honor filters when configured to go direct
  prefs.setIntPref("network.proxy.type", 0);
  directFilter = new TestFilter("http", "127.0.0.1", 7246, 0, 0);
  pps.registerFilter(directFilter, 10);

  // test that on-modify-request contains the proxy info too
  var obs = Components.classes["@mozilla.org/observer-service;1"].getService();
  obs = obs.QueryInterface(Components.interfaces.nsIObserverService);
  obs.addObserver(directFilterListener, "http-on-modify-request", false);

  var chan = ios.newChannel("http://127.0.0.1:7247", "", null);
  chan.asyncOpen(directFilterListener, chan);
}

var directFilterListener = {
  onModifyRequestCalled : false,

  onStartRequest: function test_onStart(request, ctx) {  },
  onDataAvailable: function test_OnData() { },

  onStopRequest: function test_onStop(request, ctx, status) {
    // check on the PI from the channel itself
    ctx.QueryInterface(Components.interfaces.nsIProxiedChannel);
    check_proxy(ctx.proxyInfo, "http", "127.0.0.1", 7246, 0, 0, false);
    pps.unregisterFilter(directFilter);

    // check on the PI from on-modify-request
    do_check_true(this.onModifyRequestCalled);
    var obs = Components.classes["@mozilla.org/observer-service;1"].getService();
    obs = obs.QueryInterface(Components.interfaces.nsIObserverService);
    obs.removeObserver(this, "http-on-modify-request");

    run_isresolvable_test();
  },

   observe: function(subject, topic, data) {
     if (topic === "http-on-modify-request" &&
         subject instanceof Components.interfaces.nsIHttpChannel &&
         subject instanceof Components.interfaces.nsIProxiedChannel) {
       check_proxy(subject.proxyInfo, "http", "127.0.0.1", 7246, 0, 0, false);
       this.onModifyRequestCalled = true;
     }
   }
};

function run_isresolvable_test()
{
  // test a non resolvable host in the pac file

  var pac = 'data:text/plain,' +
            'function FindProxyForURL(url, host) {' +
            ' if (isResolvable("nonexistant.lan"))' +
            '   return "DIRECT";' +
            ' return "PROXY 127.0.0.1:1234";' +
            '}';

  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  prefs.setIntPref("network.proxy.type", 2);
  prefs.setCharPref("network.proxy.autoconfig_url", pac);

  var cb = new resolveCallback();
  cb.nextFunction = isresolvable_callback;
  var req = pps.asyncResolve(uri, 0, cb);
}

function isresolvable_callback(pi)
{
  do_check_neq(pi, null);
  do_check_eq(pi.type, "http");
  do_check_eq(pi.port, 1234);
  do_check_eq(pi.host, "127.0.0.1");

  prefs.setIntPref("network.proxy.type", 0);
  do_test_finished();
}

function run_deprecated_sync_test()
{
  var uri = ios.newURI("http://www.mozilla.org/", null, null);

  pps.QueryInterface(Components.interfaces.nsIProtocolProxyService2);

  // Verify initial state
  var pi = pps.deprecatedBlockingResolve(uri, 0);
  do_check_eq(pi, null);

  // Push a filter and verify the results
  var filter1 = new BasicFilter();
  var filter2 = new BasicFilter();
  pps.registerFilter(filter1, 10);
  pps.registerFilter(filter2, 20);

  pi = pps.deprecatedBlockingResolve(uri, 0);
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  pps.unregisterFilter(filter2);
  pi = pps.deprecatedBlockingResolve(uri, 0);
  check_proxy(pi, "http", "localhost", 8080, 0, 10, true);
  check_proxy(pi.failoverProxy, "direct", "", -1, 0, 0, false);

  // Remove filter and verify that we return to the initial state
  pps.unregisterFilter(filter1);
  pi = pps.deprecatedBlockingResolve(uri, 0);
  do_check_eq(pi, null);
}

function run_test() {
  register_test_protocol_handler();

  // any synchronous tests
  run_deprecated_sync_test();

  // start of asynchronous test chain
  run_filter_test();
  do_test_pending();
}

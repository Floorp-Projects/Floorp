/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* vim: set ts=4 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.Constructor;
const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");
var serv;
var ios;

/** Example local IP addresses (literal IP address hostname).
 *
 * Note: for IPv6 Unique Local and Link Local, a wider range of addresses is
 * set aside than those most commonly used. Technically, link local addresses
 * include those beginning with fe80:: through febf::, although in practise
 * only fe80:: is used. Necko code blocks speculative connections for the wider
 * range; hence, this test considers that range too.
 */
var localIPv4Literals =
    [ // IPv4 RFC1918 \
      "10.0.0.1", "10.10.10.10", "10.255.255.255",         // 10/8
      "172.16.0.1", "172.23.172.12", "172.31.255.255",     // 172.16/20
      "192.168.0.1", "192.168.192.168", "192.168.255.255", // 192.168/16
      // IPv4 Link Local
      "169.254.0.1", "169.254.192.154", "169.254.255.255"  // 169.254/16
    ];
var localIPv6Literals =
    [ // IPv6 Unique Local fc00::/7
      "fc00::1", "fdfe:dcba:9876:abcd:ef01:2345:6789:abcd",
      "fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
      // IPv6 Link Local fe80::/10
      "fe80::1", "fe80::abcd:ef01:2345:6789",
      "febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
    ];
var localIPLiterals = localIPv4Literals.concat(localIPv6Literals);

/** Test function list and descriptions.
 */
var testList =
    [ test_speculative_connect,
      test_hostnames_resolving_to_local_addresses,
      test_proxies_with_local_addresses
    ];

var testDescription =
    [ "Expect pass with localhost",
      "Expect failure with resolved local IPs",
      "Expect failure for proxies with local IPs"
    ];

var testIdx = 0;
var hostIdx = 0;


/** TestServer
 *
 * Implements nsIServerSocket for test_speculative_connect.
 */
function TestServer() {
    this.listener = ServerSocket(-1, true, -1);
    this.listener.asyncListen(this);
}

TestServer.prototype = {
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsIServerSocket) ||
            iid.equals(Ci.nsISupports))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onSocketAccepted: function(socket, trans) {
        try { this.listener.close(); } catch(e) {}
        do_check_true(true);
        next_test();
    },

    onStopListening: function(socket) {}
};

/** TestOutputStreamCallback
 *
 * Implements nsIOutputStreamCallback for socket layer tests.
 */
function TestOutputStreamCallback(transport, hostname, proxied, expectSuccess, next) {
    this.transport = transport;
    this.hostname = hostname;
    this.proxied = proxied;
    this.expectSuccess = expectSuccess;
    this.next = next;
    this.dummyContent = "Dummy content";
}

TestOutputStreamCallback.prototype = {
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsIOutputStreamCallback) ||
            iid.equals(Ci.nsISupports))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onOutputStreamReady: function(stream) {
        do_check_neq(typeof(stream), undefined);
        try {
            stream.write(this.dummyContent, this.dummyContent.length);
        } catch (e) {
            // Spec Connect FAILED.
            do_check_instanceof(e, Ci.nsIException);
            if (this.expectSuccess) {
                // We may expect success, but the address could be unreachable
                // in the test environment, so expect errors.
                if (this.proxied) {
                    do_check_true(e.result == Cr.NS_ERROR_NET_TIMEOUT ||
                                  e.result == Cr.NS_ERROR_PROXY_CONNECTION_REFUSED);
                } else {
                    do_check_true(e.result == Cr.NS_ERROR_NET_TIMEOUT ||
                                  e.result == Cr.NS_ERROR_CONNECTION_REFUSED);
                }
            } else {
                // A refusal to connect speculatively should throw an error.
                do_check_true(e.result == Cr.NS_ERROR_UNKNOWN_HOST ||
                              e.result == Cr.NS_ERROR_UNKNOWN_PROXY_HOST ||
                              e.result == Cr.NS_ERROR_CONNECTION_REFUSED);
            }
            this.transport.close(Cr.NS_BINDING_ABORTED);
            this.next();
            return;
        }
        // Spec Connect SUCCEEDED.
        if (this.expectSuccess) {
            do_check_true(true, "Success for " + this.hostname);
        } else {
            do_throw("Speculative Connect should have failed for " +
                     this.hostname);
        }
        this.transport.close(Cr.NS_BINDING_ABORTED);
        this.next();
    }
};

/** test_speculative_connect
 *
 * Tests a basic positive case using nsIOService.SpeculativeConnect:
 * connecting to localhost.
 */
function test_speculative_connect() {
    serv = new TestServer();
    var URI = ios.newURI("http://localhost:" + serv.listener.port + "/just/a/test", null, null);
    ios.QueryInterface(Ci.nsISpeculativeConnect)
        .speculativeConnect(URI, null);
}

/* Speculative connections should not be allowed for hosts with local IP
 * addresses (Bug 853423). That list includes:
 *  -- IPv4 RFC1918 and Link Local Addresses.
 *  -- IPv6 Unique and Link Local Addresses.
 *
 * Two tests are required:
 *  1. Verify IP Literals passed to the SpeculativeConnect API.
 *  2. Verify hostnames that need to be resolved at the socket layer.
 */

/** test_hostnames_resolving_to_addresses
 *
 * Common test function for resolved hostnames. Takes a list of hosts, a
 * boolean to determine if the test is expected to succeed or fail, and a
 * function to call the next test case.
 */
function test_hostnames_resolving_to_addresses(host, expectSuccess, next) {
    do_print(host);
    var sts = Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);
    do_check_neq(typeof(sts), undefined);
    var transport = sts.createTransport(null, 0, host, 80, null);
    do_check_neq(typeof(transport), undefined);

    transport.connectionFlags = Ci.nsISocketTransport.DISABLE_RFC1918;
    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 1);
    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_READ_WRITE, 1);
    do_check_eq(1, transport.getTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT));

    var outStream = transport.openOutputStream(Ci.nsITransport.OPEN_UNBUFFERED,0,0);
    do_check_neq(typeof(outStream), undefined);

    var callback = new TestOutputStreamCallback(transport, host, false,
                                                expectSuccess,
                                                next);
    do_check_neq(typeof(callback), undefined);

    // Need to get main thread pointer to ensure nsSocketTransport::AsyncWait
    // adds callback to nsOutputStreamReadyEvent on main thread, and doesn't
    // addref off the main thread.
    var gThreadManager = Cc["@mozilla.org/thread-manager;1"]
    .getService(Ci.nsIThreadManager);
    var mainThread = gThreadManager.currentThread;

    try {
        outStream.QueryInterface(Ci.nsIAsyncOutputStream)
                 .asyncWait(callback, 0, 0, mainThread);
    } catch (e) {
        do_throw("asyncWait should not fail!");
    }
}

/**
 * test_hostnames_resolving_to_local_addresses
 *
 * Creates an nsISocketTransport and simulates a speculative connect request
 * for a hostname that resolves to a local IP address.
 * Runs asynchronously; on test success (i.e. failure to connect), the callback
 * will call this function again until all hostnames in the test list are done.
 *
 * Note: This test also uses an IP literal for the hostname. This should be ok,
 * as the socket layer will ask for the hostname to be resolved anyway, and DNS
 * code should return a numerical version of the address internally.
 */
function test_hostnames_resolving_to_local_addresses() {
    if (hostIdx >= localIPLiterals.length) {
        // No more local IP addresses; move on.
        next_test();
        return;
    }
    var host = localIPLiterals[hostIdx++];
    // Test another local IP address when the current one is done.
    var next = test_hostnames_resolving_to_local_addresses;
    test_hostnames_resolving_to_addresses(host, false, next);
}

/** test_speculative_connect_with_host_list
 *
 * Common test function for resolved proxy hosts. Takes a list of hosts, a
 * boolean to determine if the test is expected to succeed or fail, and a
 * function to call the next test case.
 */
function test_proxies(proxyHost, expectSuccess, next) {
    do_print("Proxy: " + proxyHost);
    var sts = Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);
    do_check_neq(typeof(sts), undefined);
    var pps = Cc["@mozilla.org/network/protocol-proxy-service;1"]
              .getService();
    do_check_neq(typeof(pps), undefined);

    var proxyInfo = pps.newProxyInfo("http", proxyHost, 8080, 0, 1, null);
    do_check_neq(typeof(proxyInfo), undefined);

    var transport = sts.createTransport(null, 0, "dummyHost", 80, proxyInfo);
    do_check_neq(typeof(transport), undefined);

    transport.connectionFlags = Ci.nsISocketTransport.DISABLE_RFC1918;

    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 1);
    do_check_eq(1, transport.getTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT));
    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_READ_WRITE, 1);

    var outStream = transport.openOutputStream(Ci.nsITransport.OPEN_UNBUFFERED,0,0);
    do_check_neq(typeof(outStream), undefined);

    var callback = new TestOutputStreamCallback(transport, proxyHost, true,
                                                expectSuccess,
                                                next);
    do_check_neq(typeof(callback), undefined);

    // Need to get main thread pointer to ensure nsSocketTransport::AsyncWait
    // adds callback to nsOutputStreamReadyEvent on main thread, and doesn't
    // addref off the main thread.
    var gThreadManager = Cc["@mozilla.org/thread-manager;1"]
    .getService(Ci.nsIThreadManager);
    var mainThread = gThreadManager.currentThread;

    try {
        outStream.QueryInterface(Ci.nsIAsyncOutputStream)
                 .asyncWait(callback, 0, 0, mainThread);
    } catch (e) {
        do_throw("asyncWait should not fail!");
    }
}

/**
 * test_proxies_with_local_addresses
 *
 * Creates an nsISocketTransport and simulates a speculative connect request
 * for a proxy that resolves to a local IP address.
 * Runs asynchronously; on test success (i.e. failure to connect), the callback
 * will call this function again until all proxies in the test list are done.
 *
 * Note: This test also uses an IP literal for the proxy. This should be ok,
 * as the socket layer will ask for the proxy to be resolved anyway, and DNS
 * code should return a numerical version of the address internally.
 */
function test_proxies_with_local_addresses() {
    if (hostIdx >= localIPLiterals.length) {
        // No more local IP addresses; move on.
        next_test();
        return;
    }
    var host = localIPLiterals[hostIdx++];
    // Test another local IP address when the current one is done.
    var next = test_proxies_with_local_addresses;
    test_proxies(host, false, next);
}

/** next_test
 *
 * Calls the next test in testList. Each test is responsible for calling this
 * function when its test cases are complete.
 */
function next_test() {
    if (testIdx >= testList.length) {
        // No more tests; we're done.
        do_test_finished();
        return;
    }
    do_print("SpeculativeConnect: " + testDescription[testIdx]);
    hostIdx = 0;
    // Start next test in list.
    testList[testIdx++]();
}

/** run_test
 *
 * Main entry function for test execution.
 */
function run_test() {
    // Enable speculative connects on loopback for testing.
    {
        var prefs = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefBranch);
        if (!prefs.getBoolPref("network.http.speculative.allowLoopback")) {
            prefs.setBoolPref("network.http.speculative.allowLoopback", true);
            do_register_cleanup(function() {
                prefs.setBoolPref("network.http.speculative.allowLoopback", false);
            });
        }
    }

    ios = Cc["@mozilla.org/network/io-service;1"]
        .getService(Ci.nsIIOService);

    do_test_pending();
    next_test();
}


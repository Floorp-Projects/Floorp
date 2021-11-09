"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
let h2Port = trr_test_setup();
let httpServerIPv4 = new HttpServer();
let httpServerIPv6 = new HttpServer();
let trrServer;
let testpath = "/simple";
let httpbody = "0123456789";
let CC_IPV4 = "example_cc_ipv4.com";
let CC_IPV6 = "example_cc_ipv6.com";

XPCOMUtils.defineLazyGetter(this, "URL_CC_IPV4", function() {
  return `http://${CC_IPV4}:${httpServerIPv4.identity.primaryPort}${testpath}`;
});
XPCOMUtils.defineLazyGetter(this, "URL_CC_IPV6", function() {
  return `http://${CC_IPV6}:${httpServerIPv6.identity.primaryPort}${testpath}`;
});
XPCOMUtils.defineLazyGetter(this, "URL6a", function() {
  return `http://example6a.com:${httpServerIPv6.identity.primaryPort}${testpath}`;
});
XPCOMUtils.defineLazyGetter(this, "URL6b", function() {
  return `http://example6b.com:${httpServerIPv6.identity.primaryPort}${testpath}`;
});

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const ncs = Cc[
  "@mozilla.org/network/network-connectivity-service;1"
].getService(Ci.nsINetworkConnectivityService);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  Services.prefs.clearUserPref("network.captive-portal-service.testMode");
  Services.prefs.clearUserPref("network.connectivity-service.IPv6.url");
  Services.prefs.clearUserPref("network.connectivity-service.IPv4.url");
  Services.prefs.clearUserPref("network.dns.localDomains");

  trr_clear_prefs();
  await httpServerIPv4.stop();
  await httpServerIPv6.stop();
  await trrServer.stop();
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  chan.setTRRMode(Ci.nsIRequest.TRR_DEFAULT_MODE);
  return chan;
}

function serverHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

add_task(async function test_setup() {
  httpServerIPv4.registerPathHandler(testpath, serverHandler);
  httpServerIPv4.start(-1);
  httpServerIPv6.registerPathHandler(testpath, serverHandler);
  httpServerIPv6.start_ipv6(-1);
  Services.prefs.setCharPref(
    "network.dns.localDomains",
    `foo.example.com, ${CC_IPV4}, ${CC_IPV6}`
  );

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  let hosts = ["example6a.com", "example6b.com"];
  for (const host of hosts) {
    await trrServer.registerDoHAnswers(host, "A", {
      answers: [
        {
          name: host,
          ttl: 55,
          type: "A",
          flush: false,
          data: "127.0.0.1",
        },
      ],
    });

    await trrServer.registerDoHAnswers(host, "AAAA", {
      answers: [
        {
          name: host,
          ttl: 55,
          type: "AAAA",
          flush: false,
          data: "::1",
        },
      ],
    });
  }
});

let StatusCounter = function() {
  this._statusCount = {};
};
StatusCounter.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIInterfaceRequestor",
    "nsIProgressEventSink",
  ]),

  getInterface(iid) {
    return this.QueryInterface(iid);
  },

  onProgress(request, progress, progressMax) {},
  onStatus(request, status, statusArg) {
    this._statusCount[status] = 1 + (this._statusCount[status] || 0);
  },
};

let HttpListener = function(finish, succeeded) {
  this.finish = finish;
  this.succeeded = succeeded;
};

HttpListener.prototype = {
  onStartRequest: function testOnStartRequest(request) {},

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    equal(this.succeeded, status == Cr.NS_OK);
    this.finish();
  },
};

function promiseObserverNotification(topic, matchFunc) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic, data) {
      let matches = typeof matchFunc != "function" || matchFunc(subject, data);
      if (!matches) {
        return;
      }
      Services.obs.removeObserver(observe, topic);
      resolve({ subject, data });
    }, topic);
  });
}

async function make_request(uri, check_events, succeeded) {
  let chan = makeChan(uri);
  let statusCounter = new StatusCounter();
  chan.notificationCallbacks = statusCounter;
  await new Promise(resolve =>
    chan.asyncOpen(new HttpListener(resolve, succeeded))
  );

  if (check_events) {
    equal(
      statusCounter._statusCount[0x804b000b],
      1,
      "Expecting only one instance of NS_NET_STATUS_RESOLVED_HOST"
    );
    equal(
      statusCounter._statusCount[0x804b0007],
      1,
      "Expecting only one instance of NS_NET_STATUS_CONNECTING_TO"
    );
  }
}

async function setup_connectivity(ipv6, ipv4) {
  Services.prefs.setBoolPref("network.captive-portal-service.testMode", true);

  if (ipv6) {
    Services.prefs.setCharPref(
      "network.connectivity-service.IPv6.url",
      URL_CC_IPV6 + testpath
    );
  } else {
    Services.prefs.setCharPref(
      "network.connectivity-service.IPv6.url",
      "http://donotexist.example.com"
    );
  }

  if (ipv4) {
    Services.prefs.setCharPref(
      "network.connectivity-service.IPv4.url",
      URL_CC_IPV4 + testpath
    );
  } else {
    Services.prefs.setCharPref(
      "network.connectivity-service.IPv4.url",
      "http://donotexist.example.com"
    );
  }

  let observerNotification = promiseObserverNotification(
    "network:connectivity-service:ip-checks-complete"
  );
  ncs.recheckIPConnectivity();
  await observerNotification;

  if (!ipv6) {
    equal(
      ncs.IPv6,
      Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
      "Check IPv6 support"
    );
  } else {
    equal(ncs.IPv6, Ci.nsINetworkConnectivityService.OK, "Check IPv6 support");
  }

  if (!ipv4) {
    equal(
      ncs.IPv4,
      Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
      "Check IPv4 support"
    );
  } else {
    equal(ncs.IPv4, Ci.nsINetworkConnectivityService.OK, "Check IPv4 support");
  }
}

// This test that we retry to connect using IPv4 when IPv6 connecivity is not
// present, but a ConnectionEntry have IPv6 prefered set.
// Speculative connections are disabled.
add_task(async function test_prefer_address_version_fail_trr3_1() {
  dns.clearCache(true);

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  // Make a request to setup the address version preference to a ConnectionEntry.
  await make_request(URL6a, true, true);

  // connect again using the address version preference from the ConnectionEntry.
  await make_request(URL6a, true, true);

  // Make IPv6 connectivity check fail
  await setup_connectivity(false, true);

  dns.clearCache(true);

  // This will fail, because the server is not lisenting to IPv4 address as well,
  // We should still get NS_NET_STATUS_RESOLVED_HOST and
  // NS_NET_STATUS_CONNECTING_TO notification.
  await make_request(URL6a, true, false);

  // Make IPv6 connectivity check succeed again
  await setup_connectivity(true, true);
});

// This test that we retry to connect using IPv4 when IPv6 connecivity is not
// present, but a ConnectionEntry have IPv6 prefered set.
// Speculative connections are enabled.
add_task(async function test_prefer_address_version_fail_trr3_2() {
  dns.clearCache(true);

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);

  // Make a request to setup the address version preference to a ConnectionEntry.
  await make_request(URL6b, false, true);

  // connect again using the address version preference from the ConnectionEntry.
  await make_request(URL6b, false, true);

  // Make IPv6 connectivity check fail
  await setup_connectivity(false, true);

  dns.clearCache(true);

  // This will fail, because the server is not lisenting to IPv4 address as well,
  // We should still get NS_NET_STATUS_RESOLVED_HOST and
  // NS_NET_STATUS_CONNECTING_TO notification.
  await make_request(URL6b, true, false);
});

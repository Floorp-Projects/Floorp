/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

/**
 * Waits for an observer notification to fire.
 *
 * @param {String} topicName The notification topic.
 * @returns {Promise} A promise that fulfills when the notification is fired.
 */
function promiseObserverNotification(topicName, matchFunc) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic, data) {
      let matches = typeof matchFunc != "function" || matchFunc(subject, data);
      if (!matches) {
        return;
      }
      Services.obs.removeObserver(observe, topic);
      resolve({ subject, data });
    }, topicName);
  });
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.connectivity-service.DNSv4.domain");
  Services.prefs.clearUserPref("network.connectivity-service.DNSv6.domain");
  Services.prefs.clearUserPref("network.captive-portal-service.testMode");
  Services.prefs.clearUserPref("network.connectivity-service.IPv4.url");
  Services.prefs.clearUserPref("network.connectivity-service.IPv6.url");
});

let httpserver = null;
let httpserverv6 = null;
ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpserver.identity.primaryPort + "/content";
});

ChromeUtils.defineLazyGetter(this, "URLv6", function () {
  return "http://[::1]:" + httpserverv6.identity.primaryPort + "/content";
});

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");

  const responseBody = "anybody";
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

const kDNSv6Domain =
  mozinfo.os == "linux" || mozinfo.os == "android"
    ? "ip6-localhost"
    : "localhost";

add_task(async function testDNS() {
  let ncs = Cc[
    "@mozilla.org/network/network-connectivity-service;1"
  ].getService(Ci.nsINetworkConnectivityService);

  // Set the endpoints, trigger a DNS recheck, and wait for it to complete.
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv4.domain",
    "example.org"
  );
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv6.domain",
    kDNSv6Domain
  );
  ncs.recheckDNS();
  await promiseObserverNotification(
    "network:connectivity-service:dns-checks-complete"
  );

  equal(
    ncs.DNSv4,
    Ci.nsINetworkConnectivityService.OK,
    "Check DNSv4 support (expect OK)"
  );
  equal(
    ncs.DNSv6,
    Ci.nsINetworkConnectivityService.OK,
    "Check DNSv6 support (expect OK)"
  );

  // Set the endpoints to non-exitant domains, trigger a DNS recheck, and wait for it to complete.
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv4.domain",
    "does-not-exist.example"
  );
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv6.domain",
    "does-not-exist.example"
  );
  let observerNotification = promiseObserverNotification(
    "network:connectivity-service:dns-checks-complete"
  );
  ncs.recheckDNS();
  await observerNotification;

  equal(
    ncs.DNSv4,
    Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
    "Check DNSv4 support (expect N/A)"
  );
  equal(
    ncs.DNSv6,
    Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
    "Check DNSv6 support (expect N/A)"
  );

  // Set the endpoints back to the proper domains, and simulate a captive portal
  // event.
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv4.domain",
    "example.org"
  );
  Services.prefs.setCharPref(
    "network.connectivity-service.DNSv6.domain",
    kDNSv6Domain
  );
  observerNotification = promiseObserverNotification(
    "network:connectivity-service:dns-checks-complete"
  );
  Services.obs.notifyObservers(null, "network:captive-portal-connectivity");
  // This will cause the state to go to UNKNOWN for a bit, until the check is completed.
  equal(
    ncs.DNSv4,
    Ci.nsINetworkConnectivityService.UNKNOWN,
    "Check DNSv4 support (expect UNKNOWN)"
  );
  equal(
    ncs.DNSv6,
    Ci.nsINetworkConnectivityService.UNKNOWN,
    "Check DNSv6 support (expect UNKNOWN)"
  );

  await observerNotification;

  equal(
    ncs.DNSv4,
    Ci.nsINetworkConnectivityService.OK,
    "Check DNSv4 support (expect OK)"
  );
  equal(
    ncs.DNSv6,
    Ci.nsINetworkConnectivityService.OK,
    "Check DNSv6 support (expect OK)"
  );

  httpserver = new HttpServer();
  httpserver.registerPathHandler("/content", contentHandler);
  httpserver.start(-1);

  httpserverv6 = new HttpServer();
  httpserverv6.registerPathHandler("/contentt", contentHandler);
  httpserverv6._start(-1, "[::1]");

  // Before setting the pref, this status is unknown in automation
  equal(
    ncs.IPv4,
    Ci.nsINetworkConnectivityService.UNKNOWN,
    "Check IPv4 support (expect UNKNOWN)"
  );
  equal(
    ncs.IPv6,
    Ci.nsINetworkConnectivityService.UNKNOWN,
    "Check IPv6 support (expect UNKNOWN)"
  );

  Services.prefs.setBoolPref("network.captive-portal-service.testMode", true);
  Services.prefs.setCharPref("network.connectivity-service.IPv4.url", URL);
  Services.prefs.setCharPref("network.connectivity-service.IPv6.url", URLv6);
  observerNotification = promiseObserverNotification(
    "network:connectivity-service:ip-checks-complete"
  );
  ncs.recheckIPConnectivity();
  await observerNotification;

  equal(
    ncs.IPv4,
    Ci.nsINetworkConnectivityService.OK,
    "Check IPv4 support (expect OK)"
  );
  equal(
    ncs.IPv6,
    Ci.nsINetworkConnectivityService.OK,
    "Check IPv6 support (expect OK)"
  );

  // check that the CPS status is NOT_AVAILABLE when the endpoint is down.
  await new Promise(resolve => httpserver.stop(resolve));
  await new Promise(resolve => httpserverv6.stop(resolve));
  observerNotification = promiseObserverNotification(
    "network:connectivity-service:ip-checks-complete"
  );
  Services.obs.notifyObservers(null, "network:captive-portal-connectivity");
  await observerNotification;

  equal(
    ncs.IPv4,
    Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
    "Check IPv4 support (expect NOT_AVAILABLE)"
  );
  equal(
    ncs.IPv6,
    Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
    "Check IPv6 support (expect NOT_AVAILABLE)"
  );
});

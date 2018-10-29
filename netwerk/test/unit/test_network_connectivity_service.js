/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.connectivity-service.DNSv4.domain");
  Services.prefs.clearUserPref("network.connectivity-service.DNSv6.domain");
});

const DEFAULT_WAIT_TIME = 200; // ms

const kDNSv6Domain = (mozinfo.os == "linux")
                       ? "ip6-localhost"
                       : "localhost";

add_task(async function testDNS() {
  let ncs = Cc["@mozilla.org/network/network-connectivity-service;1"]
              .getService(Ci.nsINetworkConnectivityService);

  // Set the endpoints, trigger a DNS recheck, and wait for it to complete.
  Services.prefs.setCharPref("network.connectivity-service.DNSv4.domain", "example.org");
  Services.prefs.setCharPref("network.connectivity-service.DNSv6.domain", kDNSv6Domain);
  ncs.recheckDNS();
  await new Promise(resolve => do_timeout(DEFAULT_WAIT_TIME, resolve));

  equal(ncs.DNSv4, Ci.nsINetworkConnectivityService.OK, "Check DNSv4 support (expect OK)");
  equal(ncs.DNSv6, Ci.nsINetworkConnectivityService.OK, "Check DNSv6 support (expect OK)");

  // Set the endpoints to non-exitant domains, trigger a DNS recheck, and wait for it to complete.
  Services.prefs.setCharPref("network.connectivity-service.DNSv4.domain", "does-not-exist.example");
  Services.prefs.setCharPref("network.connectivity-service.DNSv6.domain", "does-not-exist.example");
  ncs.recheckDNS();
  await new Promise(resolve => do_timeout(DEFAULT_WAIT_TIME, resolve));

  equal(ncs.DNSv4, Ci.nsINetworkConnectivityService.NOT_AVAILABLE, "Check DNSv4 support (expect N/A)");
  equal(ncs.DNSv6, Ci.nsINetworkConnectivityService.NOT_AVAILABLE, "Check DNSv6 support (expect N/A)");

  // Set the endpoints back to the proper domains, and simulate a captive portal
  // event.
  Services.prefs.setCharPref("network.connectivity-service.DNSv4.domain", "example.org");
  Services.prefs.setCharPref("network.connectivity-service.DNSv6.domain", kDNSv6Domain);
  Services.obs.notifyObservers(null, "network:captive-portal-connectivity", null);
  // This will cause the state to go to UNKNOWN for a bit, until the check is completed.
  equal(ncs.DNSv4, Ci.nsINetworkConnectivityService.UNKNOWN, "Check DNSv4 support (expect UNKNOWN)");
  equal(ncs.DNSv6, Ci.nsINetworkConnectivityService.UNKNOWN, "Check DNSv6 support (expect UNKNOWN)");

  await new Promise(resolve => do_timeout(DEFAULT_WAIT_TIME, resolve));

  equal(ncs.DNSv4, Ci.nsINetworkConnectivityService.OK, "Check DNSv4 support (expect OK)");
  equal(ncs.DNSv6, Ci.nsINetworkConnectivityService.OK, "Check DNSv6 support (expect OK)");

  // It's difficult to check when there's no connectivity in automation,
  equal(ncs.IPv4, Ci.nsINetworkConnectivityService.OK, "Check IPv4 support (expect OK)");
  equal(ncs.IPv6, Ci.nsINetworkConnectivityService.OK, "Check IPv6 support (expect OK)");
});

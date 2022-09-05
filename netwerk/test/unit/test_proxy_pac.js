// These are globlas defined for proxy servers, in ProxyAutoConfig.cpp. See
// PACGlobalFunctions
/* globals dnsResolve, alert */

"use strict";

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);
const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

class ConsoleListener {
  messages = [];
  observe(message) {
    // Ignore unexpected messages.
    if (!(message instanceof Ci.nsIConsoleMessage)) {
      return;
    }
    if (!message.message.includes("PAC")) {
      return;
    }

    this.messages.push(message.message);
  }

  register() {
    Services.console.registerListener(this);
  }

  unregister() {
    Services.console.unregisterListener(this);
  }

  clear() {
    this.messages = [];
  }
}

async function configurePac(fn) {
  let pacURI = `data:application/x-ns-proxy-autoconfig;charset=utf-8,${encodeURIComponent(
    fn.toString()
  )}`;
  Services.prefs.setIntPref("network.proxy.type", 2);
  Services.prefs.setStringPref("network.proxy.autoconfig_url", pacURI);

  // Do a request so the PAC gets loaded
  let chan = NetUtil.newChannel({
    uri: `http://localhost:1234/`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  await new Promise(resolve =>
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE))
  );

  await TestUtils.waitForCondition(
    () =>
      !!consoleListener.messages.filter(
        e => e.includes("PAC file installed from"),
        0
      ).length,
    "Wait for PAC file to be installed."
  );
  consoleListener.clear();
}

let consoleListener = null;
function setup() {
  Services.prefs.setIntPref("network.proxy.type", 2);
  Services.prefs.setStringPref("network.proxy.autoconfig_url", "");
  consoleListener = new ConsoleListener();
  consoleListener.register();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.proxy.type");
    Services.prefs.clearUserPref("network.proxy.autoconfig_url");
    if (consoleListener) {
      consoleListener.unregister();
      consoleListener = null;
    }
  });
}
setup();

// This method checks that calling dnsResult(null) does not result in
// resolving the DNS name "null"
add_task(async function test_bug1724345() {
  consoleListener.clear();
  // isInNet is defined by ascii_pac_utils.js which is included for proxies.
  /* globals isInNet */
  let pac = function FindProxyForURL(url, host) {
    alert(`PAC resolving: ${host}`);
    let destIP = dnsResolve(host);
    alert(`PAC result: ${destIP}`);
    alert(
      `PAC isInNet: ${host} ${destIP} ${isInNet(
        destIP,
        "127.0.0.0",
        "255.0.0.0"
      )}`
    );
    return "DIRECT";
  };

  await configurePac(pac);

  override.clearOverrides();
  override.addIPOverride("example.org", "N/A");
  override.addIPOverride("null", "127.0.0.1");
  dns.clearCache(true);

  let chan = NetUtil.newChannel({
    uri: `http://example.org:1234/`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  await new Promise(resolve =>
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE))
  );
  ok(
    !!consoleListener.messages.filter(e =>
      e.includes("PAC isInNet: example.org null false")
    ).length,
    `should have proper result ${consoleListener.messages}`
  );
});

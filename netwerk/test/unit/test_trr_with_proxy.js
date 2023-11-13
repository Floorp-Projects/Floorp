/* This test checks that a TRRServiceChannel can connect to the server with
   a proxy.
   Steps:
     - Setup the proxy (PAC, proxy filter, and system proxy settings)
     - Test when "network.trr.async_connInfo" is false. In this case, every
       TRRServicChannel waits for the proxy info to be resolved.
     - Test when "network.trr.async_connInfo" is true. In this case, every
       TRRServicChannel uses an already created connection info to connect.
     - The test test_trr_uri_change() is about checking if trr connection info
       is updated correctly when trr uri changed.
*/

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

/* import-globals-from trr_common.js */

let filter;
let systemProxySettings;
let trrProxy;
const pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

function setup() {
  h2Port = trr_test_setup();
  SetParentalControlEnabled(false);
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.autoconfig_url");
  Services.prefs.clearUserPref("network.trr.async_connInfo");
  if (trrProxy) {
    await trrProxy.stop();
  }
});

class ProxyFilter {
  constructor(type, host, port, flags) {
    this._type = type;
    this._host = host;
    this._port = port;
    this._flags = flags;
    this.QueryInterface = ChromeUtils.generateQI(["nsIProtocolProxyFilter"]);
  }
  applyFilter(uri, pi, cb) {
    cb.onProxyFilterResult(
      pps.newProxyInfo(
        this._type,
        this._host,
        this._port,
        "",
        "",
        this._flags,
        1000,
        null
      )
    );
  }
}

async function doTest(proxySetup, delay) {
  info("Verifying a basic A record");
  Services.dns.clearCache(true);
  // Close all previous connections.
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  setModeAndURI(2, "doh?responseIP=2.2.2.2"); // TRR-first

  trrProxy = new TRRProxy();
  await trrProxy.start(h2Port);
  info("port=" + trrProxy.port);

  await proxySetup(trrProxy.port);

  if (delay) {
    await new Promise(resolve => do_timeout(delay, resolve));
  }

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // A non-zero request count indicates that TRR requests are being routed
  // through the proxy.
  Assert.ok(
    (await trrProxy.request_count()) >= 1,
    `Request count should be at least 1`
  );

  // clean up
  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.autoconfig_url");
  if (filter) {
    pps.unregisterFilter(filter);
    filter = null;
  }
  if (systemProxySettings) {
    MockRegistrar.unregister(systemProxySettings);
    systemProxySettings = null;
  }

  await trrProxy.stop();
  trrProxy = null;
}

add_task(async function test_trr_proxy() {
  async function setupPACWithDataURL(proxyPort) {
    var pac = `data:text/plain, function FindProxyForURL(url, host) { return "HTTPS foo.example.com:${proxyPort}";}`;
    Services.prefs.setIntPref("network.proxy.type", 2);
    Services.prefs.setCharPref("network.proxy.autoconfig_url", pac);
  }

  async function setupPACWithHttpURL(proxyPort) {
    let httpserv = new HttpServer();
    httpserv.registerPathHandler("/", function handler(metadata, response) {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      let content = `function FindProxyForURL(url, host) { return "HTTPS foo.example.com:${proxyPort}";}`;
      response.setHeader("Content-Length", `${content.length}`);
      response.bodyOutputStream.write(content, content.length);
    });
    httpserv.start(-1);
    Services.prefs.setIntPref("network.proxy.type", 2);
    let pacUri = `http://127.0.0.1:${httpserv.identity.primaryPort}/`;
    Services.prefs.setCharPref("network.proxy.autoconfig_url", pacUri);

    function consoleMessageObserved() {
      return new Promise(resolve => {
        let listener = {
          QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
          observe(msg) {
            if (msg == `PAC file installed from ${pacUri}`) {
              Services.console.unregisterListener(listener);
              resolve();
            }
          },
        };
        Services.console.registerListener(listener);
      });
    }

    await consoleMessageObserved();
  }

  async function setupProxyFilter(proxyPort) {
    filter = new ProxyFilter("https", "foo.example.com", proxyPort, 0);
    pps.registerFilter(filter, 10);
  }

  async function setupSystemProxySettings(proxyPort) {
    systemProxySettings = {
      QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),
      mainThreadOnly: true,
      PACURI: null,
      getProxyForURI: (aSpec, aScheme, aHost, aPort) => {
        return `HTTPS foo.example.com:${proxyPort}`;
      },
    };

    MockRegistrar.register(
      "@mozilla.org/system-proxy-settings;1",
      systemProxySettings
    );

    Services.prefs.setIntPref(
      "network.proxy.type",
      Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM
    );

    // simulate that system proxy setting is changed.
    pps.notifyProxyConfigChangedInternal();
  }

  Services.prefs.setBoolPref("network.trr.async_connInfo", false);
  await doTest(setupPACWithDataURL);
  await doTest(setupPACWithDataURL, 1000);
  await doTest(setupPACWithHttpURL);
  await doTest(setupPACWithHttpURL, 1000);
  await doTest(setupProxyFilter);
  await doTest(setupProxyFilter, 1000);
  await doTest(setupSystemProxySettings);
  await doTest(setupSystemProxySettings, 1000);

  Services.prefs.setBoolPref("network.trr.async_connInfo", true);
  await doTest(setupPACWithDataURL);
  await doTest(setupPACWithDataURL, 1000);
  await doTest(setupPACWithHttpURL);
  await doTest(setupPACWithHttpURL, 1000);
  await doTest(setupProxyFilter);
  await doTest(setupProxyFilter, 1000);
  await doTest(setupSystemProxySettings);
  await doTest(setupSystemProxySettings, 1000);
});

add_task(async function test_trr_uri_change() {
  Services.prefs.setIntPref("network.proxy.type", 0);
  Services.prefs.setBoolPref("network.trr.async_connInfo", true);
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2", "127.0.0.1");

  await new TRRDNSListener("car.example.com", "127.0.0.1");

  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await new TRRDNSListener("car.example.net", "2.2.2.2");
});

// These are globlas defined for proxy servers, in ProxyAutoConfig.cpp. See
// PACGlobalFunctions
/* globals dnsResolve, alert */

/* This test checks that using a PAC script still works when TRR is on.
   Steps:
     - Set the pac script
     - Do a request to make sure that the script is loaded
     - Set the TRR mode
     - Make a request that would lead to running the PAC script
   We run these steps for TRR mode 2 and 3, and with fetchOffMainThread = true/false
*/

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.parse_pac_on_socket_process");
  trr_clear_prefs();
});

function FindProxyForURL(url, host) {
  alert(`PAC resolving: ${host}`);
  alert(dnsResolve(host));
  return "DIRECT";
}

XPCOMUtils.defineLazyGetter(this, "systemSettings", function() {
  return {
    QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),

    mainThreadOnly: true,
    PACURI: `data:application/x-ns-proxy-autoconfig;charset=utf-8,${encodeURIComponent(
      FindProxyForURL.toString()
    )}`,
    getProxyForURI(aURI) {
      throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
    },
  };
});

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

async function do_test_pac_dnsResolve() {
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.console.reset();
  // Create a console listener.
  let consolePromise = new Promise(resolve => {
    let listener = {
      observe(message) {
        // Ignore unexpected messages.
        if (!(message instanceof Ci.nsIConsoleMessage)) {
          return;
        }

        if (message.message.includes("PAC file installed from")) {
          Services.console.unregisterListener(listener);
          resolve();
        }
      },
    };

    Services.console.registerListener(listener);
  });

  MockRegistrar.register(
    "@mozilla.org/system-proxy-settings;1",
    systemSettings
  );
  Services.prefs.setIntPref(
    "network.proxy.type",
    Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM
  );

  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    let content = "ok";
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  httpserv.start(-1);

  Services.prefs.setBoolPref("network.dns.native-is-localhost", false);
  Services.prefs.setIntPref("network.trr.mode", 0); // Disable TRR until the PAC is loaded
  override.addIPOverride("example.org", "127.0.0.1");
  let chan = NetUtil.newChannel({
    uri: `http://example.org:${httpserv.identity.primaryPort}/`,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  await new Promise(resolve => chan.asyncOpen(new ChannelListener(resolve)));
  await consolePromise;

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  override.addIPOverride("foo.example.com", "127.0.0.1");
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=127.0.0.1`
  );

  trr_test_setup();

  async function test_with(DOMAIN, trrMode, fetchOffMainThread) {
    Services.prefs.setIntPref("network.trr.mode", trrMode); // TRR first
    Services.prefs.setBoolPref(
      "network.trr.fetch_off_main_thread",
      fetchOffMainThread
    );
    override.addIPOverride(DOMAIN, "127.0.0.1");

    chan = NetUtil.newChannel({
      uri: `http://${DOMAIN}:${httpserv.identity.primaryPort}/`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    await new Promise(resolve => chan.asyncOpen(new ChannelListener(resolve)));

    await override.clearHostOverride(DOMAIN);
  }

  await test_with("test1.com", 2, true);
  await test_with("test2.com", 3, true);
  await test_with("test3.com", 2, false);
  await test_with("test4.com", 3, false);
  await httpserv.stop();
}

add_task(async function test_pac_dnsResolve() {
  Services.prefs.setBoolPref(
    "network.proxy.parse_pac_on_socket_process",
    false
  );

  await do_test_pac_dnsResolve();

  if (mozinfo.socketprocess_networking) {
    info("run test again");
    Services.prefs.clearUserPref("network.proxy.type");
    trr_clear_prefs();
    Services.prefs.setBoolPref(
      "network.proxy.parse_pac_on_socket_process",
      true
    );
    Services.prefs.setIntPref("network.proxy.type", 2);
    Services.prefs.setIntPref("network.proxy.type", 0);
    await do_test_pac_dnsResolve();
  }
});

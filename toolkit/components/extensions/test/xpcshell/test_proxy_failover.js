"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

// Necessary for the pac script to proxy localhost requests
Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

// Pref is not builtin if direct failover is disabled in compile config.
XPCOMUtils.defineLazyGetter(this, "directFailoverDisabled", () => {
  return (
    Services.prefs.getPrefType("network.proxy.failover_direct") ==
    Ci.nsIPrefBranch.PREF_INVALID
  );
});

// Prevent the request from reaching out to the network.
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

// No hosts defined to avoid the default proxy filter setup.
const nonProxiedServer = createHttpServer();
nonProxiedServer.registerPathHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok!");
});
const { primaryHost, primaryPort } = nonProxiedServer.identity;

function getProxyData(channel) {
  if (!(channel instanceof Ci.nsIProxiedChannel)) {
    return;
  }
  let { type, host, port, sourceId } = channel.proxyInfo;
  return { type, host, port, sourceId };
}

// Get a free port with no listener to use in the proxyinfo.
function getBadProxyPort() {
  let server = new HttpServer();
  server.start(-1);
  const badPort = server.identity.primaryPort;
  server.stop();
  return badPort;
}

function xhr(url) {
  return new Promise((resolve, reject) => {
    let req = new XMLHttpRequest();
    req.open("GET", `${url}?t=${Math.random()}`);
    req.channel.QueryInterface(Ci.nsIHttpChannelInternal).beConservative = true;
    req.onload = () => {
      resolve({ text: req.responseText, proxy: getProxyData(req.channel) });
    };
    req.onerror = () => {
      reject({ status: req.status, proxy: getProxyData(req.channel) });
    };
    req.send();
  });
}

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

async function getProxyExtension(proxyDetails) {
  async function background(proxyDetails) {
    browser.proxy.onRequest.addListener(
      details => {
        return proxyDetails;
      },
      { urls: ["<all_urls>"] }
    );

    browser.test.sendMessage("proxied");
  }
  let extensionData = {
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background: `(${background})(${JSON.stringify(proxyDetails)})`,
    incognitoOverride: "spanning",
    useAddonManager: "temporary",
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("proxied");
  return extension;
}

add_task(async function test_failover_content_direct() {
  // load a content page for fetch and non-system principal, expect
  // failover to direct will work.
  const proxyDetails = [
    { type: "http", host: "127.0.0.1", port: getBadProxyPort() },
    { type: "direct" },
  ];

  // We need to load the content page before loading the proxy extension
  // to ensure that we have a valid content page to run fetch from.
  let contentUrl = `http://${primaryHost}:${primaryPort}/`;
  let page = await ExtensionTestUtils.loadContentPage(contentUrl);

  let extension = await getProxyExtension(proxyDetails);

  await ExtensionTestUtils.fetch(contentUrl, `${contentUrl}?t=${Math.random()}`)
    .then(text => {
      equal(text, "ok!", "fetch completed");
    })
    .catch(() => {
      ok(false, "fetch failed");
    });

  await extension.unload();
  await page.close();
});

add_task(
  { skip_if: () => directFailoverDisabled },
  async function test_failover_content() {
    // load a content page for fetch and non-system principal, expect
    // no failover
    const proxyDetails = [
      { type: "http", host: "127.0.0.1", port: getBadProxyPort() },
    ];

    // We need to load the content page before loading the proxy extension
    // to ensure that we have a valid content page to run fetch from.
    let contentUrl = `http://${primaryHost}:${primaryPort}/`;
    let page = await ExtensionTestUtils.loadContentPage(contentUrl);

    let extension = await getProxyExtension(proxyDetails);

    await ExtensionTestUtils.fetch(
      contentUrl,
      `${contentUrl}?t=${Math.random()}`
    )
      .then(text => {
        ok(false, "xhr unexpectedly completed");
      })
      .catch(e => {
        equal(
          e.message,
          "NetworkError when attempting to fetch resource.",
          "fetch failed"
        );
      });

    await extension.unload();
    await page.close();
  }
);

add_task(
  { skip_if: () => directFailoverDisabled },
  async function test_failover_system() {
    const proxyDetails = [
      { type: "http", host: "127.0.0.1", port: getBadProxyPort() },
      { type: "http", host: "127.0.0.1", port: getBadProxyPort() },
    ];

    let extension = await getProxyExtension(proxyDetails);

    await xhr(`http://${primaryHost}:${primaryPort}/`)
      .then(req => {
        equal(req.proxy.type, "direct", "proxy failover to direct");
        equal(req.text, "ok!", "xhr completed");
      })
      .catch(req => {
        ok(false, "xhr failed");
      });

    await extension.unload();
  }
);

add_task(
  {
    skip_if: () =>
      AppConstants.platform === "android" || directFailoverDisabled,
  },
  async function test_failover_pac() {
    const badPort = getBadProxyPort();

    async function background(badPort) {
      let pac = `function FindProxyForURL(url, host) { return "PROXY 127.0.0.1:${badPort}"; }`;
      let proxySettings = {
        proxyType: "autoConfig",
        autoConfigUrl: `data:application/x-ns-proxy-autoconfig;charset=utf-8,${encodeURIComponent(
          pac
        )}`,
      };

      await browser.proxy.settings.set({ value: proxySettings });
      browser.test.sendMessage("proxied");
    }
    let extensionData = {
      manifest: {
        permissions: ["proxy", "<all_urls>"],
      },
      background: `(${background})(${badPort})`,
      incognitoOverride: "spanning",
      useAddonManager: "temporary",
    };

    let extension = ExtensionTestUtils.loadExtension(extensionData);
    await extension.startup();
    await extension.awaitMessage("proxied");
    equal(
      Services.prefs.getIntPref("network.proxy.type"),
      2,
      "autoconfig type set"
    );
    ok(
      Services.prefs.getStringPref("network.proxy.autoconfig_url"),
      "autoconfig url set"
    );

    await xhr(`http://${primaryHost}:${primaryPort}/`)
      .then(req => {
        equal(req.proxy.type, "direct", "proxy failover to direct");
        equal(req.text, "ok!", "xhr completed");
      })
      .catch(() => {
        ok(false, "xhr failed");
      });

    await extension.unload();
  }
);

add_task(async function test_failover_system_off() {
  // Test test failover failures, uncomment and set pref to false
  Services.prefs.setBoolPref("network.proxy.failover_direct", false);

  const proxyDetails = [
    { type: "http", host: "127.0.0.1", port: getBadProxyPort() },
  ];

  let extension = await getProxyExtension(proxyDetails);

  await xhr(`http://${primaryHost}:${primaryPort}/`)
    .then(req => {
      equal(req.proxy.sourceId, extension.id, "extension matches");
      ok(false, "xhr completed");
    })
    .catch(req => {
      equal(req.proxy.sourceId, extension.id, "extension matches");
      equal(req.status, 0, "xhr failed");
    });

  await extension.unload();
});

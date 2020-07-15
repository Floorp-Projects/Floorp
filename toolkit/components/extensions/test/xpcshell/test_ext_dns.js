"use strict";

// Some test machines and android are not returning ipv6, turn it
// off to get consistent test results.
Services.prefs.setBoolPref("network.dns.disableIPv6", true);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

function getExtension(background = undefined) {
  let manifest = {
    permissions: ["dns", "proxy"],
  };
  return ExtensionTestUtils.loadExtension({
    manifest,
    background() {
      browser.test.onMessage.addListener(async (msg, data) => {
        if (msg == "proxy") {
          await browser.proxy.settings.set({ value: data });
          browser.test.sendMessage("proxied");
          return;
        }
        browser.test.log(`=== dns resolve test ${JSON.stringify(data)}`);
        browser.dns
          .resolve(data.hostname, data.flags)
          .then(result => {
            browser.test.log(
              `=== dns resolve result ${JSON.stringify(result)}`
            );
            browser.test.sendMessage("resolved", result);
          })
          .catch(e => {
            browser.test.log(`=== dns resolve error ${e.message}`);
            browser.test.sendMessage("resolved", { message: e.message });
          });
      });
      browser.test.sendMessage("ready");
    },
    incognitoOverride: "spanning",
    useAddonManager: "temporary",
  });
}

const tests = [
  {
    request: {
      hostname: "localhost",
    },
    expect: {
      addresses: ["127.0.0.1"], // ipv6 disabled , "::1"
    },
  },
  {
    request: {
      hostname: "localhost",
      flags: ["offline"],
    },
    expect: {
      addresses: ["127.0.0.1"], // ipv6 disabled , "::1"
    },
  },
  {
    request: {
      hostname: "test.example",
    },
    expect: {
      // android will error with offline
      error: /NS_ERROR_UNKNOWN_HOST|NS_ERROR_OFFLINE/,
    },
  },
  {
    request: {
      hostname: "127.0.0.1",
      flags: ["canonical_name"],
    },
    expect: {
      canonicalName: "127.0.0.1",
      addresses: ["127.0.0.1"],
    },
  },
  {
    request: {
      hostname: "localhost",
      flags: ["disable_ipv6"],
    },
    expect: {
      addresses: ["127.0.0.1"],
    },
  },
];

add_task(async function startup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_dns_resolve() {
  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("ready");

  for (let test of tests) {
    extension.sendMessage("resolve", test.request);
    let result = await extension.awaitMessage("resolved");
    if (test.expect.error) {
      ok(
        test.expect.error.test(result.message),
        `expected error ${result.message}`
      );
    } else {
      equal(
        result.canonicalName,
        test.expect.canonicalName,
        "canonicalName match"
      );
      // It seems there are platform differences happening that make this
      // testing difficult. We're going to rely on other existing dns tests to validate
      // the dns service itself works and only validate that we're getting generally
      // expected results in the webext api.
      ok(
        result.addresses.length >= test.expect.addresses.length,
        "expected number of addresses returned"
      );
      if (test.expect.addresses.length && result.addresses.length) {
        ok(
          result.addresses.includes(test.expect.addresses[0]),
          "got expected ip address"
        );
      }
    }
  }

  await extension.unload();
});

add_task(async function test_dns_resolve_socks() {
  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("ready");
  extension.sendMessage("proxy", {
    proxyType: "manual",
    socks: "127.0.0.1",
    socksVersion: 5,
    proxyDNS: true,
  });
  await extension.awaitMessage("proxied");
  equal(
    Services.prefs.getIntPref("network.proxy.type"),
    1 /* PROXYCONFIG_MANUAL */,
    "manual proxy"
  );
  equal(
    Services.prefs.getStringPref("network.proxy.socks"),
    "127.0.0.1",
    "socks proxy"
  );
  ok(
    Services.prefs.getBoolPref("network.proxy.socks_remote_dns"),
    "socks remote dns"
  );
  extension.sendMessage("resolve", {
    hostname: "mozilla.org",
  });
  let result = await extension.awaitMessage("resolved");
  ok(
    /NS_ERROR_UNKNOWN_PROXY_HOST/.test(result.message),
    `expected error ${result.message}`
  );
  await extension.unload();
});

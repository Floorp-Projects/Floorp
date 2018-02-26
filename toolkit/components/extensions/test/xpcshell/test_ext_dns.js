"use strict";

function getExtension(background = undefined) {
  let manifest = {
    "permissions": [
      "dns",
    ],
  };
  return ExtensionTestUtils.loadExtension({
    manifest,
    background() {
      browser.test.onMessage.addListener(async (msg, data) => {
        browser.dns.resolve(data.hostname, data.flags).then(result => {
          browser.test.sendMessage("resolved", result);
        }).catch(e => {
          browser.test.sendMessage("resolved", {message: e.message});
        });
      });
      browser.test.sendMessage("ready");
    },
  });
}

const tests = [
  {
    request: {
      hostname: "localhost",
    },
    expect: {
      addresses: ["127.0.0.1", "::1"],
    },
  },
  {
    request: {
      hostname: "localhost",
      flags: ["offline"],
    },
    expect: {
      addresses: ["127.0.0.1", "::1"],
    },
  },
  {
    request: {
      hostname: "test.example",
    },
    expect: {
      error: /UNKNOWN_HOST/,
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
      flags: ["disable_ipv4"],
    },
    expect: {
      addresses: ["::1"],
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

add_task(async function test_dns_resolve() {
  let extension = getExtension();
  await extension.startup();
  await extension.awaitMessage("ready");

  for (let test of tests) {
    extension.sendMessage("resolve", test.request);
    let result = await extension.awaitMessage("resolved");
    if (test.expect.error) {
      ok(test.expect.error.test(result.message), `expected error ${result.message}`);
    } else {
      equal(result.canonicalName, test.expect.canonicalName, "canonicalName match");
      equal(result.addresses.length, test.expect.addresses.length, "expected number of addresses returned");
      for (let addr of test.expect.addresses) {
        ok(result.addresses.includes(addr), `expected ip match`);
      }
    }
  }

  await extension.unload();
});

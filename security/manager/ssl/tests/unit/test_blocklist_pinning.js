"use strict";

const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);
const { RemoteSecuritySettings } = ChromeUtils.import(
  "resource://gre/modules/psm/RemoteSecuritySettings.jsm"
);

const sss = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

const { PinningBlocklistClient } = RemoteSecuritySettings.init();

add_task(async function test_uses_a_custom_signer() {
  Assert.notEqual(
    PinningBlocklistClient.signerName,
    RemoteSettings("not-specified").signerName
  );
});

add_task(async function test_pinning_has_initial_dump() {
  if (AppConstants.platform == "android") {
    // Skip test: we don't ship pinning dumps on Android (see package-manifest).
    return;
  }
  Assert.ok(
    await Utils.hasLocalDump(
      PinningBlocklistClient.bucketName,
      PinningBlocklistClient.collectionName
    )
  );
});

add_task(async function test_default_jexl_filter_is_used() {
  Assert.deepEqual(
    PinningBlocklistClient.filterFunc,
    RemoteSettings("not-specified").filterFunc
  );
});

add_task(async function test_no_pins_by_default() {
  // ensure our pins are all missing before we start
  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://one.example.com"),
      0
    )
  );
  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://two.example.com"),
      0
    )
  );
  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://three.example.com"),
      0
    )
  );
  ok(
    !sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://four.example.com"),
      0
    )
  );
  ok(
    !sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://five.example.com"),
      0
    )
  );
});

add_task(async function test_simple_pin_domain() {
  const current = [
    {
      pinType: "KeyPin",
      hostName: "one.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      pins: [
        "cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
        "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE=",
      ],
      versions: [Services.appinfo.version],
    },
  ];
  await PinningBlocklistClient.emit("sync", { data: { current } });

  // check that a pin exists for one.example.com
  ok(
    sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://one.example.com"),
      0
    )
  );
});

add_task(async function test_existing_entries_are_erased() {
  const current = [];
  await PinningBlocklistClient.emit("sync", { data: { current } });

  // check that no pin exists for one.example.com
  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://one.example.com"),
      0
    )
  );
});

add_task(async function test_multiple_entries() {
  const current = [
    {
      pinType: "KeyPin",
      hostName: "two.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      pins: [
        "cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
        "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE=",
      ],
      versions: [Services.appinfo.version],
    },
    {
      pinType: "KeyPin",
      hostName: "three.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      pins: [
        "cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
        "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE=",
      ],
      versions: [
        Services.appinfo.version,
        "some other version that won't match",
      ],
    },
    {
      pinType: "KeyPin",
      hostName: "four.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      pins: [
        "cUPcTAZWKaASuYWhhneDttWpY3oBAkE3h2+soZS7sWs=",
        "M8HztCzM3elUxkcjR2S5P4hhyBNf6lHkmjAHKhpGPWE=",
      ],
      versions: ["some version that won't match"],
    },
    {
      pinType: "STSPin",
      hostName: "five.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      versions: [Services.appinfo.version, "some version that won't match"],
    },
  ];
  await PinningBlocklistClient.emit("sync", { data: { current } });

  // check that a pin exists for two.example.com and three.example.com
  ok(
    sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://two.example.com"),
      0
    )
  );
  ok(
    sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://three.example.com"),
      0
    )
  );
  // check that a pin does not exist for four.example.com - it's in the
  // collection but the version should not match
  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://four.example.com"),
      0
    )
  );
  // Check that the HSTS preload added to the collection works...
  ok(
    sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://five.example.com"),
      0
    )
  );
  // // ...and that includeSubdomains is honored
  ok(
    !sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://subdomain.five.example.com"),
      0
    )
  );

  // Overwrite existing entries.
  current[current.length - 1].includeSubdomains = true;
  await PinningBlocklistClient.emit("sync", { data: { current } });
  // The STS entry for five.example.com now has includeSubdomains set;
  // ensure that the new includeSubdomains value is honored.
  ok(
    sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://subdomain.five.example.com"),
      0
    )
  );
});

add_task(async function test_bad_entries() {
  const current = [
    {
      irrelevant: "this entry looks nothing whatsoever like a pin preload",
      pinType: "KeyPin",
    },
    {
      irrelevant: "this entry has data of the wrong type",
      pinType: "KeyPin",
      hostName: 3,
      includeSubdomains: "nonsense",
      expires: "more nonsense",
      pins: [1, 2, 3, 4],
    },
    {
      irrelevant: "this entry is missing the actual pins",
      pinType: "KeyPin",
      hostName: "missingpins.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      versions: [Services.appinfo.version],
    },
    {
      pinType: "STSPin",
      hostName: "five.example.com",
      includeSubdomains: true,
      expires: new Date().getTime() + 1000000,
    }, // missing versions.
  ];
  // The event listener will catch any error, and won't throw.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1554939
  await PinningBlocklistClient.emit("sync", { data: { current } });

  ok(
    !sss.isSecureURI(
      sss.HEADER_HPKP,
      Services.io.newURI("https://missingpins.example.com"),
      0
    )
  );

  // Check that the HSTS preload overwrites existing entries...
  // Version field is missing.
  ok(
    !sss.isSecureURI(
      sss.HEADER_HSTS,
      Services.io.newURI("https://five.example.com"),
      0
    )
  );
});

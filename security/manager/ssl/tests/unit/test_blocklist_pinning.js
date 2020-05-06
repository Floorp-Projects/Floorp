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

add_task(async function test_multiple_entries() {
  const current = [
    {
      pinType: "STSPin",
      hostName: "five.example.com",
      includeSubdomains: false,
      expires: new Date().getTime() + 1000000,
      versions: [Services.appinfo.version, "some version that won't match"],
    },
  ];
  await PinningBlocklistClient.emit("sync", { data: { current } });

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
      pinType: "STSPin",
      hostName: "five.example.com",
      includeSubdomains: true,
      expires: new Date().getTime() + 1000000,
    }, // missing versions.
  ];
  // The event listener will catch any error, and won't throw.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1554939
  await PinningBlocklistClient.emit("sync", { data: { current } });

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

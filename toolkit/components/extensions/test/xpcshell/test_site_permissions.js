/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

async function _test_manifest(manifest, expectedError) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest(
    manifest,
    "manifest.WebExtensionSitePermissionsManifest"
  );
  ExtensionTestUtils.failOnSchemaWarnings(true);

  if (expectedError) {
    ok(
      normalized.error.includes(expectedError),
      `The manifest error ${JSON.stringify(
        normalized.error
      )} must contain ${JSON.stringify(expectedError)}`
    );
  } else {
    equal(normalized.error, undefined, "Should not have an error");
  }
  equal(normalized.errors.length, 0, "Should have no warning");
}

add_task(async function test_manifest_site_permissions() {
  await _test_manifest({
    site_permissions: ["midi"],
    install_origins: ["http://example.com"],
  });
  await _test_manifest({
    site_permissions: ["midi-sysex"],
    install_origins: ["http://example.com"],
  });
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
      install_origins: ["http://example.com"],
    },
    `Error processing site_permissions.0: Invalid enumeration value "unknown_site_permission"`
  );
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
      install_origins: [],
    },
    `Error processing install_origins: Array requires at least 1 items;`
  );
  await _test_manifest(
    {
      site_permissions: ["unknown_site_permission"],
    },
    `Property "install_origins" is required`
  );
  await _test_manifest(
    {
      install_origins: ["http://example.com"],
    },
    `Property "site_permissions" is required`
  );
  // test any extra manifest entries not part of a site permissions addon will cause an error.
  await _test_manifest(
    {
      site_permissions: ["midi"],
      install_origins: ["http://example.com"],
      permissions: ["webRequest"],
    },
    `Unexpected property`
  );
});

async function _test_ext_site_permissions(site_permissions, install_origins) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      install_origins,
      site_permissions,
    },
  });
  await extension.startup();
  await extension.unload();
  ExtensionTestUtils.failOnSchemaWarnings(true);
}

add_task(async function test_ext_site_permissions() {
  await _test_ext_site_permissions(["midi"], ["http://example.com"]);

  await _test_ext_site_permissions(
    ["midi"],
    ["http://example.com", "http://foo.com"]
  ).catch(e => {
    Assert.ok(
      e.message.includes(
        "Error processing install_origins: Array requires at most 1 items; you have 2"
      ),
      "Site permissions can only contain one install origin: "
    );
  });
});

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const ADDON_ID = "test@web.extension";

const aps = Cc["@mozilla.org/addons/policy-service;1"].getService(
  Ci.nsIAddonPolicyService
);

let policy = null;

function setExtensionCSP(csp) {
  if (policy) {
    policy.active = false;
  }

  policy = new WebExtensionPolicy({
    id: ADDON_ID,
    mozExtensionHostname: ADDON_ID,
    baseURL: "file:///",

    allowedOrigins: new MatchPatternSet([]),
    localizeCallback() {},

    extensionPageCSP: csp,
    contentScriptCSP: csp,
  });

  policy.active = true;
}

registerCleanupFunction(() => {
  policy.active = false;
});

add_task(async function test_addon_csp() {
  equal(
    aps.baseCSP,
    Preferences.get("extensions.webextensions.base-content-security-policy"),
    "Expected base CSP value"
  );

  equal(
    aps.defaultCSP,
    Preferences.get("extensions.webextensions.default-content-security-policy"),
    "Expected default CSP value"
  );

  const CUSTOM_POLICY =
    "script-src: 'self' https://xpcshell.test.custom.csp; object-src: 'none'";

  setExtensionCSP(CUSTOM_POLICY);

  equal(
    aps.getExtensionPageCSP(ADDON_ID),
    CUSTOM_POLICY,
    "CSP should point to add-on's custom extension page policy"
  );

  equal(
    aps.getContentScriptCSP(ADDON_ID),
    CUSTOM_POLICY,
    "CSP should point to add-on's custom content script policy"
  );

  setExtensionCSP(null);

  equal(
    aps.getExtensionPageCSP(ADDON_ID),
    aps.defaultCSP,
    "extension page CSP should be default when set to null"
  );

  equal(
    aps.getContentScriptCSP(ADDON_ID),
    aps.defaultCSP,
    "content script CSP should be default when set to null"
  );
});

add_task(async function test_invalid_csp() {
  let defaultPolicy = Preferences.get(
    "extensions.webextensions.default-content-security-policy"
  );
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_security_policy: {
        extension_pages: `script-src 'none'`,
        content_scripts: `script-src 'none'`,
      },
    },
  });
  await extension.startup();
  let policy = WebExtensionPolicy.getByID(extension.id);
  equal(
    policy.extensionPageCSP,
    defaultPolicy,
    "csp is default when invalid csp is provided."
  );
  equal(
    policy.contentScriptCSP,
    defaultPolicy,
    "csp is default when invalid csp is provided."
  );
  await extension.unload();
  ExtensionTestUtils.failOnSchemaWarnings(true);
});

add_task(async function test_isolated_world() {
  const test_policy = "script-src 'self'; object-src 'none'; img-src 'none'";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_security_policy: {
        isolated_world: test_policy,
      },
    },
  });
  await extension.startup();
  let policy = WebExtensionPolicy.getByID(extension.id);
  equal(
    policy.contentScriptCSP,
    test_policy,
    "csp is is correct when using isolated_world."
  );
  await extension.unload();
});

// If both isolated_world and content_scripts is provided, content_scripts is used.
add_task(async function test_isolated_world_overridden() {
  const test_policy =
    "script-src 'self'; object-src 'none'; img-src https://xpcshell.test.custom.csp";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_security_policy: {
        content_scripts: test_policy,
        isolated_world: "script-src 'self'; object-src 'none'; img-src 'none'",
      },
    },
  });
  await extension.startup();
  let policy = WebExtensionPolicy.getByID(extension.id);
  equal(
    policy.contentScriptCSP,
    test_policy,
    "csp is is correct when using isolated_world and content_scripts."
  );
  await extension.unload();
});

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

const v2_csp = Preferences.get(
  "extensions.webextensions.base-content-security-policy"
);
const v3_csp = Preferences.get(
  "extensions.webextensions.base-content-security-policy.v3"
);

add_task(async function test_invalid_addon_csp() {
  await Assert.throws(
    () => aps.getBaseCSP("invalid@missing"),
    /NS_ERROR_ILLEGAL_VALUE/,
    "no base csp for non-existent addon"
  );
  await Assert.throws(
    () => aps.getExtensionPageCSP("invalid@missing"),
    /NS_ERROR_ILLEGAL_VALUE/,
    "no extension page csp for non-existent addon"
  );
});

add_task(async function test_policy_csp() {
  equal(
    aps.defaultCSP,
    Preferences.get("extensions.webextensions.default-content-security-policy"),
    "Expected default CSP value"
  );

  const CUSTOM_POLICY =
    "script-src: 'self' https://xpcshell.test.custom.csp; object-src: 'none'";

  let tests = [
    {
      name: "manifest version 2, no custom policy",
      policyData: {},
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest version 2, no custom policy",
      policyData: {
        manifestVersion: 2,
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "version 2 custom extension policy",
      policyData: {
        extensionPageCSP: CUSTOM_POLICY,
      },
      expectedPolicy: CUSTOM_POLICY,
    },
    {
      name: "manifest version 2 set, custom extension policy",
      policyData: {
        manifestVersion: 2,
        extensionPageCSP: CUSTOM_POLICY,
      },
      expectedPolicy: CUSTOM_POLICY,
    },
    {
      name: "manifest version 3, no custom policy",
      policyData: {
        manifestVersion: 3,
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest 3 version set, custom extensionPage policy",
      policyData: {
        manifestVersion: 3,
        extensionPageCSP: CUSTOM_POLICY,
      },
      expectedPolicy: CUSTOM_POLICY,
    },
  ];

  let policy = null;

  function setExtensionCSP({ manifestVersion, extensionPageCSP }) {
    if (policy) {
      policy.active = false;
    }

    policy = new WebExtensionPolicy({
      id: ADDON_ID,
      mozExtensionHostname: ADDON_ID,
      baseURL: "file:///",

      allowedOrigins: new MatchPatternSet([]),
      localizeCallback() {},

      manifestVersion,
      extensionPageCSP,
    });

    policy.active = true;
  }

  for (let test of tests) {
    info(test.name);
    setExtensionCSP(test.policyData);
    equal(
      aps.getBaseCSP(ADDON_ID),
      test.policyData.manifestVersion == 3 ? v3_csp : v2_csp,
      "baseCSP is correct"
    );
    equal(
      aps.getExtensionPageCSP(ADDON_ID),
      test.expectedPolicy,
      "extensionPageCSP is correct"
    );
  }
});

add_task(async function test_extension_csp() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

  ExtensionTestUtils.failOnSchemaWarnings(false);

  let extension_pages = "script-src 'self'; object-src 'none'; img-src 'none'";

  let tests = [
    {
      name: "manifest_v2 invalid csp results in default csp used",
      manifest: {
        content_security_policy: `script-src 'none'`,
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v2 allows https protocol",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' https://*; object-src 'self'`,
        },
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v2 allows unsafe-eval",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' 'unsafe-eval'; object-src 'self'`,
        },
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 invalid csp results in default csp used",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'none'`,
        },
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 forbidden protocol results in default csp used",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' https://*; object-src 'self'`,
        },
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 forbidden eval results in default csp used",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' 'unsafe-eval'; object-src 'self'`,
        },
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 allows localhost",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' https://localhost; object-src 'self'`,
        },
      },
      expectedPolicy: `script-src 'self' https://localhost; object-src 'self'`,
    },
    {
      name: "manifest_v3 allows 127.0.0.1",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages: `script-src 'self' https://127.0.0.1; object-src 'self'`,
        },
      },
      expectedPolicy: `script-src 'self' https://127.0.0.1; object-src 'self'`,
    },
    {
      name: "manifest_v2 csp",
      manifest: {
        manifest_version: 2,
        content_security_policy: extension_pages,
      },
      expectedPolicy: extension_pages,
    },
    {
      name: "manifest_v2 with no csp, expect default",
      manifest: {
        manifest_version: 2,
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 used with no csp, expect default",
      manifest: {
        manifest_version: 3,
      },
      expectedPolicy: aps.defaultCSP,
    },
    {
      name: "manifest_v3 syntax used",
      manifest: {
        manifest_version: 3,
        content_security_policy: {
          extension_pages,
        },
      },
      expectedPolicy: extension_pages,
    },
  ];

  for (let test of tests) {
    info(test.name);
    let extension = ExtensionTestUtils.loadExtension({
      manifest: test.manifest,
    });
    await extension.startup();
    let policy = WebExtensionPolicy.getByID(extension.id);
    equal(
      policy.baseCSP,
      test.manifest.manifest_version == 3 ? v3_csp : v2_csp,
      "baseCSP is correct"
    );
    equal(
      policy.extensionPageCSP,
      test.expectedPolicy,
      "extensionPageCSP is correct."
    );
    await extension.unload();
  }

  ExtensionTestUtils.failOnSchemaWarnings(true);

  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});

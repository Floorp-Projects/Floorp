"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

// ExtensionParent.jsm is being imported lazily because when it is imported Services.appinfo will be
// retrieved and cached (as a side-effect of Schemas.jsm being imported), and so Services.appinfo
// will not be returning the version set by AddonTestUtils.createAppInfo and this test will
// fail on non-nightly builds (because the cached appinfo.version will be undefined and
// AddonManager startup will fail).
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

const BROWSER_PROPERTIES = "chrome://browser/locale/browser.properties";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  // Bug 1646182: Force ExtensionPermissions to run in rkv mode, the legacy
  // storage mode will run in xpcshell-legacy-ep.ini
  await ExtensionPermissions._uninit();

  optionalPermissionsPromptHandler.init();

  await AddonTestUtils.promiseStartupManager();
  AddonTestUtils.usePrivilegedSignatures = false;
});

add_task(async function test_permissions_on_startup() {
  let extensionId = "@permissionTest";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: { id: extensionId },
      },
      permissions: ["tabs"],
    },
    useAddonManager: "permanent",
    async background() {
      let perms = await browser.permissions.getAll();
      browser.test.sendMessage("permissions", perms);
    },
  });
  let adding = {
    permissions: ["internal:privateBrowsingAllowed"],
    origins: [],
  };
  await extension.startup();
  let perms = await extension.awaitMessage("permissions");
  equal(perms.permissions.length, 1, "one permission");
  equal(perms.permissions[0], "tabs", "internal permission not present");

  const { StartupCache } = ExtensionParent;

  // StartupCache.permissions will not contain the extension permissions.
  let manifestData = await StartupCache.permissions.get(extensionId, () => {
    return { permissions: [], origins: [] };
  });
  equal(manifestData.permissions.length, 0, "no permission");

  perms = await ExtensionPermissions.get(extensionId);
  equal(perms.permissions.length, 0, "no permissions");
  await ExtensionPermissions.add(extensionId, adding);

  // Restart the extension and re-test the permissions.
  await ExtensionPermissions._uninit();
  await AddonTestUtils.promiseRestartManager();
  let restarted = extension.awaitMessage("permissions");
  await extension.awaitStartup();
  perms = await restarted;

  manifestData = await StartupCache.permissions.get(extensionId, () => {
    return { permissions: [], origins: [] };
  });
  deepEqual(
    manifestData.permissions,
    adding.permissions,
    "StartupCache.permissions contains permission"
  );

  equal(perms.permissions.length, 1, "one permission");
  equal(perms.permissions[0], "tabs", "internal permission not present");
  let added = await ExtensionPermissions._get(extensionId);
  deepEqual(added, adding, "permissions were retained");

  await extension.unload();
});

async function test_permissions({
  manifest_version,
  granted_host_permissions,
  useAddonManager,
  expectAllGranted,
}) {
  const REQUIRED_PERMISSIONS = ["downloads"];
  const REQUIRED_ORIGINS = ["*://site.com/", "*://*.domain.com/"];
  const REQUIRED_ORIGINS_EXPECTED = expectAllGranted
    ? ["*://site.com/*", "*://*.domain.com/*"]
    : [];

  const OPTIONAL_PERMISSIONS = ["idle", "clipboardWrite"];
  const OPTIONAL_ORIGINS = [
    "http://optionalsite.com/",
    "https://*.optionaldomain.com/",
  ];
  const OPTIONAL_ORIGINS_NORMALIZED = [
    "http://optionalsite.com/*",
    "https://*.optionaldomain.com/*",
  ];

  function background() {
    browser.test.onMessage.addListener(async (method, arg) => {
      if (method == "getAll") {
        let perms = await browser.permissions.getAll();
        let url = browser.runtime.getURL("*");
        perms.origins = perms.origins.filter(i => i != url);
        browser.test.sendMessage("getAll.result", perms);
      } else if (method == "contains") {
        let result = await browser.permissions.contains(arg);
        browser.test.sendMessage("contains.result", result);
      } else if (method == "request") {
        try {
          let result = await browser.permissions.request(arg);
          browser.test.sendMessage("request.result", {
            status: "success",
            result,
          });
        } catch (err) {
          browser.test.sendMessage("request.result", {
            status: "error",
            message: err.message,
          });
        }
      } else if (method == "remove") {
        let result = await browser.permissions.remove(arg);
        browser.test.sendMessage("remove.result", result);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version,
      permissions: REQUIRED_PERMISSIONS,
      host_permissions: REQUIRED_ORIGINS,
      optional_permissions: [...OPTIONAL_PERMISSIONS, ...OPTIONAL_ORIGINS],
      granted_host_permissions,
    },
    useAddonManager,
  });

  await extension.startup();

  function call(method, arg) {
    extension.sendMessage(method, arg);
    return extension.awaitMessage(`${method}.result`);
  }

  let result = await call("getAll");
  deepEqual(result.permissions, REQUIRED_PERMISSIONS);
  deepEqual(result.origins, REQUIRED_ORIGINS_EXPECTED);

  for (let perm of REQUIRED_PERMISSIONS) {
    result = await call("contains", { permissions: [perm] });
    equal(result, true, `contains() returns true for fixed permission ${perm}`);
  }
  for (let origin of REQUIRED_ORIGINS) {
    result = await call("contains", { origins: [origin] });
    equal(
      result,
      expectAllGranted,
      `contains() returns true for fixed origin ${origin}`
    );
  }

  // None of the optional permissions should be available yet
  for (let perm of OPTIONAL_PERMISSIONS) {
    result = await call("contains", { permissions: [perm] });
    equal(result, false, `contains() returns false for permission ${perm}`);
  }
  for (let origin of OPTIONAL_ORIGINS) {
    result = await call("contains", { origins: [origin] });
    equal(result, false, `contains() returns false for origin ${origin}`);
  }

  result = await call("contains", {
    permissions: [...REQUIRED_PERMISSIONS, ...OPTIONAL_PERMISSIONS],
  });
  equal(
    result,
    false,
    "contains() returns false for a mix of available and unavailable permissions"
  );

  let perm = OPTIONAL_PERMISSIONS[0];
  result = await call("request", { permissions: [perm] });
  equal(
    result.status,
    "error",
    "request() fails if not called from an event handler"
  );
  ok(
    /request may only be called from a user input handler/.test(result.message),
    "error message for calling request() outside an event handler is reasonable"
  );
  result = await call("contains", { permissions: [perm] });
  equal(
    result,
    false,
    "Permission requested outside an event handler was not granted"
  );

  await withHandlingUserInput(extension, async () => {
    result = await call("request", { permissions: ["notifications"] });
    equal(
      result.status,
      "error",
      "request() for permission not in optional_permissions should fail"
    );
    ok(
      /since it was not declared in optional_permissions/.test(result.message),
      "error message for undeclared optional_permission is reasonable"
    );

    // Check request() when the prompt is canceled.
    optionalPermissionsPromptHandler.acceptPrompt = false;
    result = await call("request", { permissions: [perm] });
    equal(result.status, "success", "request() returned cleanly");
    equal(
      result.result,
      false,
      "request() returned false for rejected permission"
    );

    result = await call("contains", { permissions: [perm] });
    equal(result, false, "Rejected permission was not granted");

    // Call request() and accept the prompt
    optionalPermissionsPromptHandler.acceptPrompt = true;
    let allOptional = {
      permissions: OPTIONAL_PERMISSIONS,
      origins: OPTIONAL_ORIGINS,
    };
    result = await call("request", allOptional);
    equal(result.status, "success", "request() returned cleanly");
    equal(
      result.result,
      true,
      "request() returned true for accepted permissions"
    );

    // Verify that requesting a permission/origin in the wrong field fails
    let originsAsPerms = {
      permissions: OPTIONAL_ORIGINS,
    };
    let permsAsOrigins = {
      origins: OPTIONAL_PERMISSIONS,
    };

    result = await call("request", originsAsPerms);
    equal(
      result.status,
      "error",
      "Requesting an origin as a permission should fail"
    );
    ok(
      /Type error for parameter permissions \(Error processing permissions/.test(
        result.message
      ),
      "Error message for origin as permission is reasonable"
    );

    result = await call("request", permsAsOrigins);
    equal(
      result.status,
      "error",
      "Requesting a permission as an origin should fail"
    );
    ok(
      /Type error for parameter permissions \(Error processing origins/.test(
        result.message
      ),
      "Error message for permission as origin is reasonable"
    );
  });

  let allPermissions = {
    permissions: [...REQUIRED_PERMISSIONS, ...OPTIONAL_PERMISSIONS],
    origins: [...REQUIRED_ORIGINS_EXPECTED, ...OPTIONAL_ORIGINS_NORMALIZED],
  };

  result = await call("getAll");
  deepEqual(
    result,
    allPermissions,
    "getAll() returns required and runtime requested permissions"
  );

  result = await call("contains", allPermissions);
  equal(
    result,
    true,
    "contains() returns true for runtime requested permissions"
  );

  // Restart extension, verify permissions are still present.
  if (useAddonManager === "permanent") {
    await AddonTestUtils.promiseRestartManager();
  } else {
    // Manually reload for temporarily loaded.
    await extension.addon.reload();
  }
  await extension.awaitBackgroundStarted();

  result = await call("getAll");
  deepEqual(
    result,
    allPermissions,
    "Runtime requested permissions are still present after restart"
  );

  // Check remove()
  result = await call("remove", { permissions: OPTIONAL_PERMISSIONS });
  equal(result, true, "remove() succeeded");

  let perms = {
    permissions: REQUIRED_PERMISSIONS,
    origins: [...REQUIRED_ORIGINS_EXPECTED, ...OPTIONAL_ORIGINS_NORMALIZED],
  };

  result = await call("getAll");
  deepEqual(result, perms, "Expected permissions remain after removing some");

  result = await call("remove", { origins: OPTIONAL_ORIGINS });
  equal(result, true, "remove() succeeded");

  perms.origins = REQUIRED_ORIGINS_EXPECTED;
  result = await call("getAll");
  deepEqual(result, perms, "Back to default permissions after removing more");

  await extension.unload();
}

add_task(function test_normal_mv2() {
  return test_permissions({
    manifest_version: 2,
    useAddonManager: "permanent",
    expectAllGranted: true,
  });
});

add_task(function test_normal_mv3() {
  return test_permissions({
    manifest_version: 3,
    useAddonManager: "permanent",
    expectAllGranted: false,
  });
});

add_task(function test_granted_for_temporary_mv3() {
  return test_permissions({
    manifest_version: 3,
    granted_host_permissions: true,
    useAddonManager: "temporary",
    expectAllGranted: true,
  });
});

add_task(async function test_granted_only_for_privileged_mv3() {
  try {
    // For permanent non-privileged, granted_host_permissions does nothing.
    await test_permissions({
      manifest_version: 3,
      granted_host_permissions: true,
      useAddonManager: "permanent",
      expectAllGranted: false,
    });

    // Make extensions loaded with addon manager privileged.
    AddonTestUtils.usePrivilegedSignatures = true;

    await test_permissions({
      manifest_version: 3,
      granted_host_permissions: true,
      useAddonManager: "permanent",
      expectAllGranted: true,
    });
  } finally {
    AddonTestUtils.usePrivilegedSignatures = false;
  }
});

add_task(async function test_startup() {
  async function background() {
    browser.test.onMessage.addListener(async perms => {
      await browser.permissions.request(perms);
      browser.test.sendMessage("requested");
    });

    let all = await browser.permissions.getAll();
    let url = browser.runtime.getURL("*");
    all.origins = all.origins.filter(i => i != url);
    browser.test.sendMessage("perms", all);
  }

  const PERMS1 = {
    permissions: ["clipboardRead", "tabs"],
  };
  const PERMS2 = {
    origins: ["https://site2.com/*"],
  };

  let extension1 = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: PERMS1.permissions,
    },
    useAddonManager: "permanent",
  });
  let extension2 = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      optional_permissions: PERMS2.origins,
    },
    useAddonManager: "permanent",
  });

  await extension1.startup();
  await extension2.startup();

  let perms = await extension1.awaitMessage("perms");
  perms = await extension2.awaitMessage("perms");

  await withHandlingUserInput(extension1, async () => {
    extension1.sendMessage(PERMS1);
    await extension1.awaitMessage("requested");
  });

  await withHandlingUserInput(extension2, async () => {
    extension2.sendMessage(PERMS2);
    await extension2.awaitMessage("requested");
  });

  // Restart everything, and force the permissions store to be
  // re-read on startup
  await ExtensionPermissions._uninit();
  await AddonTestUtils.promiseRestartManager();
  await extension1.awaitStartup();
  await extension2.awaitStartup();

  async function checkPermissions(extension, permissions) {
    perms = await extension.awaitMessage("perms");
    let expect = Object.assign({ permissions: [], origins: [] }, permissions);
    deepEqual(perms, expect, "Extension got correct permissions on startup");
  }

  await checkPermissions(extension1, PERMS1);
  await checkPermissions(extension2, PERMS2);

  await extension1.unload();
  await extension2.unload();
});

// Test that we don't prompt for permissions an extension already has.
async function test_alreadyGranted(manifest_version) {
  const REQUIRED_PERMISSIONS = ["geolocation"];
  const REQUIRED_ORIGINS = [
    "*://required-host.com/",
    "*://*.required-domain.com/",
  ];
  const OPTIONAL_PERMISSIONS = [
    ...REQUIRED_PERMISSIONS,
    ...REQUIRED_ORIGINS,
    "clipboardRead",
    "*://optional-host.com/",
    "*://*.optional-domain.com/",
  ];

  function pageScript() {
    browser.test.onMessage.addListener(async (msg, arg) => {
      if (msg == "request") {
        let result = await browser.permissions.request(arg);
        browser.test.sendMessage("request.result", result);
      } else if (msg == "remove") {
        let result = await browser.permissions.remove(arg);
        browser.test.sendMessage("remove.result", result);
      } else if (msg == "close") {
        window.close();
      }
    });

    browser.test.sendMessage("page-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
    },

    manifest: {
      manifest_version,
      permissions: REQUIRED_PERMISSIONS,
      host_permissions: REQUIRED_ORIGINS,
      optional_permissions: OPTIONAL_PERMISSIONS,
      granted_host_permissions: true,
    },
    temporarilyInstalled: true,

    files: {
      "page.html": `<html><head>
          <script src="page.js"><\/script>
        </head></html>`,

      "page.js": pageScript,
    },
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    let url = await extension.awaitMessage("ready");
    let page = await ExtensionTestUtils.loadContentPage(url, { extension });
    await extension.awaitMessage("page-ready");

    async function checkRequest(arg, expectPrompt, msg) {
      optionalPermissionsPromptHandler.sawPrompt = false;
      extension.sendMessage("request", arg);
      let result = await extension.awaitMessage("request.result");
      ok(result, "request() call succeeded");
      equal(
        optionalPermissionsPromptHandler.sawPrompt,
        expectPrompt,
        `Got ${expectPrompt ? "" : "no "}permission prompt for ${msg}`
      );
    }

    await checkRequest(
      { permissions: ["geolocation"] },
      false,
      "required permission from manifest"
    );
    await checkRequest(
      { origins: ["http://required-host.com/"] },
      false,
      "origin permission from manifest"
    );
    await checkRequest(
      { origins: ["http://host.required-domain.com/"] },
      false,
      "wildcard origin permission from manifest"
    );

    await checkRequest(
      { permissions: ["clipboardRead"] },
      true,
      "optional permission"
    );
    await checkRequest(
      { permissions: ["clipboardRead"] },
      false,
      "already granted optional permission"
    );

    await checkRequest(
      { origins: ["http://optional-host.com/"] },
      true,
      "optional origin"
    );
    await checkRequest(
      { origins: ["http://optional-host.com/"] },
      false,
      "already granted origin permission"
    );

    await checkRequest(
      { origins: ["http://*.optional-domain.com/"] },
      true,
      "optional wildcard origin"
    );
    await checkRequest(
      { origins: ["http://*.optional-domain.com/"] },
      false,
      "already granted optional wildcard origin"
    );
    await checkRequest(
      { origins: ["http://host.optional-domain.com/"] },
      false,
      "host matching optional wildcard origin"
    );
    await page.close();
  });

  await extension.unload();
}
add_task(async function test_alreadyGranted_mv2() {
  return test_alreadyGranted(2);
});
add_task(async function test_alreadyGranted_mv3() {
  return test_alreadyGranted(3);
});

// IMPORTANT: Do not change this list without review from a Web Extensions peer!

const GRANTED_WITHOUT_USER_PROMPT = [
  "activeTab",
  "activityLog",
  "alarms",
  "captivePortal",
  "contextMenus",
  "contextualIdentities",
  "cookies",
  "declarativeNetRequestFeedback",
  "declarativeNetRequestWithHostAccess",
  "dns",
  "geckoProfiler",
  "identity",
  "idle",
  "menus",
  "menus.overrideContext",
  "mozillaAddons",
  "networkStatus",
  "normandyAddonStudy",
  "scripting",
  "search",
  "storage",
  "telemetry",
  "theme",
  "unlimitedStorage",
  "urlbar",
  "webRequest",
  "webRequestBlocking",
  "webRequestFilterResponse.serviceWorkerScript",
];

add_task(function test_permissions_have_localization_strings() {
  let noPromptNames = Schemas.getPermissionNames([
    "PermissionNoPrompt",
    "OptionalPermissionNoPrompt",
    "PermissionPrivileged",
  ]);
  Assert.deepEqual(
    GRANTED_WITHOUT_USER_PROMPT,
    noPromptNames,
    "List of no-prompt permissions is correct."
  );

  const bundle = Services.strings.createBundle(BROWSER_PROPERTIES);

  for (const perm of Schemas.getPermissionNames()) {
    try {
      const str = bundle.GetStringFromName(`webextPerms.description.${perm}`);

      ok(str.length, `Found localization string for '${perm}' permission`);
    } catch (e) {
      ok(
        GRANTED_WITHOUT_USER_PROMPT.includes(perm),
        `Permission '${perm}' intentionally granted without prompting the user`
      );
    }
  }
});

// Check <all_urls> used as an optional API permission.
add_task(async function test_optional_all_urls() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      optional_permissions: ["<all_urls>"],
    },

    background() {
      browser.test.onMessage.addListener(async () => {
        let before = !!browser.tabs.captureVisibleTab;
        let granted = await browser.permissions.request({
          origins: ["<all_urls>"],
        });
        let after = !!browser.tabs.captureVisibleTab;

        browser.test.sendMessage("results", [before, granted, after]);
      });
    },
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
    let [before, granted, after] = await extension.awaitMessage("results");

    equal(
      before,
      false,
      "captureVisibleTab() unavailable before optional permission request()"
    );
    equal(granted, true, "request() for optional permissions granted");
    equal(
      after,
      true,
      "captureVisibleTab() available after optional permission request()"
    );
  });

  await extension.unload();
});

// Check when content_script match patterns are treated as optional origins.
async function test_content_script_is_optional(manifest_version) {
  function background() {
    browser.test.onMessage.addListener(async (msg, arg) => {
      if (msg == "request") {
        try {
          let result = await browser.permissions.request(arg);
          browser.test.sendMessage("result", result);
        } catch (e) {
          browser.test.sendMessage("result", e.message);
        }
      }
      if (msg === "getAll") {
        let result = await browser.permissions.getAll(arg);
        browser.test.sendMessage("granted", result);
      }
    });
  }

  const CS_ORIGIN = "https://test2.example.com/*";

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      manifest_version,
      content_scripts: [
        {
          matches: [CS_ORIGIN],
          js: [],
        },
      ],
    },
  });

  await extension.startup();

  extension.sendMessage("getAll");
  let initial = await extension.awaitMessage("granted");
  deepEqual(initial.origins, [], "Nothing granted on install.");

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request", {
      permissions: [],
      origins: [CS_ORIGIN],
    });
    let result = await extension.awaitMessage("result");
    if (manifest_version < 3) {
      equal(
        result,
        `Cannot request origin permission for ${CS_ORIGIN} since it was not declared in the manifest`,
        "Content script match pattern is not a requestable optional origin in MV2"
      );
    } else {
      equal(result, true, "request() for optional permissions succeeded");
    }
  });

  extension.sendMessage("getAll");
  let granted = await extension.awaitMessage("granted");
  deepEqual(
    granted.origins,
    manifest_version < 3 ? [] : [CS_ORIGIN],
    "Granted content script origin in MV3."
  );

  await extension.unload();
}
add_task(() => test_content_script_is_optional(2));
add_task(() => test_content_script_is_optional(3));

// Check that optional permissions are not included in update prompts
async function test_permissions_prompt(manifest_version) {
  function background() {
    browser.test.onMessage.addListener(async (msg, arg) => {
      if (msg == "request") {
        let result = await browser.permissions.request(arg);
        browser.test.sendMessage("result", result);
      }
      if (msg === "getAll") {
        let result = await browser.permissions.getAll(arg);
        browser.test.sendMessage("granted", result);
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      name: "permissions test",
      description: "permissions test",
      manifest_version,
      version: "1.0",

      permissions: ["tabs"],
      host_permissions: ["https://test1.example.com/*"],
      optional_permissions: ["clipboardWrite", "<all_urls>"],

      content_scripts: [
        {
          matches: ["https://test2.example.com/*"],
          js: [],
        },
      ],
    },
    useAddonManager: "permanent",
  });

  await extension.startup();

  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request", {
      permissions: ["clipboardWrite"],
      origins: ["https://test2.example.com/*"],
    });
    let result = await extension.awaitMessage("result");
    equal(result, true, "request() for optional permissions succeeded");
  });

  if (manifest_version >= 3) {
    await withHandlingUserInput(extension, async () => {
      extension.sendMessage("request", {
        origins: ["https://test1.example.com/*"],
      });
      let result = await extension.awaitMessage("result");
      equal(result, true, "request() for host_permissions in mv3 succeeded");
    });
  }

  const PERMS = ["history", "tabs"];
  const ORIGINS = ["https://test1.example.com/*", "https://test3.example.com/"];
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    background,
    manifest: {
      name: "permissions test",
      description: "permissions test",
      manifest_version,
      version: "2.0",

      applications: { gecko: { id: extension.id } },

      permissions: PERMS,
      host_permissions: ORIGINS,
      optional_permissions: ["clipboardWrite", "<all_urls>"],
    },
  });

  let install = await AddonManager.getInstallForFile(xpi);

  let perminfo;
  install.promptHandler = info => {
    perminfo = info;
    return Promise.resolve();
  };

  await AddonTestUtils.promiseCompleteInstall(install);
  await extension.awaitStartup();

  notEqual(perminfo, undefined, "Permission handler was invoked");
  let perms = perminfo.addon.userPermissions;
  deepEqual(
    perms.permissions,
    PERMS,
    "Update details includes only manifest api permissions"
  );
  deepEqual(
    perms.origins,
    manifest_version < 3 ? ORIGINS : [],
    "Update details includes only manifest origin permissions"
  );

  let EXPECTED = ["https://test1.example.com/*", "https://test2.example.com/*"];
  if (manifest_version < 3) {
    EXPECTED.push("https://test3.example.com/*");
  }

  extension.sendMessage("getAll");
  let granted = await extension.awaitMessage("granted");
  deepEqual(
    granted.origins.sort(),
    EXPECTED,
    "Granted origins persisted after update."
  );

  await extension.unload();
}
add_task(async function test_permissions_prompt_mv2() {
  return test_permissions_prompt(2);
});
add_task(async function test_permissions_prompt_mv3() {
  return test_permissions_prompt(3);
});

// Check that internal permissions can not be set and are not returned by the API.
add_task(async function test_internal_permissions() {
  function background() {
    browser.test.onMessage.addListener(async (method, arg) => {
      try {
        if (method == "getAll") {
          let perms = await browser.permissions.getAll();
          browser.test.sendMessage("getAll.result", perms);
        } else if (method == "contains") {
          let result = await browser.permissions.contains(arg);
          browser.test.sendMessage("contains.result", {
            status: "success",
            result,
          });
        } else if (method == "request") {
          let result = await browser.permissions.request(arg);
          browser.test.sendMessage("request.result", {
            status: "success",
            result,
          });
        } else if (method == "remove") {
          let result = await browser.permissions.remove(arg);
          browser.test.sendMessage("remove.result", result);
        }
      } catch (err) {
        browser.test.sendMessage(`${method}.result`, {
          status: "error",
          message: err.message,
        });
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      name: "permissions test",
      description: "permissions test",
      manifest_version: 2,
      version: "1.0",
      permissions: [],
    },
    useAddonManager: "permanent",
    incognitoOverride: "spanning",
  });

  let perm = "internal:privateBrowsingAllowed";

  await extension.startup();

  function call(method, arg) {
    extension.sendMessage(method, arg);
    return extension.awaitMessage(`${method}.result`);
  }

  let result = await call("getAll");
  ok(!result.permissions.includes(perm), "internal not returned");

  result = await call("contains", { permissions: [perm] });
  ok(
    /Type error for parameter permissions \(Error processing permissions/.test(
      result.message
    ),
    `Unable to check for internal permission: ${result.message}`
  );

  result = await call("remove", { permissions: [perm] });
  ok(
    /Type error for parameter permissions \(Error processing permissions/.test(
      result.message
    ),
    `Unable to remove for internal permission ${result.message}`
  );

  await withHandlingUserInput(extension, async () => {
    result = await call("request", {
      permissions: [perm],
      origins: [],
    });
    ok(
      /Type error for parameter permissions \(Error processing permissions/.test(
        result.message
      ),
      `Unable to request internal permission ${result.message}`
    );
  });

  await extension.unload();
});

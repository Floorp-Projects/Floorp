/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = false;
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
// This pref is not set in Thunderbird, and needs to be true for the test to pass.
Services.prefs.setBoolPref("extensions.postDownloadThirdPartyPrompt", true);

let server = AddonTestUtils.createHttpServer({
  hosts: ["example.com", "example.org", "amo.example.com", "github.io"],
});

server.registerFile(
  `/addons/origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "origins@example.com",
        },
      },
      install_origins: ["http://example.com"],
    },
  })
);

server.registerFile(
  `/addons/sitepermission.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins sitepermission test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "sitepermission@example.com",
        },
      },
      install_origins: ["http://example.com"],
      site_permissions: ["midi"],
    },
  })
);

server.registerFile(
  `/addons/sitepermission-suffix.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins sitepermission public suffix test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "sitepermission-suffix@github.io",
        },
      },
      install_origins: ["http://github.io"],
      site_permissions: ["midi"],
    },
  })
);

server.registerFile(
  `/addons/two_origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "two_origins@example.com",
        },
      },
      install_origins: ["http://example.com", "http://example.org"],
    },
  })
);

server.registerFile(
  `/addons/no_origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "no_origins@example.com",
        },
      },
    },
  })
);

server.registerFile(
  `/addons/empty_origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 2,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "no_origins@example.com",
        },
      },
      install_origins: [],
    },
  })
);

server.registerFile(
  `/addons/v3_origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 3,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "v3_origins@example.com",
        },
      },
      install_origins: ["http://example.com"],
    },
  })
);

server.registerFile(
  `/addons/v3_no_origins.xpi`,
  AddonTestUtils.createTempXPIFile({
    "manifest.json": {
      manifest_version: 3,
      name: "Install Origins test",
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: "v3_no_origins@example.com",
        },
      },
    },
  })
);

add_setup(() => {
  do_get_profile();
  Services.fog.initializeFOG();
});

function testInstallEvent(expectTelemetry) {
  const snapshot = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    true
  );

  ok(
    snapshot.parent && !!snapshot.parent.length,
    "Got parent telemetry events in the snapshot"
  );

  let events = snapshot.parent
    .filter(
      ([timestamp, category, method, object, value, extra]) =>
        category === "addonsManager" &&
        method == "install" &&
        extra.step == expectTelemetry.step
    )
    .map(event => event[5]);
  equal(events.length, 1, "one event for install completion");
  Assert.deepEqual(events[0], expectTelemetry, "telemetry matches");

  let gleanEvents = AddonTestUtils.getAMGleanEvents("install", {
    step: expectTelemetry.step,
  });
  Services.fog.testResetFOG();

  equal(gleanEvents.length, 1, "One glean event for install completion.");
  delete gleanEvents[0].addon_type;
  Assert.deepEqual(gleanEvents[0], expectTelemetry, "Glean telemetry matches.");
}

function promiseCompleteWebInstall(
  install,
  triggeringPrincipal,
  expectPrompts = true
) {
  let listener;
  return new Promise(_resolve => {
    let resolve = () => {
      install.removeListener(listener);
      _resolve();
    };

    listener = {
      onDownloadFailed: resolve,
      onDownloadCancelled: resolve,
      onInstallFailed: resolve,
      onInstallCancelled: resolve,
      onInstallEnded: resolve,
      onInstallPostponed: resolve,
    };

    install.addListener(listener);

    // Observers to bypass panels and continue install.
    if (expectPrompts) {
      TestUtils.topicObserved("addon-install-blocked").then(([subject]) => {
        let installInfo = subject.wrappedJSObject;
        info(`==== test got addon-install-blocked ${subject} ${installInfo}`);
        installInfo.install();
      });

      TestUtils.topicObserved("addon-install-confirmation").then(
        (subject, data) => {
          info(`==== test got addon-install-confirmation`);
          let installInfo = subject.wrappedJSObject;
          for (let installer of installInfo.installs) {
            installer.install();
          }
        }
      );
      TestUtils.topicObserved("webextension-permission-prompt").then(
        ([subject]) => {
          const { info } = subject.wrappedJSObject || {};
          info.resolve();
        }
      );
    }

    AddonManager.installAddonFromWebpage(
      "application/x-xpinstall",
      null /* aBrowser */,
      triggeringPrincipal,
      install
    );
  });
}

async function testAddonInstall(test) {
  let { name, xpiUrl, installPrincipal, expectState, expectTelemetry } = test;
  info(`testAddonInstall: ${name}`);
  let expectInstall = expectState == AddonManager.STATE_INSTALLED;
  let install = await AddonManager.getInstallForURL(xpiUrl, {
    triggeringPrincipal: installPrincipal,
  });
  await promiseCompleteWebInstall(install, installPrincipal, expectInstall);

  // Test origins telemetry
  testInstallEvent(expectTelemetry);

  if (expectInstall) {
    equal(
      install.state,
      expectState,
      `${name} ${install.addon.id} install was completed`
    );
    // Wait the extension startup to ensure manifest.json has been read,
    // otherwise we get NS_ERROR_FILE_NOT_FOUND log spam.
    await WebExtensionPolicy.getByID(install.addon.id)?.readyPromise;
    await install.addon.uninstall();
  } else {
    equal(
      install.state,
      expectState,
      `${name} ${install.addon?.id} install failed`
    );
  }
}

let ssm = Services.scriptSecurityManager;
const PRINCIPAL_AMO = ssm.createContentPrincipalFromOrigin(
  "https://amo.example.com"
);
const PRINCIPAL_COM =
  ssm.createContentPrincipalFromOrigin("http://example.com");
const SUB_PRINCIPAL_COM = ssm.createContentPrincipalFromOrigin(
  "http://abc.example.com"
);
const THIRDPARTY_PRINCIPAL_COM = ssm.createContentPrincipalFromOrigin(
  "http://fake-example.com"
);
const PRINCIPAL_ORG =
  ssm.createContentPrincipalFromOrigin("http://example.org");
const PRINCIPAL_ETLD = ssm.createContentPrincipalFromOrigin("http://github.io");

const TESTS = [
  {
    name: "Install MV2 with install_origins",
    xpiUrl: "http://example.com/addons/origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install MV2 without install_origins",
    xpiUrl: "http://example.com/addons/no_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "no_origins@example.com",
      install_origins: "0",
    },
  },
  {
    name: "Install valid xpi location from invalid website",
    xpiUrl: "http://example.com/addons/origins.xpi",
    installPrincipal: PRINCIPAL_ORG,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "origins@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install invalid xpi location from valid website",
    xpiUrl: "http://example.org/addons/origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "origins@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install MV3 with install_origins",
    xpiUrl: "http://example.com/addons/v3_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "v3_origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install MV3 with install_origins from AMO",
    xpiUrl: "http://example.com/addons/v3_origins.xpi",
    installPrincipal: PRINCIPAL_AMO,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "v3_origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install MV3 without install_origins",
    xpiUrl: "http://example.com/addons/v3_no_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "v3_no_origins@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "0",
    },
  },
  {
    // An installing principal with install permission is
    // considered "AMO" in code, and will always be allowed.
    name: "Install MV3 without install_origins from AMO",
    xpiUrl: "http://example.com/addons/v3_no_origins.xpi",
    installPrincipal: PRINCIPAL_AMO,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "v3_no_origins@example.com",
      install_origins: "0",
    },
  },
  {
    name: "Install MV3 without install_origins from null principal",
    xpiUrl: "http://example.com/addons/v3_no_origins.xpi",
    installPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
    expectState: AddonManager.STATE_CANCELLED,
    expectTelemetry: { step: "site_blocked", install_origins: "0" },
  },
  {
    name: "Install addon with two install_origins",
    xpiUrl: "http://example.com/addons/two_origins.xpi",
    installPrincipal: PRINCIPAL_ORG,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "two_origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install addon with two install_origins",
    xpiUrl: "http://example.com/addons/two_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "two_origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install from site with empty install_origins",
    xpiUrl: "http://example.com/addons/empty_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "no_origins@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install from site with empty install_origins",
    xpiUrl: "http://example.com/addons/empty_origins.xpi",
    installPrincipal: PRINCIPAL_ORG,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "no_origins@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install with empty install_origins from AMO",
    xpiUrl: "http://amo.example.com/addons/empty_origins.xpi",
    installPrincipal: PRINCIPAL_AMO,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "no_origins@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install sitepermission from domain",
    xpiUrl: "http://example.com/addons/sitepermission.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "sitepermission@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install sitepermission from subdomain",
    xpiUrl: "http://example.com/addons/sitepermission.xpi",
    installPrincipal: SUB_PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "sitepermission@example.com",
      install_origins: "1",
    },
  },
  {
    name: "Install sitepermission from thirdparty domain should fail",
    xpiUrl: "http://example.com/addons/sitepermission.xpi",
    installPrincipal: THIRDPARTY_PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "sitepermission@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install sitepermission from different domain",
    xpiUrl: "http://example.com/addons/sitepermission.xpi",
    installPrincipal: PRINCIPAL_ORG,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "sitepermission@example.com",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
  {
    name: "Install sitepermission from public suffix domain",
    xpiUrl: "http://github.io/addons/sitepermission-suffix.xpi",
    installPrincipal: PRINCIPAL_ETLD,
    expectState: AddonManager.STATE_INSTALL_FAILED,
    expectTelemetry: {
      step: "failed",
      addon_id: "sitepermission-suffix@github.io",
      error: "ERROR_INVALID_DOMAIN",
      install_origins: "1",
    },
  },
];

add_task(async function test_install_url() {
  Services.prefs.setBoolPref("extensions.install_origins.enabled", true);
  PermissionTestUtils.add(
    PRINCIPAL_AMO,
    "install",
    Services.perms.ALLOW_ACTION
  );
  await promiseStartupManager();

  for (let test of TESTS) {
    await testAddonInstall(test);
  }
});

add_task(async function test_install_origins_disabled() {
  Services.prefs.setBoolPref("extensions.install_origins.enabled", false);
  await testAddonInstall({
    name: "Install MV3 without install_origins, verification disabled",
    xpiUrl: "http://example.com/addons/v3_no_origins.xpi",
    installPrincipal: PRINCIPAL_COM,
    expectState: AddonManager.STATE_INSTALLED,
    expectTelemetry: {
      step: "completed",
      addon_id: "v3_no_origins@example.com",
    },
  });
});

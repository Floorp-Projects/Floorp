/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);

const XPI_INCOMPATIBLE_ID = "incompatible-xpi@tests.mozilla.org";
// NOTE: we are using an HTTP url on purpose here, the test case fails
// otherwise... We disable `AddonManager.checkUpdateSecurity` to allow
// retrieving updates from HTTP (which is restored in a
// `registerCleanupFunction()` or at the end of the task).
//
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const BASE_URL = "http://fake-updates.example.com";

const server = AddonTestUtils.createHttpServer({
  hosts: ["fake-updates.example.com"],
});

const UPDATE_ENTRY_COMPATIBLE = {
  // NOTE: this version must be the exact same one associated than the
  // initially incompatible XPI, otherwise it won't override the initial
  // compatibility range.
  // See the check in `AddonUpdateChecker.getCompatibilityUpdate` here:
  // https://searchfox.org/mozilla-central/rev/4044c340/toolkit/mozapps/extensions/internal/AddonUpdateChecker.sys.mjs#489
  version: "4.0",
  // An empty compatibility range will make this update to be overriding the
  // incompatible range in the xpi and makes the xpi version to be considered
  // compatible.
  applications: { gecko: {} },
};

const UPDATE_ENTRY_INCOMPATIBLE = {
  ...UPDATE_ENTRY_COMPATIBLE,
  // This update entry instead is including a compatibility range that would
  // makes the xpi version being installed to be considered still incompatible.
  applications: {
    gecko: {
      strict_min_version: "41",
      strict_max_version: "41.*",
    },
  },
};

AddonTestUtils.registerJSON(server, "/updates-still-incompatible.json", {
  addons: {
    [XPI_INCOMPATIBLE_ID]: {
      updates: [UPDATE_ENTRY_INCOMPATIBLE],
    },
  },
});

AddonTestUtils.registerJSON(server, "/updates-now-compatible.json", {
  addons: {
    [XPI_INCOMPATIBLE_ID]: {
      updates: [UPDATE_ENTRY_COMPATIBLE],
    },
  },
});

add_task(async function test_local_install_blocklisted() {
  let id = "amosigned-xpi@tests.mozilla.org";
  let version = "2.1";

  await AddonTestUtils.loadBlocklistRawData({
    extensionsMLBF: [
      {
        stash: { blocked: [`${id}:${version}`], unblocked: [] },
        stash_time: 0,
      },
    ],
  });
  let needsCleanupBlocklist = true;
  const cleanupBlocklist = async () => {
    if (!needsCleanupBlocklist) {
      return;
    }
    await AddonTestUtils.loadBlocklistRawData({
      extensionsMLBF: [
        {
          stash: { blocked: [], unblocked: [] },
          stash_time: 0,
        },
      ],
    });
    needsCleanupBlocklist = false;
  };
  registerCleanupFunction(cleanupBlocklist);

  const xpiFilePath = getTestFilePath("../xpinstall/amosigned.xpi");
  const xpiFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  xpiFile.initWithPath(xpiFilePath);
  ok(xpiFile.exists(), "Expect the xpi file to exist");
  const xpiFileURI = Services.io.newFileURI(xpiFile);

  let install = await AddonManager.getInstallForURL(xpiFileURI.spec, {
    telemetryInfo: { source: "file-url" },
  });
  const promiseInstallFailed = BrowserUtils.promiseObserved(
    "addon-install-failed",
    subject => {
      return subject.wrappedJSObject.installs[0] == install;
    }
  );

  AddonManager.installAddonFromWebpage(
    "application/x-xpinstall",
    gBrowser.selectedBrowser,
    Services.scriptSecurityManager.getSystemPrincipal(),
    install
  );

  info("Wait for addon-install-failed to be notified");
  await promiseInstallFailed;
  Assert.equal(
    install.error,
    AddonManager.ERROR_BLOCKLISTED,
    "LocalInstall cancelled with the expected error"
  );

  await cleanupBlocklist();
});

add_task(async function test_local_install_incompatible() {
  const xpiFilePath = getTestFilePath("../xpinstall/incompatible.xpi");
  const xpiFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  xpiFile.initWithPath(xpiFilePath);
  ok(xpiFile.exists(), "Expect the xpi file to exist");
  const xpiFileURI = Services.io.newFileURI(xpiFile);

  const installTestExtension = async ({ expectIncompatible }) => {
    let install = await AddonManager.getInstallForURL(xpiFileURI.spec, {
      telemetryInfo: { source: "file-url" },
    });
    const promiseInstallDone = expectIncompatible
      ? BrowserUtils.promiseObserved(
          "addon-install-failed",
          subject => subject.wrappedJSObject.installs[0] == install
        )
      : BrowserUtils.promiseObserved(
          "webextension-permission-prompt",
          subject => subject.wrappedJSObject.info.addon == install.addon
        );

    AddonManager.installAddonFromWebpage(
      "application/x-xpinstall",
      gBrowser.selectedBrowser,
      Services.scriptSecurityManager.getSystemPrincipal(),
      install
    );

    if (expectIncompatible) {
      info("Wait for addon-install-failed to be notified");
      await promiseInstallDone;
      Assert.equal(
        install.error,
        AddonManager.ERROR_INCOMPATIBLE,
        "LocalInstall cancelled with the expected error"
      );
    } else {
      info("Wait for webextension-permission-prompt to be notified");
      await promiseInstallDone;
      Assert.equal(
        install.error,
        0,
        "no error expected on the LocalInstall instance"
      );
      Assert.equal(
        install.state,
        AddonManager.STATE_DOWNLOADED,
        "Got the expected LocalInstall state"
      );
      Assert.ok(
        install.addon.isCompatible,
        "updated Addon XPI is expected to be compatible"
      );
      Assert.equal(
        install.addon.version,
        "4.0",
        "Addon version expected to match the updated xpi file"
      );
      // Cancel the installation, before exiting the test.
      await install.cancel();
    }
  };

  info("Test incompatible xpi without a compatibility override");
  // Use a new tab to make sure the doorhanger will be gone when
  // the test tab is being removed (same when repeating the
  // test with expectIncompatible set to false).
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await installTestExtension({ expectIncompatible: true });
  });

  // Add the prefs to ignore signature checks for this test (allowed on all
  // channels while running in automation).
  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.update.url", `${BASE_URL}/updates.json`],
      ["xpinstall.signatures.required", false],
      ["extensions.ui.ignoreUnsigned", true],
    ],
  });
  AddonManager.checkUpdateSecurity = false;
  registerCleanupFunction(() => {
    AddonManager.checkUpdateSecurity = true;
  });

  info(
    "Test incompatible xpi with a compatibility override that is still incompatible"
  );
  // Add the prefs to provide a compatibility range override which is still
  // incompatible.
  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.update.url", `${BASE_URL}/updates-still-incompatible.json`],
    ],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await installTestExtension({ expectIncompatible: true });
  });
  SpecialPowers.popPrefEnv();

  info(
    "Test incompatible xpi with a compatibility override that makes it compatible"
  );
  // Add the prefs to provide a compatibility range override which is
  // compatible.
  SpecialPowers.pushPrefEnv({
    set: [["extensions.update.url", `${BASE_URL}/updates-now-compatible.json`]],
  });
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await installTestExtension({ expectIncompatible: false });
  });
  SpecialPowers.popPrefEnv();

  SpecialPowers.popPrefEnv();
  AddonManager.checkUpdateSecurity = true;
});

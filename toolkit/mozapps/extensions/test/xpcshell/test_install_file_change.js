/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

/* globals browser */

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

async function createXPIWithID(addonId, version = "1.0") {
  let xpiFile = await createTempWebExtensionFile({
    manifest: {
      version,
      browser_specific_settings: { gecko: { id: addonId } },
    },
  });
  return xpiFile;
}

const ERROR_PATTERN_INSTALL_FAIL = /Failed to install .+ from .+ to /;
const ERROR_PATTERN_POSTPONE_FAIL = /Failed to postpone install of /;

async function promiseInstallFail(install, expectedErrorPattern) {
  let { messages } = await promiseConsoleOutput(async () => {
    await Assert.rejects(
      install.install(),
      /^Error: Install failed: onInstallFailed$/
    );
  });
  messages = messages.filter(msg => expectedErrorPattern.test(msg.message));
  equal(messages.length, 1, "Expected log messages");
  equal(install.state, AddonManager.STATE_INSTALL_FAILED);
  equal(install.error, AddonManager.ERROR_FILE_ACCESS);
  equal((await AddonManager.getAllInstalls()).length, 0, "no pending installs");
}

add_task(async function test_file_deleted() {
  let xpiFile = await createXPIWithID("delete@me");
  let install = await AddonManager.getInstallForFile(xpiFile);
  equal(install.state, AddonManager.STATE_DOWNLOADED);

  xpiFile.remove(false);

  await promiseInstallFail(install, ERROR_PATTERN_INSTALL_FAIL);

  equal(await AddonManager.getAddonByID("delete@me"), null);
});

add_task(async function test_file_emptied() {
  let xpiFile = await createXPIWithID("empty@me");
  let install = await AddonManager.getInstallForFile(xpiFile);
  equal(install.state, AddonManager.STATE_DOWNLOADED);

  await IOUtils.write(xpiFile.path, new Uint8Array());

  await promiseInstallFail(install, ERROR_PATTERN_INSTALL_FAIL);

  equal(await AddonManager.getAddonByID("empty@me"), null);
});

add_task(async function test_file_replaced() {
  let xpiFile = await createXPIWithID("replace@me");
  let install = await AddonManager.getInstallForFile(xpiFile);
  equal(install.state, AddonManager.STATE_DOWNLOADED);

  await IOUtils.copy(
    (await createXPIWithID("replace@me", "2")).path,
    xpiFile.path
  );

  await promiseInstallFail(install, ERROR_PATTERN_INSTALL_FAIL);

  equal(await AddonManager.getAddonByID("replace@me"), null);
});

async function do_test_update_with_file_replaced(wantPostponeTest) {
  const ADDON_ID = wantPostponeTest ? "postpone@me" : "update@me";
  function backgroundWithPostpone() {
    // The registration of this listener postpones the update.
    browser.runtime.onUpdateAvailable.addListener(() => {
      browser.test.fail("Unusable update should not call onUpdateAvailable");
    });
  }
  await promiseInstallWebExtension({
    manifest: {
      version: "1.0",
      browser_specific_settings: {
        gecko: {
          id: ADDON_ID,
          update_url: `http://example.com/update-${ADDON_ID}.json`,
        },
      },
    },
    background: wantPostponeTest ? backgroundWithPostpone : () => {},
  });

  server.registerFile(
    `/update-${ADDON_ID}.xpi`,
    await createTempWebExtensionFile({
      manifest: {
        version: "2.0",
        browser_specific_settings: { gecko: { id: ADDON_ID } },
      },
    })
  );
  AddonTestUtils.registerJSON(server, `/update-${ADDON_ID}.json`, {
    addons: {
      [ADDON_ID]: {
        updates: [
          {
            version: "2.0",
            update_link: `http://example.com/update-${ADDON_ID}.xpi`,
          },
        ],
      },
    },
  });

  // Setup completed, let's try to verify that file corruption halts the update.

  let addon = await promiseAddonByID(ADDON_ID);
  equal(addon.version, "1.0");

  let update = await promiseFindAddonUpdates(
    addon,
    AddonManager.UPDATE_WHEN_USER_REQUESTED
  );
  let install = update.updateAvailable;
  equal(install.version, "2.0");
  equal(install.state, AddonManager.STATE_AVAILABLE);
  equal(install.existingAddon, addon);
  equal(install.file, null);

  let promptCount = 0;
  let didReplaceFile = false;
  install.promptHandler = async function () {
    ++promptCount;
    equal(install.state, AddonManager.STATE_DOWNLOADED);
    await IOUtils.copy(
      (await createXPIWithID(ADDON_ID, "3")).path,
      install.file.path
    );
    didReplaceFile = true;
    equal(install.state, AddonManager.STATE_DOWNLOADED, "State not changed");
  };

  if (wantPostponeTest) {
    await promiseInstallFail(install, ERROR_PATTERN_POSTPONE_FAIL);
  } else {
    await promiseInstallFail(install, ERROR_PATTERN_INSTALL_FAIL);
  }

  equal(promptCount, 1);
  ok(didReplaceFile, "Replaced update with different file");

  // Now verify that the add-on is still at the old version.
  addon = await promiseAddonByID(ADDON_ID);
  equal(addon.version, "1.0");

  await addon.uninstall();
}

add_task(async function test_update_and_file_replaced() {
  await do_test_update_with_file_replaced();
});

add_task(async function test_update_postponed_and_file_replaced() {
  await do_test_update_with_file_replaced(/* wantPostponeTest = */ true);
});

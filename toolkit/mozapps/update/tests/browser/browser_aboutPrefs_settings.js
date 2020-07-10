/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);

// Changes, then verifies the value of app.update.auto via the about:preferences
// UI. Requires a tab with about:preferences open to be passed in.
async function changeAndVerifyPref(tab, newConfigValue) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ newConfigValue }],
    async function({ newConfigValue }) {
      let radioId = newConfigValue ? "autoDesktop" : "manualDesktop";
      let radioElement = content.document.getElementById(radioId);
      radioElement.click();
    }
  );

  // On Windows, we really need to wait for the change to finish being
  // written to the disk before we go to verify anything. Unfortunately, it
  // would be difficult to check for quick changes to the attributes of the
  // about:preferences controls (to wait for the controls to be disabled and
  // re-enabled). So instead, just start the verification by asking the
  // Application Update Service for the value of app.update.auto. It already
  // serializes reads and writes to the app update config file, so this will not
  // resolve until the file write is complete.
  let configValueRead = await UpdateUtils.getAppUpdateAutoEnabled();
  is(
    configValueRead,
    newConfigValue,
    "Value returned should have matched the expected value"
  );

  // Only Windows currently has the update configuration JSON file.
  if (AppConstants.platform == "win") {
    let configFile = getUpdateDirFile(FILE_UPDATE_CONFIG_JSON);
    let decoder = new TextDecoder();
    let fileContents = await OS.File.read(configFile.path);
    let saveObject = JSON.parse(decoder.decode(fileContents));
    is(
      saveObject["app.update.auto"],
      newConfigValue,
      "Value in file should match expected"
    );
  }

  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ newConfigValue }],
    async function({ newConfigValue }) {
      let updateRadioGroup = content.document.getElementById(
        "updateRadioGroup"
      );
      is(
        updateRadioGroup.value,
        `${newConfigValue}`,
        "Update preference should match expected"
      );
    }
  );
}

add_task(async function testUpdateAutoPrefUI() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  await changeAndVerifyPref(tab, true);
  ok(!gUpdateManager.activeUpdate, "There should not be an active update");

  await changeAndVerifyPref(tab, false);
  ok(!gUpdateManager.activeUpdate, "There should not be an active update");

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { checkInterval: "1" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_PENDING);
  reloadUpdateManagerData();
  ok(!!gUpdateManager.activeUpdate, "There should be an active update");

  // A value of 0 will keep the update and a value of 1 will discard the update
  // when the prompt service is called when the value of app.update.auto is
  // changed to false.
  let discardUpdate = 0;
  let { prompt } = Services;
  let promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx(...args) {
      promptService._confirmExArgs = args;
      return discardUpdate;
    },
  };
  Services.prompt = promptService;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  // Setting the value to false will call the prompt service and with 1 for
  // discardUpdate the update won't be discarded so there should still be an
  // active update.
  discardUpdate = 1;
  await changeAndVerifyPref(tab, false);
  ok(!!gUpdateManager.activeUpdate, "There should be an active update");

  // Setting the value to true should not call the prompt service so there
  // should still be an active update even with a value of 0 for
  // discardUpdate.
  discardUpdate = 0;
  await changeAndVerifyPref(tab, true);
  ok(!!gUpdateManager.activeUpdate, "There should be an active update");

  // Setting the value to false will call the prompt service and with 0 for
  // discardUpdate the update should be discarded so there should not be an
  // active update.
  discardUpdate = 0;
  await changeAndVerifyPref(tab, false);
  ok(!gUpdateManager.activeUpdate, "There should not be an active update");

  await BrowserTestUtils.removeTab(tab);
});

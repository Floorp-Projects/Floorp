/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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

  let configValueRead = await UpdateUtils.getAppUpdateAutoEnabled();
  is(
    configValueRead,
    newConfigValue,
    "Value returned should have matched the expected value"
  );

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
  ok(
    !gUpdateManager.downloadingUpdate,
    "There should not be a downloading update"
  );
  ok(!gUpdateManager.readyUpdate, "There should not be a ready update");

  await changeAndVerifyPref(tab, false);
  ok(
    !gUpdateManager.downloadingUpdate,
    "There should not be a downloading update"
  );
  ok(!gUpdateManager.readyUpdate, "There should not be a ready update");

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { checkInterval: "1" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_PENDING);
  reloadUpdateManagerData();
  ok(!!gUpdateManager.readyUpdate, "There should be a ready update");

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
  ok(!!gUpdateManager.readyUpdate, "There should be a ready update");

  // Setting the value to true should not call the prompt service so there
  // should still be an active update even with a value of 0 for
  // discardUpdate.
  discardUpdate = 0;
  await changeAndVerifyPref(tab, true);
  ok(!!gUpdateManager.readyUpdate, "There should be a ready update");

  // Setting the value to false will call the prompt service and with 0 for
  // discardUpdate the update should be discarded so there should not be an
  // active update.
  discardUpdate = 0;
  await changeAndVerifyPref(tab, false);
  ok(
    !gUpdateManager.downloadingUpdate,
    "There should not be a downloading update"
  );
  ok(!gUpdateManager.readyUpdate, "There should not be a ready update");

  await BrowserTestUtils.removeTab(tab);
});

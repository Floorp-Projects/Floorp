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
    async function ({ newConfigValue }) {
      let radioId = newConfigValue ? "autoDesktop" : "manualDesktop";
      let radioElement = content.document.getElementById(radioId);
      let updateRadioGroup = radioElement.radioGroup;
      let promise = ContentTaskUtils.waitForEvent(
        updateRadioGroup,
        "ProcessedUpdatePrefChange"
      );
      radioElement.click();
      await promise;

      is(
        updateRadioGroup.value,
        `${newConfigValue}`,
        "Update preference should match expected"
      );
      is(
        updateRadioGroup.disabled,
        false,
        "Update preferences should no longer be disabled"
      );
    }
  );

  let configValueRead = await UpdateUtils.getAppUpdateAutoEnabled();
  is(
    configValueRead,
    newConfigValue,
    "Value returned should have matched the expected value"
  );
}

async function changeAndVerifyUpdateWrites({
  tab,
  newConfigValue,
  discardUpdate,
  expectPrompt,
  expectRemainingUpdate,
}) {
  // A value of 1 will keep the update and a value of 0 will discard the update
  // when the prompt service is called when the value of app.update.auto is
  // changed to false.
  let confirmExReply = discardUpdate ? 0 : 1;
  let didPrompt = false;
  let promptService = {
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
    confirmEx(...args) {
      promptService._confirmExArgs = args;
      didPrompt = true;
      return confirmExReply;
    },
  };
  Services.prompt = promptService;
  await changeAndVerifyPref(tab, newConfigValue);
  is(
    didPrompt,
    expectPrompt,
    `We should ${expectPrompt ? "" : "not "}be prompted`
  );
  is(
    !!(await gUpdateManager.getReadyUpdate()),
    expectRemainingUpdate,
    `There should ${expectRemainingUpdate ? "" : "not "}be a ready update`
  );
}

add_task(async function testUpdateAutoPrefUI() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences"
  );

  // Hack: make the test run faster:
  await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    content.gMainPane._minUpdatePrefDisableTime = 10;
  });

  info("Enable automatic updates and check that works.");
  await changeAndVerifyPref(tab, true);
  ok(
    !(await gUpdateManager.getDownloadingUpdate()),
    "There should not be a downloading update"
  );
  ok(
    !(await gUpdateManager.getReadyUpdate()),
    "There should not be a ready update"
  );

  info("Disable automatic updates and check that works.");
  await changeAndVerifyPref(tab, false);
  ok(
    !(await gUpdateManager.getDownloadingUpdate()),
    "There should not be a downloading update"
  );
  ok(
    !(await gUpdateManager.getReadyUpdate()),
    "There should not be a ready update"
  );

  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { checkInterval: "1" };
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_PENDING);
  reloadUpdateManagerData();
  ok(
    !!(await gUpdateManager.getReadyUpdate()),
    "There should be a ready update"
  );

  let { prompt } = Services;
  registerCleanupFunction(() => {
    Services.prompt = prompt;
  });

  // Setting the value to false will call the prompt service and when we
  // don't discard the update there should still be an active update afterwards.
  await changeAndVerifyUpdateWrites({
    tab,
    newConfigValue: false,
    discardUpdate: false,
    expectPrompt: true,
    expectRemainingUpdate: true,
  });

  // Setting the value to true should not call the prompt service so there
  // should still be an active update, even if we indicate we can discard
  // the update in a hypothetical prompt.
  await changeAndVerifyUpdateWrites({
    tab,
    newConfigValue: true,
    discardUpdate: true,
    expectPrompt: false,
    expectRemainingUpdate: true,
  });

  // Setting the value to false will call the prompt service, and we do
  // discard the update, so there should not be an active update.
  await changeAndVerifyUpdateWrites({
    tab,
    newConfigValue: false,
    discardUpdate: true,
    expectPrompt: true,
    expectRemainingUpdate: false,
  });

  await BrowserTestUtils.removeTab(tab);
});

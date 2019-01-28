/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);

// Changes, then verifies the value of app.update.auto via the about:preferences
// UI. Requires a tab with about:preferences open to be passed in.
async function changeAndVerifyPref(tab, newConfigValue) {
  await ContentTask.spawn(tab.linkedBrowser, {newConfigValue}, async function({newConfigValue}) {
    let radioId = newConfigValue ? "autoDesktop" : "manualDesktop";
    let radioElement = content.document.getElementById(radioId);
    radioElement.click();
  });

  // At this point, we really need to wait for the change to finish being
  // written to the disk before we go to verify anything. Unfortunately, it
  // would be difficult to check for quick changes to the attributes of the
  // about:preferences controls (to wait for the controls to be disabled and
  // re-enabled). So instead, just start the verification by asking the
  // Application Update Service for the value of app.update.auto. It already
  // serializes reads and writes to the app update config file, so this will not
  // resolve until the file write is complete.

  let configValueRead = await UpdateUtils.getAppUpdateAutoEnabled();
  is(configValueRead, newConfigValue,
     "Value returned should have matched the expected value");

  let configFile = getUpdateConfigFile();
  let decoder = new TextDecoder();
  let fileContents = await OS.File.read(configFile.path);
  let saveObject = JSON.parse(decoder.decode(fileContents));
  is(saveObject["app.update.auto"], newConfigValue,
     "Value in file should match expected");

  await ContentTask.spawn(tab.linkedBrowser, {newConfigValue}, async function({newConfigValue}) {
    let updateRadioGroup = content.document.getElementById("updateRadioGroup");
    is(updateRadioGroup.value, `${newConfigValue}`,
       "Update preference should match expected");
  });
}

add_task(async function testUpdateAutoPrefUI() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences");

  await changeAndVerifyPref(tab, true);
  await changeAndVerifyPref(tab, false);
  await changeAndVerifyPref(tab, false);
  await changeAndVerifyPref(tab, true);

  await BrowserTestUtils.removeTab(tab);
});

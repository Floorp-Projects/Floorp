/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing addition of a successful update to " + FILE_UPDATES_XML +
            " and verification of update properties including the format " +
            "prior to bug 530872");

  setUpdateChannel("test_channel");

  // This test expects that the app.update.download.backgroundInterval
  // preference doesn't already exist.
  Services.prefs.deleteBranch("app.update.download.backgroundInterval");

  let patchProps = {type: "partial",
                    url: "http://partial/",
                    hashFunction: "SHA256",
                    hashValue: "cd43",
                    size: "86",
                    selected: "true",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {type: "major",
                     name: "New",
                     displayVersion: "version 4",
                     appVersion: "4.0",
                     buildID: "20070811053724",
                     detailsURL: "http://details1/",
                     serviceURL: "http://service1/",
                     installDate: "1238441300314",
                     statusText: "test status text",
                     isCompleteUpdate: "false",
                     channel: "test_channel",
                     foregroundDownload: "true",
                     promptWaitTime: "345600",
                     backgroundInterval: "300",
                     previousAppVersion: "3.0",
                     custom1: "custom1_attr=\"custom1 value\"",
                     custom2: "custom2_attr=\"custom2 value\""};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  patchProps = {type: "complete",
                url: "http://complete/",
                hashFunction: "SHA1",
                hashValue: "6232",
                size: "75",
                selected: "true",
                state: STATE_FAILED};
  patches = getLocalPatchString(patchProps);
  updateProps = {type: "minor",
                 name: "Existing",
                 appVersion: "3.0",
                 detailsURL: "http://details2/",
                 serviceURL: "http://service2/",
                 statusText: getString("patchApplyFailure"),
                 isCompleteUpdate: "true",
                 channel: "test_channel",
                 foregroundDownload: "false",
                 promptWaitTime: "691200",
                 custom1: "custom3_attr=\"custom3 value\"",
                 custom2: "custom4_attr=\"custom4 value\""};
  updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  standardInit();

  Assert.ok(!gUpdateManager.activeUpdate,
            "the update manager activeUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.updateCount, 2,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);

  debugDump("checking the activeUpdate properties");
  let update = gUpdateManager.getUpdateAt(0).QueryInterface(Ci.nsIPropertyBag);
  Assert.equal(update.state, STATE_SUCCEEDED,
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.type, "major",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.name, "New",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.displayVersion, "version 4",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.appVersion, "4.0",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.buildID, "20070811053724",
               "the update buildID attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details1/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.serviceURL, "http://service1/",
               "the update serviceURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.installDate, "1238441300314",
               "the update installDate attribute" + MSG_SHOULD_EQUAL);
  // statusText is updated
  Assert.equal(update.statusText, getString("installSuccess"),
               "the update statusText attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.isCompleteUpdate,
            "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.channel, "test_channel",
               "the update channel attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "345600",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.getProperty("backgroundInterval"), "300",
               "the update backgroundInterval attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.previousAppVersion, "3.0",
               "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);
  // Custom attributes
  Assert.equal(update.getProperty("custom1_attr"), "custom1 value",
               "the update custom1_attr property" + MSG_SHOULD_EQUAL);
  Assert.equal(update.getProperty("custom2_attr"), "custom2 value",
               "the update custom2_attr property" + MSG_SHOULD_EQUAL);

  debugDump("checking the activeUpdate patch properties");
  let patch = update.selectedPatch;
  Assert.equal(patch.type, "partial",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, "http://partial/",
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "SHA256",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, "cd43",
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, "86",
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.state, STATE_SUCCEEDED,
               "the update patch state attribute" + MSG_SHOULD_EQUAL);

  debugDump("checking the first update properties");
  update = gUpdateManager.getUpdateAt(1).QueryInterface(Ci.nsIPropertyBag);
  Assert.equal(update.state, STATE_FAILED,
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.name, "Existing",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.type, "minor",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.displayVersion, "3.0",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.appVersion, "3.0",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details2/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.serviceURL, "http://service2/",
               "the update serviceURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.installDate, "1238441400314",
               "the update installDate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.statusText, getString("patchApplyFailure"),
               "the update statusText attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.buildID, "20080811053724",
               "the update buildID attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.isCompleteUpdate,
            "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.channel, "test_channel",
               "the update channel attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "691200",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  // The default and maximum value for backgroundInterval is 600
  Assert.equal(update.getProperty("backgroundInterval"), "600",
               "the update backgroundInterval attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.previousAppVersion, null,
               "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);
  // Custom attributes
  Assert.equal(update.getProperty("custom3_attr"), "custom3 value",
               "the update custom3_attr property" + MSG_SHOULD_EQUAL);
  Assert.equal(update.getProperty("custom4_attr"), "custom4 value",
               "the update custom4_attr property" + MSG_SHOULD_EQUAL);

  debugDump("checking the first update patch properties");
  patch = update.selectedPatch;
  Assert.equal(patch.type, "complete",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, "http://complete/",
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "SHA1",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, "6232",
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, "75",
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.state, STATE_FAILED,
               "the update patch state attribute" + MSG_SHOULD_EQUAL);

  doTestFinish();
}

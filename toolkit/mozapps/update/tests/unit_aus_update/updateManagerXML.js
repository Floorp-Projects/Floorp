/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  debugDump("testing addition of a successful update to " + FILE_UPDATES_DB +
            " and verification of update properties including the format " +
            "prior to bug 530872");

  setUpdateChannel("test_channel");

  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
  // and until bug 470244 is fixed this will not test the value for detailsURL 
  // when it isn't specified in the update xml.
  let patches = getLocalPatchString("partial", "http://partial/", "SHA256",
                                    "cd43", "86", "true", STATE_PENDING);
  let updates = getLocalUpdateString(patches, "major", "New", "version 4",
                                     "4.0", "4.0", "20070811053724",
                                     "http://details1/", "http://billboard1/",
                                     "http://license1/", "http://service1/",
                                     "1238441300314", "test status text",
                                     "false", "test_channel", "true", "true",
                                     "true", "345600", "true", "test version",
                                     "3.0", "3.0",
                                     "custom1_attr=\"custom1 value\"",
                                     "custom2_attr=\"custom2 value\"");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  patches = getLocalPatchString("complete", "http://complete/", "SHA1", "6232",
                                "75", "true", STATE_FAILED);
  updates = getLocalUpdateString(patches, "major", "Existing", null, null,
                                 "3.0", null, "http://details2/", null, null,
                                 "http://service2/", null,
                                 getString("patchApplyFailure"), "true",
                                 "test_channel", "false", null, null, "691200",
                                 null, "version 3", "3.0", null,
                                 "custom3_attr=\"custom3 value\"",
                                 "custom4_attr=\"custom4 value\"");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  standardInit();

  Assert.ok(!gUpdateManager.activeUpdate,
            "the update manager activeUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.updateCount, 2,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);

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
  Assert.equal(update.platformVersion, "4.0",
               "the update platformVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.buildID, "20070811053724",
               "the update buildID attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details1/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.billboardURL, "http://billboard1/",
               "the update billboardURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.licenseURL, "http://license1/",
               "the update licenseURL attribute" + MSG_SHOULD_EQUAL);
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
  Assert.ok(!!update.showPrompt,
            "the update showPrompt attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.showNeverForVersion,
            "the update showNeverForVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "345600",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.previousAppVersion, "3.0",
               "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);
  // Custom attributes
  Assert.equal(update.getProperty("custom1_attr"), "custom1 value",
               "the update custom1_attr property" + MSG_SHOULD_EQUAL);
  Assert.equal(update.getProperty("custom2_attr"), "custom2 value",
               "the update custom2_attr property" + MSG_SHOULD_EQUAL);

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

  update = gUpdateManager.getUpdateAt(1).QueryInterface(Ci.nsIPropertyBag);
  Assert.equal(update.state, STATE_FAILED,
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.name, "Existing",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.type, "major",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.displayVersion, "version 3",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.appVersion, "3.0",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.platformVersion, "3.0",
               "the update platformVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details2/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.billboardURL, "http://details2/",
               "the update billboardURL attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.licenseURL,
            "the update licenseURL attribute" + MSG_SHOULD_EQUAL);
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
  Assert.ok(!!update.showPrompt,
            "the update showPrompt attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.showNeverForVersion,
            "the update showNeverForVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "691200",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.previousAppVersion, null,
               "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);
  // Custom attributes
  Assert.equal(update.getProperty("custom3_attr"), "custom3 value",
               "the update custom3_attr property" + MSG_SHOULD_EQUAL);
  Assert.equal(update.getProperty("custom4_attr"), "custom4 value",
               "the update custom4_attr property" + MSG_SHOULD_EQUAL);

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

  removeUpdateDirsAndFiles();

  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
  // and until this is fixed this will not test the value for detailsURL when it
  // isn't specified in the update xml.
  patches = getLocalPatchString(null, null, null, null, null, null,
                                STATE_PENDING);
  updates = getLocalUpdateString(patches, "major", "New", null, null, "4.0",
                                 null, "http://details/", "http://billboard/",
                                 "http://license/", "http://service/",
                                 "1238441400314", "test status text", null,
                                 "test_channel", "true", "true", "true", "100",
                                 "true", "version 4.0", "4.0", "3.0");

  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  patches = getLocalPatchString(null, null, null, null, null, null,
                                STATE_FAILED);
  updates = getLocalUpdateString(patches, "major", "Existing", "version 3.0",
                                 "3.0", "3.0", null, "http://details/", null,
                                 null, "http://service/", null,
                                 getString("patchApplyFailure"), null,
                                 "test_channel", "false", null, null, "200",
                                 null, "version 3", null, null);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  reloadUpdateManagerData();
  initUpdateServiceStub();

  Assert.ok(!gUpdateManager.activeUpdate,
            "the update manager activeUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.updateCount, 2,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);

  update = gUpdateManager.getUpdateAt(0);
  Assert.equal(update.state, STATE_SUCCEEDED,
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.type, "major",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.name, "New",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.displayVersion, "version 4.0",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.appVersion, "4.0",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.platformVersion, "4.0",
               "the update platformVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.billboardURL, "http://billboard/",
               "the update billboardURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.licenseURL, "http://license/",
               "the update licenseURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.serviceURL, "http://service/",
               "the update serviceURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.installDate, "1238441400314",
               "the update installDate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.statusText, getString("installSuccess"),
               "the update statusText attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.buildID, "20080811053724",
               "the update buildID attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.isCompleteUpdate,
            "the update isCompleteUpdate attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.channel, "test_channel",
               "the update channel attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.showPrompt,
            "the update showPrompt attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!update.showNeverForVersion,
            "the update showNeverForVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "100",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.previousAppVersion, "3.0",
               "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);

  patch = update.selectedPatch;
  Assert.equal(patch.type, "complete",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, URL_HOST + "/" + FILE_SIMPLE_MAR,
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "MD5",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, MD5_HASH_SIMPLE_MAR,
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, SIZE_SIMPLE_MAR,
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.state, STATE_SUCCEEDED,
            "the update patch state attribute" + MSG_SHOULD_EQUAL);

  update = gUpdateManager.getUpdateAt(1);
  Assert.equal(update.state, STATE_FAILED,
               "the update state attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.name, "Existing",
               "the update name attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.type, "major",
               "the update type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.displayVersion, "version 3.0",
               "the update displayVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.appVersion, "3.0",
               "the update appVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.platformVersion, "3.0",
               "the update platformVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.detailsURL, "http://details/",
               "the update detailsURL attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.billboardURL,
            "the update billboardURL attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.licenseURL,
            "the update licenseURL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.serviceURL, "http://service/",
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
  Assert.ok(!update.showPrompt,
            "the update showPrompt attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.showNeverForVersion,
            "the update showNeverForVersion attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(update.promptWaitTime, "200",
               "the update promptWaitTime attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!update.previousAppVersion,
            "the update previousAppVersion attribute" + MSG_SHOULD_EQUAL);

  patch = update.selectedPatch;
  Assert.equal(patch.type, "complete",
               "the update patch type attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.URL, URL_HOST + "/" + FILE_SIMPLE_MAR,
               "the update patch URL attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashFunction, "MD5",
               "the update patch hashFunction attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.hashValue, MD5_HASH_SIMPLE_MAR,
               "the update patch hashValue attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.size, SIZE_SIMPLE_MAR,
               "the update patch size attribute" + MSG_SHOULD_EQUAL);
  Assert.ok(!!patch.selected,
            "the update patch selected attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(patch.state, STATE_FAILED,
               "the update patch state attribute" + MSG_SHOULD_EQUAL);

  doTestFinish();
}

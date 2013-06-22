/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Update Manager Tests */

function run_test() {
  do_test_pending();
  do_register_cleanup(end_test);

  logTestInfo("testing addition of a successful update to " + FILE_UPDATES_DB +
              " and verification of update properties with the format prior " +
              "to bug 530872");
  removeUpdateDirsAndFiles();
  setUpdateChannel("test_channel");

  var patch, patches, update, updates;
  // XXXrstrong - not specifying a detailsURL will cause a leak due to bug 470244
  // and until bug 470244 is fixed this will not test the value for detailsURL 
  // when it isn't specified in the update xml.
  patches = getLocalPatchString("partial", "http://partial/", "SHA256", "cd43",
                                "86", "true", STATE_PENDING);
  updates = getLocalUpdateString(patches, "major", "New", "version 4", "4.0",
                                 "4.0", "20070811053724", "http://details1/",
                                 "http://billboard1/", "http://license1/",
                                 "http://service1/", "1238441300314",
                                 "test status text", "false", "test_channel",
                                 "true", "true", "true", "345600", "true",
                                 "test version", "3.0", "3.0",
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

  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 2);

  update = gUpdateManager.getUpdateAt(0).QueryInterface(AUS_Ci.nsIPropertyBag);
  do_check_eq(update.state, STATE_SUCCEEDED);
  do_check_eq(update.type, "major");
  do_check_eq(update.name, "New");
  do_check_eq(update.displayVersion, "version 4");
  do_check_eq(update.appVersion, "4.0");
  do_check_eq(update.platformVersion, "4.0");
  do_check_eq(update.buildID, "20070811053724");
  do_check_eq(update.detailsURL, "http://details1/");
  do_check_eq(update.billboardURL, "http://billboard1/");
  do_check_eq(update.licenseURL, "http://license1/");
  do_check_eq(update.serviceURL, "http://service1/");
  do_check_eq(update.installDate, "1238441300314");
  // statusText is updated
  do_check_eq(update.statusText, getString("installSuccess"));
  do_check_false(update.isCompleteUpdate);
  do_check_eq(update.channel, "test_channel");
  do_check_true(update.showPrompt);
  do_check_true(update.showNeverForVersion);
  do_check_eq(update.promptWaitTime, "345600");
  do_check_eq(update.previousAppVersion, "3.0");
  // Custom attributes
  do_check_eq(update.getProperty("custom1_attr"), "custom1 value");
  do_check_eq(update.getProperty("custom2_attr"), "custom2 value");

  patch = update.selectedPatch;
  do_check_eq(patch.type, "partial");
  do_check_eq(patch.URL, "http://partial/");
  do_check_eq(patch.hashFunction, "SHA256");
  do_check_eq(patch.hashValue, "cd43");
  do_check_eq(patch.size, "86");
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_SUCCEEDED);

  update = gUpdateManager.getUpdateAt(1).QueryInterface(AUS_Ci.nsIPropertyBag);
  do_check_eq(update.state, STATE_FAILED);
  do_check_eq(update.name, "Existing");
  do_check_eq(update.type, "major");
  do_check_eq(update.displayVersion, "version 3");
  do_check_eq(update.appVersion, "3.0");
  do_check_eq(update.platformVersion, "3.0");
  do_check_eq(update.detailsURL, "http://details2/");
  do_check_eq(update.billboardURL, "http://details2/");
  do_check_eq(update.licenseURL, null);
  do_check_eq(update.serviceURL, "http://service2/");
  do_check_eq(update.installDate, "1238441400314");
  do_check_eq(update.statusText, getString("patchApplyFailure"));
  do_check_eq(update.buildID, "20080811053724");
  do_check_true(update.isCompleteUpdate);
  do_check_eq(update.channel, "test_channel");
  do_check_true(update.showPrompt);
  do_check_true(update.showNeverForVersion);
  do_check_eq(update.promptWaitTime, "691200");
  do_check_eq(update.previousAppVersion, null);
  // Custom attributes
  do_check_eq(update.getProperty("custom3_attr"), "custom3 value");
  do_check_eq(update.getProperty("custom4_attr"), "custom4 value");

  patch = update.selectedPatch;
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, "http://complete/");
  do_check_eq(patch.hashFunction, "SHA1");
  do_check_eq(patch.hashValue, "6232");
  do_check_eq(patch.size, "75");
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_FAILED);

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

  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 2);

  update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);
  do_check_eq(update.type, "major");
  do_check_eq(update.name, "New");
  do_check_eq(update.displayVersion, "version 4.0");
  do_check_eq(update.appVersion, "4.0");
  do_check_eq(update.platformVersion, "4.0");
  do_check_eq(update.detailsURL, "http://details/");
  do_check_eq(update.billboardURL, "http://billboard/");
  do_check_eq(update.licenseURL, "http://license/");
  do_check_eq(update.serviceURL, "http://service/");
  do_check_eq(update.installDate, "1238441400314");
  do_check_eq(update.statusText, getString("installSuccess"));
  do_check_eq(update.buildID, "20080811053724");
  do_check_true(update.isCompleteUpdate);
  do_check_eq(update.channel, "test_channel");
  do_check_true(update.showPrompt);
  do_check_true(update.showNeverForVersion);
  do_check_eq(update.promptWaitTime, "100");
  do_check_eq(update.previousAppVersion, "3.0");

  patch = update.selectedPatch;
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, URL_HOST + URL_PATH + "/" + FILE_SIMPLE_MAR);
  do_check_eq(patch.hashFunction, "MD5");
  do_check_eq(patch.hashValue, MD5_HASH_SIMPLE_MAR);
  do_check_eq(patch.size, SIZE_SIMPLE_MAR);
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_SUCCEEDED);

  update = gUpdateManager.getUpdateAt(1);
  do_check_eq(update.state, STATE_FAILED);
  do_check_eq(update.name, "Existing");
  do_check_eq(update.type, "major");
  do_check_eq(update.displayVersion, "version 3.0");
  do_check_eq(update.appVersion, "3.0");
  do_check_eq(update.platformVersion, "3.0");
  do_check_eq(update.detailsURL, "http://details/");
  do_check_eq(update.billboardURL, null);
  do_check_eq(update.licenseURL, null);
  do_check_eq(update.serviceURL, "http://service/");
  do_check_eq(update.installDate, "1238441400314");
  do_check_eq(update.statusText, getString("patchApplyFailure"));
  do_check_eq(update.buildID, "20080811053724");
  do_check_true(update.isCompleteUpdate);
  do_check_eq(update.channel, "test_channel");
  do_check_false(update.showPrompt);
  do_check_false(update.showNeverForVersion);
  do_check_eq(update.promptWaitTime, "200");
  do_check_eq(update.previousAppVersion, null);

  patch = update.selectedPatch;
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, URL_HOST + URL_PATH + "/" + FILE_SIMPLE_MAR);
  do_check_eq(patch.hashFunction, "MD5");
  do_check_eq(patch.hashValue, MD5_HASH_SIMPLE_MAR);
  do_check_eq(patch.size, SIZE_SIMPLE_MAR);
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_FAILED);

  do_test_finished();
}

function end_test() {
  cleanUp();
}

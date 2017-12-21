/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Application Update URL Construction Tests */

/**
 * Tests for the majority of values are located in
 * toolkit/modules/tests/xpcshell/test_UpdateUtils_updatechannel.js
 * toolkit/modules/tests/xpcshell/test_UpdateUtils_url.js
 */

const URL_PREFIX = URL_HOST + "/";

function run_test() {
  setupTestCommon();

  standardInit();
  executeSoon(run_test_pt1);
}

// Test that the force param is present
function run_test_pt1() {
  gCheckFunc = check_test_pt1;
  let url = URL_PREFIX;
  debugDump("testing url force param is present: " + url);
  setUpdateURL(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  let url = URL_PREFIX + "?force=1";
  Assert.equal(gRequestURL, url,
               "the url" + MSG_SHOULD_EQUAL);
  run_test_pt2();
}

// Test that force param is present when there is already a param - bug 454357
function run_test_pt2() {
  gCheckFunc = check_test_pt2;
  let url = URL_PREFIX + "?testparam=1";
  debugDump("testing url force param when there is already a param present: " +
            url);
  setUpdateURL(url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt2() {
  let url = URL_PREFIX + "?testparam=1&force=1";
  Assert.equal(gRequestURL, url,
               "the url" + MSG_SHOULD_EQUAL);
  doTestFinish();
}

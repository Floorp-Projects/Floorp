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

async function run_test() {
  const URL_PREFIX = URL_HOST + "/";
  setupTestCommon();
  let url = URL_PREFIX;
  debugDump("testing url force param is present: " + url);
  setUpdateURL(url);
  await waitForUpdateCheck(false, { url: url + "?force=1" });
  url = URL_PREFIX + "?testparam=1";
  debugDump("testing url force param when there is already a param: " + url);
  setUpdateURL(url);
  await waitForUpdateCheck(false, { url: url + "&force=1" });
  doTestFinish();
}

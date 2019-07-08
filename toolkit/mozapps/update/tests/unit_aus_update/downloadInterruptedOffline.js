/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  setupTestCommon();
  debugDump("testing mar download when offline");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  initMockIncrementalDownload();
  gIncrementalDownloadErrorType = 4;
  let patches = getRemotePatchString({});
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, { updateCount: 1 }).then(async aArgs => {
    await waitForUpdateDownload(aArgs.updates, Cr.NS_OK);
  });
  stop_httpserver(doTestFinish);
}

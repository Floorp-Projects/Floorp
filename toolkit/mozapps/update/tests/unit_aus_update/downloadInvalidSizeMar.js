/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

async function run_test() {
  // The network code that downloads the mar file accesses the profile to cache
  // the download, but the profile is only available after calling
  // do_get_profile in xpcshell tests. This prevents an error from being logged.
  do_get_profile();
  setupTestCommon();
  debugDump("testing mar download with an invalid file size");
  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  start_httpserver();
  setUpdateURL(gURLData + gHTTPHandlerPath);
  let patchProps = {size: "1024000"};
  let patches = getRemotePatchString(patchProps);
  let updates = getRemoteUpdateString({}, patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  await waitForUpdateCheck(true, {updateCount: 1}).then(async (aArgs) => {
    await waitForUpdateDownload(aArgs.updates, aArgs.updateCount,
                                Cr.NS_ERROR_UNEXPECTED);
  });
  // There is a pending write to the xml files.
  await waitForUpdateXMLFiles();
  stop_httpserver(doTestFinish);
}

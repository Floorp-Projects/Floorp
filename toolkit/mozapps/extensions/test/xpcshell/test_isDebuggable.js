/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var ID = "bootstrap2@tests.mozilla.org";

const ADDON = {
  id: ID,
  version: "1.0",
  bootstrap: "true",
  multiprocessCompatible: "true",

  name: "Test Bootstrap 2",
  description: "Test Description",

  iconURL: "chrome://foo/skin/icon.png",
  aboutURL: "chrome://foo/content/about.xul",
  optionsURL: "chrome://foo/content/options.xul",

  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"}],
};

add_task(async function() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  await promiseStartupManager();
  AddonManager.checkCompatibility = false;

  await promiseInstallXPI(ADDON);

  let addon = await AddonManager.getAddonByID(ID);
  Assert.equal(addon.isDebuggable, true);
});

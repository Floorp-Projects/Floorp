/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var installLocation = gProfD.clone();
installLocation.append("baddir");
installLocation.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0o664);

var dirProvider2 = {
  getFile(prop, persistent) {
    persistent.value = true;
    if (prop == "XREUSysExt")
      return installLocation.clone();
    return null;
  },
  QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
        iid.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};
Services.dirsvc.QueryInterface(Components.interfaces.nsIDirectoryService)
               .registerProvider(dirProvider2);

function run_test() {
  var log = gProfD.clone();
  log.append("extensions.log");
  do_check_false(log.exists());

  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  startupManager();
  do_check_false(log.exists());
}

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is the script that runs in the child xpcshell process for the test
// unit_aus_update/updateSyncManager.js.
// The main thing this script does is override the child's directory service
// so that it ends up with the same fake binary path that the parent test runner
// has opened its update lock with.
// This requires that we have already been passed a constant on our command
// line which contains the relevant fake binary path, which is called:
/* global customExePath */

print("child process is running");

// This function is copied from xpcshellUtilsAUS.js so that we can have our
// xpcshell subprocess call it without having to load that whole file, because
// it turns out that needs a bunch of infrastructure that normally the testing
// framework would provide, and that also requires a bunch of setup, and it's
// just not worth all that. This is a cut down version that only includes the
// directory provider functionality that the subprocess really needs.
function adjustGeneralPaths() {
  let dirProvider = {
    getFile: function AGP_DP_getFile(aProp, aPersistent) {
      // Set the value of persistent to false so when this directory provider is
      // unregistered it will revert back to the original provider.
      aPersistent.value = false;
      // The sync manager only needs XREExeF, so that's all we provide.
      if (aProp == "XREExeF") {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(customExePath);
        return file;
      }
      return null;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };
  let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
  ds.QueryInterface(Ci.nsIProperties).undefine("XREExeF");
  ds.registerProvider(dirProvider);

  // Now that we've overridden the directory provider, the name of the update
  // lock needs to be changed to match the overridden path.
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  syncManager.resetLock();
}

adjustGeneralPaths();

// Wait a few seconds for the parent to do what it needs to do, then exit.
print("child process should now have the lock; will exit in 5 seconds");
simulateNoScriptActivity(5);
print("child process exiting now");

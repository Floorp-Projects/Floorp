/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  setupTestCommon();

  // Verify write access to the custom app dir
  debugDump("testing write access to the application directory");
  let testFile = getCurrentProcessDir();
  testFile.append("update_write_access_test");
  testFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
  Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
  testFile.remove(false);
  Assert.ok(!testFile.exists(), MSG_SHOULD_NOT_EXIST);

  if (AppConstants.platform == "win") {
    // Create a mutex to prevent being able to check for or apply updates.
    debugDump("attempting to create mutex");
    let handle = createMutex(getPerInstallationMutexName());
    Assert.ok(!!handle, "the update mutex should have been created");

    // Check if available updates cannot be checked for when there is a mutex
    // for this installation.
    Assert.ok(
      !gAUS.canCheckForUpdates,
      "should not be able to check for " +
        "updates when there is an update mutex"
    );

    // Check if updates cannot be applied when there is a mutex for this
    // installation.
    Assert.ok(
      !gAUS.canApplyUpdates,
      "should not be able to apply updates when there is an update mutex"
    );

    debugDump("destroying mutex");
    closeHandle(handle);
  }

  // Check if available updates can be checked for
  Assert.ok(gAUS.canCheckForUpdates, "should be able to check for updates");
  // Check if updates can be applied
  Assert.ok(gAUS.canApplyUpdates, "should be able to apply updates");

  if (AppConstants.platform == "win") {
    // Attempt to create a mutex when application update has already created one
    // with the same name.
    debugDump("attempting to create mutex");
    let handle = createMutex(getPerInstallationMutexName());

    Assert.ok(
      !handle,
      "should not be able to create the update mutex when " +
        "the application has created the update mutex"
    );
  }

  doTestFinish();
}

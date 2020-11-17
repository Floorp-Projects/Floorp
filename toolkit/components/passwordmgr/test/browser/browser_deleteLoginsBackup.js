/**
 * Test that logins backup is deleted as expected when logins are deleted.
 */

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  FXA_PWDMGR_HOST: "resource://gre/modules/FxAccountsCommon.js",
  FXA_PWDMGR_REALM: "resource://gre/modules/FxAccountsCommon.js",
});

const nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);

const login1 = new nsLoginInfo(
  "http://example.com",
  "http://example.com",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);
const login2 = new nsLoginInfo(
  "http://example.com",
  "http://example.com",
  null,
  "",
  "notifyp1",
  "",
  "pass"
);

const fxaKey = new nsLoginInfo(
  FXA_PWDMGR_HOST,
  null,
  FXA_PWDMGR_REALM,
  "foo@bar.com",
  "pass2",
  "",
  ""
);

const loginStorePath = OS.Path.join(
  OS.Constants.Path.profileDir,
  "logins.json"
);
const loginBackupPath = OS.Path.join(
  OS.Constants.Path.profileDir,
  "logins-backup.json"
);

async function waitForBackupUpdate() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic) {
      Services.obs.removeObserver(observer, "logins-backup-updated");
      resolve();
    }, "logins-backup-updated");
  });
}

async function loginStoreExists() {
  return TestUtils.waitForCondition(async () => {
    let fileExists = await OS.File.exists(loginStorePath);
    return fileExists;
  });
}

async function loginBackupExists() {
  return TestUtils.waitForCondition(async () => {
    let fileExists = await OS.File.exists(loginBackupPath);
    return fileExists;
  });
}

async function loginBackupDeleted() {
  return TestUtils.waitForCondition(async () => {
    let fileExists = await OS.File.exists(loginBackupPath);
    return !fileExists;
  });
}

// 1. Test that logins backup is deleted when Services.logins.removeAllUserFacingLogins() is called.
// 2. Test that logins backup is deleted when the last saved login is removed using
//    Services.logins.removeLogin() when no fxa key is saved.
// 3. If a fxa key is stored as a login, test that logins backup is updated to only store
//    the fxa key when the last user facing login is deleted.
add_task(async function test_deleteLoginsBackup_removeAll() {
  // Remove logins.json and logins-backup.json before starting.
  info("Testing the removeAllUserFacingLogins() case");

  await OS.File.remove(loginStorePath, { ignoreAbsent: true });
  await OS.File.remove(loginBackupPath, { ignoreAbsent: true });

  let storageUpdatePromise = TestUtils.topicObserved(
    "password-storage-updated"
  );
  info("Add a login to create logins.json");
  Services.logins.addLogin(login1);
  await loginStoreExists();
  ok(true, "logins.json now exists");

  info("Add a second login to create logins-backup.json");
  Services.logins.addLogin(login2);
  await loginBackupExists();
  info("logins-backup.json now exists");

  await storageUpdatePromise;
  info("Writes to storage are complete for addLogin calls");

  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  info("Removing all logins");
  Services.logins.removeAllUserFacingLogins();

  await storageUpdatePromise;
  info("Writes to storage are complete when removeAllLogins() is called");
  await loginBackupDeleted();
  info(
    "logins-backup.json was deleted as expected when all logins were removed"
  );

  info("Testing the removeLogin() case when there is no saved fxa key");
  info("Adding two logins");
  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  Services.logins.addLogin(login1);
  await loginStoreExists();
  Services.logins.addLogin(login2);
  await loginBackupExists();
  info("logins-backup.json now exists");

  await storageUpdatePromise;
  info("Writes to storage are complete for addLogin calls");

  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  info("Removing one login");
  Services.logins.removeLogin(login1);
  await storageUpdatePromise;
  info("Writes to storage are complete after one removeLogin call");
  await loginBackupExists();

  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  info("Removing the last login");
  Services.logins.removeLogin(login2);
  await storageUpdatePromise;
  info("Writes to storage are complete after the last removeLogin call");
  await loginBackupDeleted();
  info(
    "logins-backup.json was deleted as expected when the last saved login was removed"
  );

  info("Testing the removeLogin() case when there is a saved fxa key");
  info("Adding two logins: fxa key and a user facing login");
  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  Services.logins.addLogin(login1);
  await loginStoreExists();
  Services.logins.addLogin(fxaKey);
  await loginBackupExists();
  info("logins-backup.json now exists");
  await storageUpdatePromise;
  info("Writes to storage are complete for addLogin calls");

  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  info("Removing the last user facing login");
  Services.logins.removeLogin(login1);
  await storageUpdatePromise;
  info("Writes to storage are complete after one removeLogin call");
  await waitForBackupUpdate();
  info("logins-backup.json was updated to only store the fxa key, as expected");

  storageUpdatePromise = TestUtils.topicObserved("password-storage-updated");
  info("Removing the fxa key");
  Services.logins.removeLogin(fxaKey);
  await storageUpdatePromise;
  info("Writes to storage are complete after the last removeLogin call");
  await loginBackupDeleted();
  info(
    "logins-backup.json was deleted when the last login was removed, as expected"
  );

  // Clean up.
  await OS.File.remove(loginStorePath);
});

const { ContentTaskUtils } = ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm");
const PWMGR_DLG = "chrome://passwordmgr/content/passwordManager.xul";

var doc;
var pwmgr;
var pwmgrdlg;
var signonsTree;

function addLogin(site, username, password) {
  let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                               Ci.nsILoginInfo, "init");
  let login = new nsLoginInfo(site, site, null, username, password, "u", "p");
  Services.logins.addLogin(login);
}

function getUsername(row) {
  return signonsTree.view.getCellText(row, signonsTree.columns.getNamedColumn("userCol"));
}

function getPassword(row) {
  return signonsTree.view.getCellText(row, signonsTree.columns.getNamedColumn("passwordCol"));
}

function synthesizeDblClickOnCell(aTree, column, row) {
  let rect = aTree.getCoordsForCellItem(row, aTree.columns[column], "text");
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  // Simulate the double click.
  EventUtils.synthesizeMouse(aTree.body, x, y, { clickCount: 2 },
                             aTree.ownerGlobal);
}

async function togglePasswords() {
  pwmgrdlg.document.querySelector("#togglePasswords").doCommand();
  await ContentTaskUtils.waitForCondition(() => !signonsTree.columns.getNamedColumn("passwordCol").hidden,
                                          "Waiting for Passwords Column to Show/Hide");
  await new Promise(resolve => waitForFocus(resolve, pwmgrdlg));
  pwmgrdlg.document.documentElement.clientWidth; // flush to ensure UI up-to-date
}

async function editUsernamePromises(site, oldUsername, newUsername) {
  is(Services.logins.findLogins(site, "", "").length, 1, "Correct login found");
  let login = Services.logins.findLogins(site, "", "")[0];
  is(login.username, oldUsername, "Correct username saved");
  is(getUsername(0), oldUsername, "Correct username shown");
  let focusPromise = BrowserTestUtils.waitForEvent(signonsTree.inputField, "focus", true);
  synthesizeDblClickOnCell(signonsTree, 1, 0);
  await focusPromise;

  EventUtils.sendString(newUsername, pwmgrdlg);
  let signonsIntro = doc.querySelector("#signonsIntro");
  EventUtils.sendMouseEvent({type: "click"}, signonsIntro, pwmgrdlg);
  await ContentTaskUtils.waitForCondition(() => !signonsTree.getAttribute("editing"),
                                          "Waiting for editing to stop");

  is(Services.logins.findLogins(site, "", "").length, 1, "Correct login replaced");
  login = Services.logins.findLogins(site, "", "")[0];
  is(login.username, newUsername, "Correct username updated");
  is(getUsername(0), newUsername, "Correct username shown after the update");
}

async function editPasswordPromises(site, oldPassword, newPassword) {
  is(Services.logins.findLogins(site, "", "").length, 1, "Correct login found");
  let login = Services.logins.findLogins(site, "", "")[0];
  is(login.password, oldPassword, "Correct password saved");
  is(getPassword(0), oldPassword, "Correct password shown");

  let focusPromise = BrowserTestUtils.waitForEvent(signonsTree.inputField, "focus", true);
  synthesizeDblClickOnCell(signonsTree, 2, 0);
  await focusPromise;

  EventUtils.sendString(newPassword, pwmgrdlg);
  let signonsIntro = doc.querySelector("#signonsIntro");
  EventUtils.sendMouseEvent({type: "click"}, signonsIntro, pwmgrdlg);
  await ContentTaskUtils.waitForCondition(() => !signonsTree.getAttribute("editing"),
                                          "Waiting for editing to stop");

  is(Services.logins.findLogins(site, "", "").length, 1, "Correct login replaced");
  login = Services.logins.findLogins(site, "", "")[0];
  is(login.password, newPassword, "Correct password updated");
  is(getPassword(0), newPassword, "Correct password shown after the update");
}

add_task(async function test_setup() {
  registerCleanupFunction(function() {
    Services.logins.removeAllLogins();
  });

  Services.logins.removeAllLogins();
  // Open the password manager dialog.
  pwmgrdlg = window.openDialog(PWMGR_DLG, "Toolkit:PasswordManager", "");

  Services.ww.registerNotification(function notification(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let win = aSubject;
      SimpleTest.waitForFocus(function() {
        EventUtils.sendKey("RETURN", win);
      }, win);
    } else if (aSubject.location == pwmgrdlg.location && aTopic == "domwindowclosed") {
      // Unregister ourself.
      Services.ww.unregisterNotification(notification);
    }
  });

  await new Promise((resolve) => {
    SimpleTest.waitForFocus(() => {
      doc = pwmgrdlg.document;
      signonsTree = doc.querySelector("#signonsTree");
      resolve();
    }, pwmgrdlg);
  });
});

add_task(async function test_edit_multiple_logins() {
  async function testLoginChange(site, oldUsername, oldPassword, newUsername, newPassword) {
    addLogin(site, oldUsername, oldPassword);
    await editUsernamePromises(site, oldUsername, newUsername);
    await togglePasswords();
    await editPasswordPromises(site, oldPassword, newPassword);
    await togglePasswords();
  }

  await testLoginChange("http://c.tn/", "userC", "passC", "usernameC", "passwordC");
  await testLoginChange("http://b.tn/", "userB", "passB", "usernameB", "passwordB");
  await testLoginChange("http://a.tn/", "userA", "passA", "usernameA", "passwordA");

  pwmgrdlg.close();
});

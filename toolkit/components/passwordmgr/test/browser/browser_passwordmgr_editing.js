const { ContentTaskUtils } = Cu.import("resource://testing-common/ContentTaskUtils.jsm", {});
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
  let tbo = aTree.treeBoxObject;
  let rect = tbo.getCoordsForCellItem(row, aTree.columns[column], "text");
  let x = rect.x + rect.width / 2;
  let y = rect.y + rect.height / 2;
  // Simulate the double click.
  EventUtils.synthesizeMouse(aTree.body, x, y, { clickCount: 2 },
                             aTree.ownerDocument.defaultView);
}

function* togglePasswords() {
  pwmgrdlg.document.querySelector("#togglePasswords").doCommand();
  yield new Promise(resolve => waitForFocus(resolve, pwmgrdlg));
}

function* editUsernamePromises(site, oldUsername, newUsername) {
  is(Services.logins.findLogins({}, site, "", "").length, 1, "Correct login found");
  let login = Services.logins.findLogins({}, site, "", "")[0];
  is(login.username, oldUsername, "Correct username saved");
  is(getUsername(0), oldUsername, "Correct username shown");
  synthesizeDblClickOnCell(signonsTree, 1, 0);
  yield ContentTaskUtils.waitForCondition(() => signonsTree.getAttribute("editing"),
                                          "Waiting for editing");

  EventUtils.sendString(newUsername, pwmgrdlg);
  let signonsIntro = doc.querySelector("#signonsIntro");
  EventUtils.sendMouseEvent({type: "click"}, signonsIntro, pwmgrdlg);
  yield ContentTaskUtils.waitForCondition(() => !signonsTree.getAttribute("editing"),
                                          "Waiting for editing to stop");

  is(Services.logins.findLogins({}, site, "", "").length, 1, "Correct login replaced");
  login = Services.logins.findLogins({}, site, "", "")[0];
  is(login.username, newUsername, "Correct username updated");
  is(getUsername(0), newUsername, "Correct username shown after the update");
}

function* editPasswordPromises(site, oldPassword, newPassword) {
  is(Services.logins.findLogins({}, site, "", "").length, 1, "Correct login found");
  let login = Services.logins.findLogins({}, site, "", "")[0];
  is(login.password, oldPassword, "Correct password saved");
  is(getPassword(0), oldPassword, "Correct password shown");

  synthesizeDblClickOnCell(signonsTree, 2, 0);
  yield ContentTaskUtils.waitForCondition(() => signonsTree.getAttribute("editing"),
                                          "Waiting for editing");

  EventUtils.sendString(newPassword, pwmgrdlg);
  let signonsIntro = doc.querySelector("#signonsIntro");
  EventUtils.sendMouseEvent({type: "click"}, signonsIntro, pwmgrdlg);
  yield ContentTaskUtils.waitForCondition(() => !signonsTree.getAttribute("editing"),
                                          "Waiting for editing to stop");

  is(Services.logins.findLogins({}, site, "", "").length, 1, "Correct login replaced");
  login = Services.logins.findLogins({}, site, "", "")[0];
  is(login.password, newPassword, "Correct password updated");
  is(getPassword(0), newPassword, "Correct password shown after the update");
}

add_task(function* test_setup() {
  registerCleanupFunction(function() {
    Services.logins.removeAllLogins();
  });

  Services.logins.removeAllLogins();
  // Open the password manager dialog.
  pwmgrdlg = window.openDialog(PWMGR_DLG, "Toolkit:PasswordManager", "");

  Services.ww.registerNotification(function(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
      SimpleTest.waitForFocus(function() {
        EventUtils.sendKey("RETURN", win);
      }, win);
    } else if (aSubject.location == pwmgrdlg.location && aTopic == "domwindowclosed") {
      // Unregister ourself.
      Services.ww.unregisterNotification(arguments.callee);
    }
  });

  yield new Promise((resolve) => {
    SimpleTest.waitForFocus(() => {
      doc = pwmgrdlg.document;
      signonsTree = doc.querySelector("#signonsTree");
      resolve();
    }, pwmgrdlg);
  });
});

add_task(function* test_edit_multiple_logins() {
  function* testLoginChange(site, oldUsername, oldPassword, newUsername, newPassword) {
    addLogin(site, oldUsername, oldPassword);
    yield* editUsernamePromises(site, oldUsername, newUsername);
    yield* togglePasswords();
    yield* editPasswordPromises(site, oldPassword, newPassword);
    yield* togglePasswords();
  }

  yield* testLoginChange("http://c.tn/", "userC", "passC", "usernameC", "passwordC");
  yield* testLoginChange("http://b.tn/", "userB", "passB", "usernameB", "passwordB");
  yield* testLoginChange("http://a.tn/", "userA", "passA", "usernameA", "passwordA");

  pwmgrdlg.close();
});

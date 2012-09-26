/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  const LOGIN_HOST = "http://example.com";
  const LOGIN_COUNT = 5;

  let nsLoginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
  let pmDialog = window.openDialog(
    "chrome://passwordmgr/content/passwordManager.xul",
    "Toolkit:PasswordManager", "");
  let pmexDialog = window.openDialog(
    "chrome://passwordmgr/content/passwordManagerExceptions.xul",
    "Toolkit:PasswordManagerExceptions", "");
  let logins = [];
  let loginCounter = 0;
  let modifiedLogin;
  let testNumber = 0;
  let testObserver = {
    observe: function (subject, topic, data) {
      if (topic == "passwordmgr-dialog-updated") {
        switch (testNumber) {
          case 1:
            is(countLogins(), loginCounter, "Verify login added");
            runNextTest();
            break;
          case 2:
            is(countLogins(), loginCounter, "Verify login count");
            is(getLoginPassword(), "newpassword0", "Verify login modified");
            runNextTest();
            break;
          case 3:
            is(countLogins(), loginCounter, "Verify login removed");
            runNextTest();
            break;
          case 4:
          case 5:
          case 6:
          case 7:
          case 8:
            is(countLogins(), loginCounter, "Verify login added");
            runNextTest();
            break;
          case 9:
            is(countLogins(), 0, "Verify all logins removed");
            runNextTest();
            break;
          case 10:
            is(countDisabledHosts(), 1, "Verify disabled host added");
            runNextTest();
            break;
          case 11:
            is(countDisabledHosts(), 0, "Verify disabled host removed");
            runNextTest();
            break;
        }
      }
    }
  };

  SimpleTest.waitForFocus(startTest, pmDialog);

  function createLogins() {
    let login;
    for (let i = 0; i < LOGIN_COUNT; i++) {
      login = new nsLoginInfo(LOGIN_HOST + "?n=" + i, LOGIN_HOST + "?n=" + i,
        null, "user" + i, "password" + i, "u" + i, "p" + i);
      logins.push(login);
    }
    modifiedLogin = new nsLoginInfo(LOGIN_HOST + "?n=0", LOGIN_HOST + "?n=0",
      null, "user0", "newpassword0", "u0", "p0");
    is(logins.length, LOGIN_COUNT, "Verify logins created");
  }

  function countDisabledHosts() {
    let doc = pmexDialog.document;
    let rejectsTree = doc.getElementById("rejectsTree");
    return rejectsTree.view.rowCount;
  }

  function countLogins() {
    let doc = pmDialog.document;
    let signonsTree = doc.getElementById("signonsTree");
    return signonsTree.view.rowCount;
  }

  function getLoginPassword() {
    let doc = pmDialog.document;
    let loginsTree = doc.getElementById("signonsTree");
    let column = loginsTree.columns[2]; // password column
    return loginsTree.view.getCellText(0, column);
  }

  function startTest() {
    Services.obs.addObserver(
      testObserver, "passwordmgr-dialog-updated", false);
    is(countLogins(), 0, "Verify starts with 0 logins");
    createLogins();
    runNextTest();
  }

  function runNextTest() {
    switch (++testNumber) {
      case 1: // add a login
        loginCounter++;
        Services.logins.addLogin(logins[0]);
        break;
      case 2: // modify a login
        Services.logins.modifyLogin(logins[0], modifiedLogin);
        break;
      case 3: // remove a login
        loginCounter--;
        Services.logins.removeLogin(modifiedLogin);
        break;
      case 4: // add all logins
        for (let i = 0; i < logins.length; i++) {
          loginCounter++;
          Services.logins.addLogin(logins[i]);
        }
        break;
      case 9: // remove all logins
        Services.logins.removeAllLogins();
        break;
      case 10: // save a disabled host
        pmDialog.close();
        SimpleTest.waitForFocus(function() {
          Services.logins.setLoginSavingEnabled(LOGIN_HOST, false);
        }, pmexDialog);
        break;
      case 11: // remove a disabled host
        Services.logins.setLoginSavingEnabled(LOGIN_HOST, true);
        break;
      case 12: // finish
        Services.obs.removeObserver(
          testObserver, "passwordmgr-dialog-updated", false);
        pmexDialog.close();
        finish();
        break;
    }
  }
}

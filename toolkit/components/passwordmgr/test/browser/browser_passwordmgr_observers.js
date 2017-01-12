/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test() {
  yield new Promise(resolve => {

  const LOGIN_HOST = "http://example.com";
  const LOGIN_COUNT = 5;

  let nsLoginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1", Ci.nsILoginInfo, "init");
  let pmDialog = window.openDialog(
    "chrome://passwordmgr/content/passwordManager.xul",
    "Toolkit:PasswordManager", "");

  let logins = [];
  let loginCounter = 0;
  let loginOrder = null;
  let modifiedLogin;
  let testNumber = 0;
  let testObserver = {
    observe(subject, topic, data) {
      if (topic == "passwordmgr-dialog-updated") {
        switch (testNumber) {
          case 1:
          case 2:
          case 3:
          case 4:
          case 5:
            is(countLogins(), loginCounter, "Verify login added");
            ok(getLoginOrder().startsWith(loginOrder), "Verify login order");
            runNextTest();
            break;
          case 6:
            is(countLogins(), loginCounter, "Verify login count");
            is(getLoginOrder(), loginOrder, "Verify login order");
            is(getLoginPassword(), "newpassword0", "Verify login modified");
            runNextTest();
            break;
          case 7:
            is(countLogins(), loginCounter, "Verify login removed");
            ok(loginOrder.endsWith(getLoginOrder()), "Verify login order");
            runNextTest();
            break;
          case 8:
            is(countLogins(), 0, "Verify all logins removed");
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

  function countLogins() {
    let doc = pmDialog.document;
    let signonsTree = doc.getElementById("signonsTree");
    return signonsTree.view.rowCount;
  }

  function getLoginOrder() {
    let doc = pmDialog.document;
    let signonsTree = doc.getElementById("signonsTree");
    let column = signonsTree.columns[0]; // host column
    let order = [];
    for (let i = 0; i < signonsTree.view.rowCount; i++) {
      order.push(signonsTree.view.getCellText(i, column));
    }
    return order.join(',');
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
      case 1: // add the logins
        for (let i = 0; i < logins.length; i++) {
          loginCounter++;
          loginOrder = getLoginOrder();
          Services.logins.addLogin(logins[i]);
        }
        break;
      case 6: // modify a login
        loginOrder = getLoginOrder();
        Services.logins.modifyLogin(logins[0], modifiedLogin);
        break;
      case 7: // remove a login
        loginCounter--;
        loginOrder = getLoginOrder();
        Services.logins.removeLogin(modifiedLogin);
        break;
      case 8: // remove all logins
        Services.logins.removeAllLogins();
        break;
      case 9: // finish
        Services.obs.removeObserver(
          testObserver, "passwordmgr-dialog-updated");
        pmDialog.close();
        resolve();
        break;
    }
  }

  });
});

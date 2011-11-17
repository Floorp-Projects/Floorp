var testURL_01 = chromeRoot + "browser_blank_01.html";

// Tests for the Remember Password UI

let gCurrentTab = null;
function test() {
  waitForExplicitFinish();

  messageManager.addMessageListener("pageshow", function() {
    if (gCurrentTab.browser.currentURI.spec == testURL_01) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      pageLoaded();
    }
  });

  gCurrentTab = Browser.addTab(testURL_01, true);
}

function pageLoaded() {
  let iHandler = getIdentityHandler();
  let iPassword = document.getElementById("pageaction-password");
  let lm = getLoginManager();
  let host = gCurrentTab.browser.currentURI.prePath;
  let nullSubmit = createLogin(host, host, null);
  let nullForm   = createLogin(host, null, "realm");

  lm.removeAllLogins();

  iHandler.show();
  is(iPassword.hidden, true, "Remember password hidden for no logins");
  iHandler.hide();

  lm.addLogin(nullSubmit);
  iHandler.show();
  is(iPassword.hidden, false, "Remember password shown for form logins");
  iPassword.click();
  is(iPassword.hidden, true, "Remember password hidden after click");
  is(lm.countLogins(host, "", null), 0, "Logins deleted when clicked");
  iHandler.hide();

  lm.addLogin(nullForm);
  iHandler.show();
  is(iPassword.hidden, false, "Remember password shown for protocol logins");
  iPassword.click();
  is(iPassword.hidden, true, "Remember password hidden after click");
  is(lm.countLogins(host, null, ""), 0, "Logins deleted when clicked");
  iHandler.hide();

  Browser.closeTab(gCurrentTab);
  finish();
}

function getLoginManager() {
  return Components.classes["@mozilla.org/login-manager;1"].getService(Components.interfaces.nsILoginManager);
}

function createLogin(aHostname, aFormSubmitURL, aRealm) {
  let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1", Components.interfaces.nsILoginInfo, "init");
  return new nsLoginInfo(aHostname, aFormSubmitURL, aRealm, "username", "password", "uname", "pword");   
}

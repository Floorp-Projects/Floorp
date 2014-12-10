/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AndroidLog.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

const LOGIN_FIELDS = {
  hostname: "http://example.org/tests/robocop/robocop_blank_01.html",
  formSubmitUrl: "",
  realmAny: null,
  username: "username1",
  password: "password1",
  usernameField: "",
  passwordField: ""
};

const LoginInfo = Components.Constructor("@mozilla.org/login-manager/loginInfo;1", "nsILoginInfo", "init");

let BrowserApp;
let browser;

function add_login(login) {
  let newLogin = new LoginInfo(login.hostname,
                               login.formSubmitUrl,
                               login.realmAny,
                               login.username,
                               login.password,
                               login.usernameField,
                               login.passwordField);

  Services.logins.addLogin(newLogin);
}

add_test(function password_setup() {
  add_login(LOGIN_FIELDS);

  // Load about:passwords.
  BrowserApp = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
  browser = BrowserApp.addTab("about:passwords", { selected: true, parentId: BrowserApp.selectedTab.id }).browser;

  browser.addEventListener("load", () => {
    browser.removeEventListener("load", this, true);
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, true);
});

add_test(function test_passwords_list() {
  // Test that the (single) entry added in setup is correct.
  let logins_list = browser.contentDocument.getElementById("logins-list");

  let hostname = logins_list.querySelector(".hostname");
  do_check_eq(hostname.textContent, LOGIN_FIELDS.hostname);

  let username = logins_list.querySelector(".username");
  do_check_eq(username.textContent, LOGIN_FIELDS.username);

  let login_item = browser.contentDocument.querySelector("#logins-list > .login-item");
  browser.addEventListener("PasswordsDetailsLoad", function() {
    browser.removeEventListener("PasswordsDetailsLoad", this, false);
    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, false);

  // Expand item details.
  login_item.click();
});

add_test(function test_passwords_details() {
  let login_details = browser.contentDocument.getElementById("login-details");

  let hostname = login_details.querySelector(".hostname");
  do_check_eq(hostname.textContent, LOGIN_FIELDS.hostname);
  let username = login_details.querySelector(".username");
  do_check_eq(username.textContent, LOGIN_FIELDS.username);

  // Check that details page opens link to host.
  BrowserApp.deck.addEventListener("TabOpen", (tabevent) => {
    // Wait for tab to finish loading.
    let browser_target = tabevent.target;
    browser_target.addEventListener("load", () => {
      browser_target.removeEventListener("load", this, true);

      do_check_eq(BrowserApp.selectedTab.browser.currentURI.spec, LOGIN_FIELDS.hostname);
      Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
    }, true);

    BrowserApp.deck.removeEventListener("TabOpen", this, false);
  }, false);

  browser.contentDocument.getElementById("details-header").click();
});

run_next_test();

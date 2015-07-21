/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AndroidLog.jsm");

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

  // Load about:logins.
  BrowserApp = Services.wm.getMostRecentWindow("navigator:browser").BrowserApp;
  browser = BrowserApp.addTab("about:logins", { selected: true, parentId: BrowserApp.selectedTab.id }).browser;

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

  run_next_test();
});

run_next_test();

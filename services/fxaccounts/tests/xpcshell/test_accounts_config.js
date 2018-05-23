/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/FxAccounts.jsm");

add_task(async function test_non_https_remote_server_uri_with_requireHttps_false() {
  Services.prefs.setBoolPref(
    "identity.fxaccounts.allowHttp",
    true);
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.root",
    "http://example.com/");
  Assert.equal(await FxAccounts.config.promiseSignUpURI("test"),
               "http://example.com/signup?service=sync&context=null&entrypoint=test");

  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
  Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
});

add_task(async function test_non_https_remote_server_uri() {
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.root",
    "http://example.com/");
  await Assert.rejects(FxAccounts.config.promiseSignUpURI(), /Firefox Accounts server must use HTTPS/);
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

function createFakeOldPrefs() {
  const baseURL = "https://example.com/myfxa/";
  let createPref = (pref, extraPath) => {
    Services.prefs.setCharPref(pref, `${baseURL}${extraPath}?service=sync&context=fx_desktop_v3`);
  };
  createPref("identity.fxaccounts.remote.signin.uri", "signin");
  createPref("identity.fxaccounts.remote.signup.uri", "signup");
  createPref("identity.fxaccounts.remote.email.uri", "email");
  createPref("identity.fxaccounts.remote.connectdevice.uri", "connect_another_device");
  createPref("identity.fxaccounts.remote.force_auth.uri", "force_auth");
  createPref("identity.fxaccounts.settings.uri", "settings");
  createPref("identity.fxaccounts.settings.devices.uri", "settings/clients");
  Services.prefs.setCharPref("identity.fxaccounts.remote.webchannel.uri", baseURL);
}

function checkOldPrefsDeleted() {
  const migratedPrefs = [
    "identity.fxaccounts.remote.webchannel.uri",
    "identity.fxaccounts.settings.uri",
    "identity.fxaccounts.settings.devices.uri",
    "identity.fxaccounts.remote.signup.uri",
    "identity.fxaccounts.remote.signin.uri",
    "identity.fxaccounts.remote.email.uri",
    "identity.fxaccounts.remote.connectdevice.uri",
    "identity.fxaccounts.remote.force_auth.uri",
  ];
  for (const pref of migratedPrefs) {
    Assert.ok(!Services.prefs.prefHasUserValue(pref));
  }
}

add_task(async function test_migration_autoconfig() {
  createFakeOldPrefs();
  Services.prefs.setCharPref("identity.fxaccounts.autoconfig.uri",
                             "https://example.com/.well-known/fxa-client-configuration");
  sinon.stub(FxAccounts.config, "fetchConfigURLs");
  await FxAccounts.config.tryPrefsMigration();
  Assert.ok(FxAccounts.config.fetchConfigURLs.called);
  checkOldPrefsDeleted();
  FxAccounts.config.fetchConfigURLs.restore();
  Services.prefs.clearUserPref("identity.fxaccounts.autoconfig.uri");
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

add_task(async function test_migration_manual() {
  createFakeOldPrefs();
  sinon.stub(FxAccounts.config, "fetchConfigURLs");
  await FxAccounts.config.tryPrefsMigration();
  Assert.equal(Services.prefs.getCharPref("identity.fxaccounts.remote.root"),
               "https://example.com/myfxa/");
  Assert.ok(!FxAccounts.config.fetchConfigURLs.called);
  checkOldPrefsDeleted();
  FxAccounts.config.fetchConfigURLs.restore();
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

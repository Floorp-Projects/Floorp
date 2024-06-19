/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);

add_task(
  async function test_non_https_remote_server_uri_with_requireHttps_false() {
    Services.prefs.setStringPref("identity.fxaccounts.autoconfig.uri", "");
    Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
    Services.prefs.setStringPref(
      "identity.fxaccounts.remote.root",
      "http://example.com/"
    );
    Assert.equal(
      await FxAccounts.config.promiseConnectAccountURI("test"),
      "http://example.com/?context=fx_desktop_v3&entrypoint=test&action=email&service=sync"
    );

    Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  }
);

add_task(async function test_non_https_remote_server_uri() {
  Services.prefs.setStringPref(
    "identity.fxaccounts.remote.root",
    "http://example.com/"
  );
  await Assert.rejects(
    FxAccounts.config.promiseConnectAccountURI(),
    /Firefox Accounts server must use HTTPS/
  );
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

add_task(async function test_is_production_config() {
  // should start with no auto-config URL.
  Assert.ok(!FxAccounts.config.getAutoConfigURL());
  // which means we are using prod.
  Assert.ok(FxAccounts.config.isProductionConfig());

  // Set an auto-config URL.
  Services.prefs.setStringPref(
    "identity.fxaccounts.autoconfig.uri",
    "http://x"
  );
  Assert.equal(FxAccounts.config.getAutoConfigURL(), "http://x");
  Assert.ok(!FxAccounts.config.isProductionConfig());

  // Clear the auto-config URL, but set one of the other config params.
  Services.prefs.clearUserPref("identity.fxaccounts.autoconfig.uri");
  Services.prefs.setStringPref("identity.sync.tokenserver.uri", "http://t");
  Assert.ok(!FxAccounts.config.isProductionConfig());
  Services.prefs.clearUserPref("identity.sync.tokenserver.uri");
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccounts } = ChromeUtils.import(
  "resource://gre/modules/FxAccounts.jsm"
);

add_task(
  async function test_non_https_remote_server_uri_with_requireHttps_false() {
    Services.prefs.setBoolPref("identity.fxaccounts.allowHttp", true);
    Services.prefs.setCharPref(
      "identity.fxaccounts.remote.root",
      "http://example.com/"
    );
    Assert.equal(
      await FxAccounts.config.promiseSignUpURI("test"),
      "http://example.com/signup?service=sync&context=null&entrypoint=test"
    );

    Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
    Services.prefs.clearUserPref("identity.fxaccounts.allowHttp");
  }
);

add_task(async function test_non_https_remote_server_uri() {
  Services.prefs.setCharPref(
    "identity.fxaccounts.remote.root",
    "http://example.com/"
  );
  await Assert.rejects(
    FxAccounts.config.promiseSignUpURI(),
    /Firefox Accounts server must use HTTPS/
  );
  Services.prefs.clearUserPref("identity.fxaccounts.remote.root");
});

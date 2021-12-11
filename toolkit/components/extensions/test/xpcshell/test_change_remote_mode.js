/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function change_remote() {
  let remote = Services.prefs.getBoolPref("extensions.webextensions.remote");
  Assert.equal(
    WebExtensionPolicy.useRemoteWebExtensions,
    remote,
    "value of useRemoteWebExtensions matches the pref"
  );

  Services.prefs.setBoolPref("extensions.webextensions.remote", !remote);

  Assert.equal(
    WebExtensionPolicy.useRemoteWebExtensions,
    remote,
    "value of useRemoteWebExtensions is still the same after changing the pref"
  );
});

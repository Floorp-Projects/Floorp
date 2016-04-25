/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/Preferences.jsm");

const ADDON_ID = "test@web.extension";

const aps = Cc["@mozilla.org/addons/policy-service;1"]
  .getService(Ci.nsIAddonPolicyService).wrappedJSObject;

do_register_cleanup(() => {
  aps.setAddonCSP(ADDON_ID, null);
});

add_task(function* test_addon_csp() {
  equal(aps.baseCSP, Preferences.get("extensions.webextensions.base-content-security-policy"),
        "Expected base CSP value");

  equal(aps.defaultCSP, Preferences.get("extensions.webextensions.default-content-security-policy"),
        "Expected default CSP value");

  equal(aps.getAddonCSP(ADDON_ID), aps.defaultCSP,
        "CSP for unknown add-on ID should be the default CSP");


  const CUSTOM_POLICY = "script-src: 'self' https://xpcshell.test.custom.csp; object-src: 'none'";

  aps.setAddonCSP(ADDON_ID, CUSTOM_POLICY);

  equal(aps.getAddonCSP(ADDON_ID), CUSTOM_POLICY, "CSP should point to add-on's custom policy");


  aps.setAddonCSP(ADDON_ID, null);

  equal(aps.getAddonCSP(ADDON_ID), aps.defaultCSP,
        "CSP should revert to default when set to null");
});

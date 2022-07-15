/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported checkSitePermissions */

const { Services } = SpecialPowers;
const { NetUtil } = SpecialPowers.ChromeUtils.import(
  "resource://gre/modules/NetUtil.jsm"
);

function checkSitePermissions(uuid, expectedPermAction, assertMessage) {
  if (!uuid) {
    throw new Error(
      "checkSitePermissions should not be called with an undefined uuid"
    );
  }

  const baseURI = NetUtil.newURI(`moz-extension://${uuid}/`);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    baseURI,
    {}
  );

  const sitePermissions = {
    webextUnlimitedStorage: Services.perms.testPermissionFromPrincipal(
      principal,
      "WebExtensions-unlimitedStorage"
    ),
    persistentStorage: Services.perms.testPermissionFromPrincipal(
      principal,
      "persistent-storage"
    ),
  };

  for (const [sitePermissionName, actualPermAction] of Object.entries(
    sitePermissions
  )) {
    is(
      actualPermAction,
      expectedPermAction,
      `The extension "${sitePermissionName}" SitePermission ${assertMessage} as expected`
    );
  }
}

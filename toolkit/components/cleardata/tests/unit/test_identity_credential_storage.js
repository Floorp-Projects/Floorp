/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdentityCredentialStorageService",
  "@mozilla.org/browser/identity-credential-storage-service;1",
  "nsIIdentityCredentialStorageService"
);

do_get_profile();

add_task(async function test_deleteByRange() {
  Services.prefs.setBoolPref(
    "dom.security.credentialmanagement.identity.enabled",
    true
  );
  const expiry = Date.now() + 24 * 60 * 60;
  let rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://rp.com/"),
    {}
  );
  let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://idp.com/"),
    {}
  );
  const credentialID = "ID";

  // Test initial value
  let registered = {};
  let allowLogout = {};
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered initially.");
  Assert.ok(!allowLogout.value, "Should not allow logout initially.");

  // Set and read a value
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    true,
    true
  );

  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(registered.value, "Should be registered by set.");
  Assert.ok(allowLogout.value, "Should now allow logout by set.");

  let from = Date.now() + 60 * 60;
  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      from * 1000,
      expiry * 1000,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_CREDENTIAL_MANAGER_STATE,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );

  Assert.ok(
    registered.value,
    "Should be existing since the value is not deleted"
  );

  from = Date.now() - 60 * 60;

  await new Promise(aResolve => {
    Services.clearData.deleteDataInTimeRange(
      from * 1000,
      expiry * 1000,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_CREDENTIAL_MANAGER_STATE,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be existing");

  Services.prefs.clearUserPref(
    "dom.security.credentialmanagement.identity.enabled"
  );
});

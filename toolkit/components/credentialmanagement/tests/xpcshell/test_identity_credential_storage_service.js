/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "IdentityCredentialStorageService",
  "@mozilla.org/browser/identity-credential-storage-service;1",
  "nsIIdentityCredentialStorageService"
);

do_get_profile();

add_task(async function test_insert_and_delete() {
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

  IdentityCredentialStorageService.delete(
    rpPrincipal,
    idpPrincipal,
    credentialID
  );
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.clear();
});

add_task(async function test_basedomain_delete() {
  let rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://rp.com/"),
    {}
  );
  let rpPrincipal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://www.rp.com/"),
    {}
  );
  let rpPrincipal3 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://www.other.com/"),
    {}
  );
  let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://idp.com/"),
    {}
  );
  const credentialID = "ID";
  let registered = {};
  let allowLogout = {};

  // Set values
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    true,
    true
  );

  IdentityCredentialStorageService.deleteFromBaseDomain(
    rpPrincipal2.baseDomain
  );
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.getState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.getState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(registered.value, "Should be registered by set.");
  Assert.ok(allowLogout.value, "Should now allow logout by set.");
  IdentityCredentialStorageService.clear();
});

add_task(async function test_principal_delete() {
  let rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://rp.com/"),
    {}
  );
  let rpPrincipal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://www.rp.com/"),
    {}
  );
  let rpPrincipal3 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://www.other.com/"),
    {}
  );
  let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://idp.com/"),
    {}
  );
  const credentialID = "ID";
  let registered = {};
  let allowLogout = {};

  // Set values
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    true,
    true
  );

  IdentityCredentialStorageService.deleteFromPrincipal(rpPrincipal2);
  IdentityCredentialStorageService.getState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(registered.value, "Should be registered by set.");
  Assert.ok(allowLogout.value, "Should now allow logout by set.");
  IdentityCredentialStorageService.getState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.getState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(registered.value, "Should be registered by set.");
  Assert.ok(allowLogout.value, "Should now allow logout by set.");
  IdentityCredentialStorageService.clear();
});

add_task(async function test_principal_delete() {
  let rpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://rp.com/"),
    {}
  );
  let rpPrincipal2 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://rp.com/"),
    { privateBrowsingId: 1 }
  );
  let rpPrincipal3 = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://www.other.com/"),
    { privateBrowsingId: 1 }
  );
  let idpPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI("https://idp.com/"),
    {}
  );
  const credentialID = "ID";
  let registered = {};
  let allowLogout = {};

  // Set values
  IdentityCredentialStorageService.setState(
    rpPrincipal,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    true,
    true
  );
  IdentityCredentialStorageService.setState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    true,
    true
  );

  IdentityCredentialStorageService.deleteFromOriginAttributesPattern(
    '{ "privateBrowsingId": 1 }'
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
  IdentityCredentialStorageService.getState(
    rpPrincipal2,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.getState(
    rpPrincipal3,
    idpPrincipal,
    credentialID,
    registered,
    allowLogout
  );
  Assert.ok(!registered.value, "Should not be registered after deletion.");
  Assert.ok(!allowLogout.value, "Should not allow logout after deletion.");
  IdentityCredentialStorageService.clear();
});

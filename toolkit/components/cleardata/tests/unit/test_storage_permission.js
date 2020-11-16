/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for permissions
 */

"use strict";

// Test that only the storageAccessAPI gets removed.
add_task(async function test_removing_storage_permission() {
  const uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    principal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION,
    "There is a storageAccessAPI permission set"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION,
    "the storageAccessAPI permission has been removed"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(principal, "cookie"),
    Services.perms.ALLOW_ACTION,
    "the cookie permission has not been removed"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => aResolve()
    );
  });
});

// Test that the storageAccessAPI gets removed from a particular principal
add_task(async function test_removing_storage_permission_from_principal() {
  const uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  const anotherUri = Services.io.newURI("https://example.com");
  const anotherPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    anotherUri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    anotherPrincipal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION,
    "storageAccessAPI permission has been added to the first principal"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      anotherPrincipal,
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION,
    "storageAccessAPI permission has been added to the second principal"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      principal,
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION,
    "storageAccessAPI permission has been removed from the first principal"
  );
  Assert.equal(
    Services.perms.testExactPermissionFromPrincipal(
      anotherPrincipal,
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION,
    "storageAccessAPI permission has not been removed from the second principal"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => aResolve()
    );
  });
});

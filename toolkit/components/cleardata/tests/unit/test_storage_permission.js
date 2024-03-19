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
      () => aResolve()
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
  const anotherPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(anotherUri, {});

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
      () => aResolve()
    );
  });
});

// Test that the storageAccessAPI gets removed from a base domain.
add_task(async function test_removing_storage_permission_from_base_domainl() {
  const uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  const uriSub = Services.io.newURI("http://test.example.net");
  const principalSub = Services.scriptSecurityManager.createContentPrincipal(
    uriSub,
    {}
  );

  const anotherUri = Services.io.newURI("https://example.com");
  const anotherPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(anotherUri, {});

  Services.perms.addFromPrincipal(
    principal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    principalSub,
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
      principalSub,
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION,
    "storageAccessAPI permission has been added to the subdomain principal"
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
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
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
      principalSub,
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION,
    "storageAccessAPI permission has been removed from the sub domain principal"
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
      () => aResolve()
    );
  });
});

// Tests the deleteUserInteractionForClearingHistory function.
add_task(async function test_deleteUserInteractionForClearingHistory() {
  // These should be retained.
  PermissionTestUtils.add(
    "https://example.com",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://sub.example.com",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://sub.example.com^userContextId=3",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  // These should be removed.
  PermissionTestUtils.add(
    "https://example.org",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://sub.example.org",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://sub.example.org^userContextId=3",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  let principalWithStorage =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://sub.example.com"
    );

  await new Promise(resolve => {
    return Services.clearData.deleteUserInteractionForClearingHistory(
      [principalWithStorage],
      0,
      resolve
    );
  });

  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://example.org",
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://sub.example.org",
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://sub.example.org^userContextId=3",
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION
  );

  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://example.com",
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://sub.example.com",
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://sub.example.com^userContextId=3",
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION
  );

  // This permission is set earlier than the timestamp and should be retained.
  PermissionTestUtils.add(
    "https://example.net",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  // Add some time in between taking the snapshot of the timestamp
  // to avoid flakyness.
  await new Promise(c => do_timeout(100, c));
  let timestamp = Date.now();
  await new Promise(c => do_timeout(100, c));

  // This permission is set later than the timestamp and should be removed.
  PermissionTestUtils.add(
    "https://example.org",
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  await new Promise(resolve => {
    return Services.clearData.deleteUserInteractionForClearingHistory(
      [principalWithStorage],
      // ClearDataService takes PRTime (microseconds)
      timestamp * 1000,
      resolve
    );
  });

  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://example.org",
      "storageAccessAPI"
    ),
    Services.perms.UNKNOWN_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://example.net",
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.testExactPermission(
      "https://example.com",
      "storageAccessAPI"
    ),
    Services.perms.ALLOW_ACTION
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      () => aResolve()
    );
  });
});

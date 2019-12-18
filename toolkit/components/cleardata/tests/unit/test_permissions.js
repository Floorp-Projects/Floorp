/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for permissions
 */

"use strict";

add_task(async function test_all_permissions() {
  const uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Services.perms.addFromPrincipal(
    principal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) == null
  );
});

add_task(async function test_principal_permissions() {
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
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    anotherPrincipal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) != null
  );
  Assert.ok(
    Services.perms.getPermissionObject(anotherPrincipal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) == null
  );
  Assert.ok(
    Services.perms.getPermissionObject(anotherPrincipal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => aResolve()
    );
  });
});

add_task(async function test_3rdpartystorage_permissions() {
  const uri = Services.io.newURI("https://example.net");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  Services.perms.addFromPrincipal(
    principal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  const anotherUri = Services.io.newURI("https://example.com");
  const anotherPrincipal = Services.scriptSecurityManager.createContentPrincipal(
    anotherUri,
    {}
  );
  Services.perms.addFromPrincipal(
    anotherPrincipal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  Services.perms.addFromPrincipal(
    anotherPrincipal,
    "3rdPartyStorage^https://example.net",
    Services.perms.ALLOW_ACTION
  );

  const oneMoreUri = Services.io.newURI("https://example.org");
  const oneMorePrincipal = Services.scriptSecurityManager.createContentPrincipal(
    oneMoreUri,
    {}
  );
  Services.perms.addFromPrincipal(
    oneMorePrincipal,
    "cookie",
    Services.perms.ALLOW_ACTION
  );

  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) != null
  );
  Assert.ok(
    Services.perms.getPermissionObject(anotherPrincipal, "cookie", true) != null
  );
  Assert.ok(
    Services.perms.getPermissionObject(
      anotherPrincipal,
      "3rdPartyStorage^https://example.net",
      true
    ) != null
  );
  Assert.ok(
    Services.perms.getPermissionObject(oneMorePrincipal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    Services.perms.getPermissionObject(principal, "cookie", true) == null
  );
  Assert.ok(
    Services.perms.getPermissionObject(anotherPrincipal, "cookie", true) != null
  );
  Assert.ok(
    Services.perms.getPermissionObject(
      anotherPrincipal,
      "3rdPartyStorage^https://example.net",
      true
    ) == null
  );
  Assert.ok(
    Services.perms.getPermissionObject(oneMorePrincipal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => aResolve()
    );
  });
});

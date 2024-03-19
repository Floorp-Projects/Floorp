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
  const anotherPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(anotherUri, {});

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
      () => aResolve()
    );
  });
});

function addTestPermissions() {
  Services.perms.removeAll();

  PermissionTestUtils.add(
    "https://example.net",
    "geo",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "http://example.net",
    "cookie",
    Services.perms.DENY_ACTION
  );
  PermissionTestUtils.add(
    "https://bar.example.net",
    "geo",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://foo.bar.example.net",
    "geo",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://example.com",
    "3rdPartyStorage^https://example.net",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "https://example.com",
    "3rdPartyFrameStorage^https://example.net",
    Services.perms.ALLOW_ACTION
  );

  PermissionTestUtils.add(
    "https://example.com",
    "cookie",
    Services.perms.ALLOW_ACTION
  );
  PermissionTestUtils.add(
    "http://example.com",
    "geo",
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    PermissionTestUtils.getPermissionObject("https://example.net", "geo", true)
      .capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "http://example.net",
      "cookie",
      true
    ).capability,
    Services.perms.DENY_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://bar.example.net",
      "geo",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://foo.bar.example.net",
      "geo",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "3rdPartyStorage^https://example.net",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "3rdPartyFrameStorage^https://example.net",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "cookie",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject("http://example.com", "geo", true)
      .capability,
    Services.perms.ALLOW_ACTION
  );
}

add_task(async function test_basedomain_permissions() {
  for (let domain of [
    "example.net",
    "test.example.net",
    "foo.bar.example.net",
  ]) {
    addTestPermissions();

    await new Promise(aResolve => {
      Services.clearData.deleteDataFromBaseDomain(
        domain,
        true /* user request */,
        Ci.nsIClearDataService.CLEAR_PERMISSIONS,
        value => {
          Assert.equal(value, 0);
          aResolve();
        }
      );
    });

    // Should have cleared all entries associated with the base domain.
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "https://example.net",
        "geo",
        true
      )
    );
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "http://example.net",
        "cookie",
        true
      )
    );
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "https://bar.example.net",
        "geo",
        true
      )
    );
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "https://foo.bar.example.net",
        "geo",
        true
      )
    );
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "https://example.com",
        "3rdPartyStorage^https://example.net",
        true
      )
    );
    Assert.ok(
      !PermissionTestUtils.getPermissionObject(
        "https://example.com",
        "3rdPartyFrameStorage^https://example.net",
        true
      )
    );

    // Unrelated entries should still exist.
    Assert.equal(
      PermissionTestUtils.getPermissionObject(
        "https://example.com",
        "cookie",
        true
      ).capability,
      Services.perms.ALLOW_ACTION
    );
    Assert.equal(
      PermissionTestUtils.getPermissionObject("http://example.com", "geo", true)
        .capability,
      Services.perms.ALLOW_ACTION
    );
  }

  Services.perms.removeAll();
});

add_task(async function test_host_permissions() {
  addTestPermissions();

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "bar.example.net",
      true /* user request */,
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  // Should have cleared all entries associated with the host and its
  // subdomains.
  Assert.ok(
    !PermissionTestUtils.getPermissionObject(
      "https://bar.example.net",
      "geo",
      true
    )
  );
  Assert.ok(
    !PermissionTestUtils.getPermissionObject(
      "https://foo.bar.example.net",
      "geo",
      true
    )
  );

  // Unrelated entries should still exist.
  Assert.equal(
    PermissionTestUtils.getPermissionObject("https://example.net", "geo", true)
      .capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "http://example.net",
      "cookie",
      true
    ).capability,
    Services.perms.DENY_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "3rdPartyStorage^https://example.net",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "3rdPartyFrameStorage^https://example.net",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );

  Assert.equal(
    PermissionTestUtils.getPermissionObject(
      "https://example.com",
      "cookie",
      true
    ).capability,
    Services.perms.ALLOW_ACTION
  );
  Assert.equal(
    PermissionTestUtils.getPermissionObject("http://example.com", "geo", true)
      .capability,
    Services.perms.ALLOW_ACTION
  );

  Services.perms.removeAll();
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
  const anotherPrincipal =
    Services.scriptSecurityManager.createContentPrincipal(anotherUri, {});
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
  Services.perms.addFromPrincipal(
    anotherPrincipal,
    "3rdPartyFrameStorage^https://example.net",
    Services.perms.ALLOW_ACTION
  );

  const oneMoreUri = Services.io.newURI("https://example.org");
  const oneMorePrincipal =
    Services.scriptSecurityManager.createContentPrincipal(oneMoreUri, {});
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
    Services.perms.getPermissionObject(
      anotherPrincipal,
      "3rdPartyFrameStorage^https://example.net",
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
    Services.perms.getPermissionObject(
      anotherPrincipal,
      "3rdPartyFrameStorage^https://example.net",
      true
    ) == null
  );
  Assert.ok(
    Services.perms.getPermissionObject(oneMorePrincipal, "cookie", true) != null
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_PERMISSIONS,
      () => aResolve()
    );
  });
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test clearing cache.
 */

"use strict";

add_task(async function test_deleteFromHost() {
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache has an entry"
  );

  await SiteDataTestUtils.addCacheEntry("http://example.org/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.org/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache has an entry"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.com",
      true,
      Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache is cleared"
  );
  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache is cleared"
  );

  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache has an entry"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteFromPrincipal() {
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache has an entry"
  );

  await SiteDataTestUtils.addCacheEntry("http://example.org/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.org/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache has an entry"
  );

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "http://example.com/"
  );
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      principal,
      true,
      Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache is cleared"
  );
  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache is cleared"
  );

  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache has an entry"
  );

  await SiteDataTestUtils.clear();
});

add_task(async function test_deleteAll() {
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.com/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache has an entry"
  );

  await SiteDataTestUtils.addCacheEntry("http://example.org/", "disk");
  await SiteDataTestUtils.addCacheEntry("http://example.org/", "memory");
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache has an entry"
  );
  Assert.ok(
    SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache has an entry"
  );

  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
      value => {
        Assert.equal(value, 0);
        aResolve();
      }
    );
  });

  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "disk"),
    "The disk cache is cleared"
  );
  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.com/", "memory"),
    "The memory cache is cleared"
  );

  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.org/", "disk"),
    "The disk cache is cleared"
  );
  Assert.ok(
    !SiteDataTestUtils.hasCacheEntry("http://example.org/", "memory"),
    "The memory cache is cleared"
  );

  await SiteDataTestUtils.clear();
});

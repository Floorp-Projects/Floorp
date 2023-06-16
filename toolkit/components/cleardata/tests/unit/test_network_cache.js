/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test clearing cache.
 */

"use strict";

function getPartitionedLoadContextInfo(
  { scheme, topLevelBaseDomain, port },
  originAttributes = {}
) {
  return Services.loadContextInfo.custom(
    false,
    getOAWithPartitionKey(
      { scheme, topLevelBaseDomain, port },
      originAttributes
    )
  );
}

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

  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
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

add_task(async function test_deleteFromBaseDomain() {
  for (let cacheType of ["disk", "memory"]) {
    await SiteDataTestUtils.addCacheEntry("http://example.com/", cacheType);
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry("http://example.com/", cacheType),
      `The ${cacheType} cache has an entry.`
    );

    await SiteDataTestUtils.addCacheEntry("http://example.org/", cacheType);
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry("http://example.org/", cacheType),
      `The ${cacheType} cache has an entry.`
    );

    // Partitioned cache.
    await SiteDataTestUtils.addCacheEntry(
      "http://example.com/",
      cacheType,
      getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.org" })
    );
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry(
        "http://example.com/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.org" })
      ),
      `The ${cacheType} cache has a partitioned entry`
    );
    await SiteDataTestUtils.addCacheEntry(
      "http://example.org/",
      cacheType,
      getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.com" })
    );
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry(
        "http://example.org/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.com" })
      ),
      `The ${cacheType} cache has a partitioned entry`
    );

    // Clear an unrelated base domain.
    await new Promise(aResolve => {
      Services.clearData.deleteDataFromBaseDomain(
        "foo.com",
        true,
        Ci.nsIClearDataService.CLEAR_NETWORK_CACHE,
        value => {
          Assert.equal(value, 0);
          aResolve();
        }
      );
    });

    // Should still have all cache entries.
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry("http://example.com/", cacheType),
      `The ${cacheType} cache has an entry.`
    );
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry("http://example.org/", cacheType),
      `The ${cacheType} cache has an entry.`
    );
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry(
        "http://example.com/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.org" })
      ),
      `The ${cacheType} cache has a partitioned entry`
    );
    Assert.ok(
      SiteDataTestUtils.hasCacheEntry(
        "http://example.org/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.com" })
      ),
      `The ${cacheType} cache has a partitioned entry`
    );

    // Clear data for example.com
    await new Promise(aResolve => {
      Services.clearData.deleteDataFromBaseDomain(
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
      !SiteDataTestUtils.hasCacheEntry("http://example.com/", cacheType),
      `The ${cacheType} cache is cleared.`
    );

    Assert.ok(
      SiteDataTestUtils.hasCacheEntry("http://example.org/", cacheType),
      `The ${cacheType} cache has an entry.`
    );

    Assert.ok(
      !SiteDataTestUtils.hasCacheEntry(
        "http://example.com/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.org" })
      ),
      `The ${cacheType} cache is cleared.`
    );

    Assert.ok(
      !SiteDataTestUtils.hasCacheEntry(
        "http://example.org/",
        cacheType,
        getPartitionedLoadContextInfo({ topLevelBaseDomain: "example.com" })
      ),
      `The ${cacheType} cache is cleared.`
    );
    await SiteDataTestUtils.clear();
  }
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

/* import-globals-from partitionedstorage_head.js */

const APS_PREF =
  "privacy.partition.always_partition_third_party_non_cookie_storage";

PartitionedStorageHelper.runTest(
  "DOMCache",
  async (win3rdParty, win1stParty, allowed) => {
    await win3rdParty.caches.open("wow").then(
      _ => {
        ok(allowed, "DOM Cache cannot be used!");
      },
      _ => {
        ok(!allowed, "DOM Cache cannot be used!");
      }
    );

    await win1stParty.caches.open("wow").then(
      _ => {
        ok(true, "DOM Cache should be available");
      },
      _ => {
        ok(false, "DOM Cache should be available");
      }
    );
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    ["dom.caches.testing.enabled", true],
    [APS_PREF, false],
  ]
);

PartitionedStorageHelper.runTest(
  "DOMCache",
  async (win3rdParty, win1stParty, allowed) => {
    await win1stParty.caches.open("wow").then(
      async cache => {
        ok(true, "DOM Cache should be available");
        await cache.add("/");
      },
      _ => {
        ok(false, "DOM Cache should be available");
      }
    );

    await win3rdParty.caches.open("wow").then(
      async cache => {
        ok(true, "DOM Cache can be used!");
        is(undefined, await cache.match("/"), "DOM Cache is partitioned");
      },
      _ => {
        ok(false, "DOM Cache cannot be used!");
      }
    );
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },

  [
    ["dom.caches.testing.enabled", true],
    [APS_PREF, true],
  ]
);

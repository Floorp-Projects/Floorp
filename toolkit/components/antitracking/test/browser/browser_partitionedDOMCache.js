/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTest(
  "DOMCache",
  async (win3rdParty, win1stParty, allowed) => {
    // DOM Cache is not supported. Always blocked.
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

  [["dom.caches.testing.enabled", true]]
);

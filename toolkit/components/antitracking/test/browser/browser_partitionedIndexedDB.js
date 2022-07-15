/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTest(
  "IndexedDB",
  async (win3rdParty, win1stParty, allowed) => {
    await new Promise(resolve => {
      let a = win1stParty.indexedDB.open("test", 1);
      ok(!!a, "IDB should not be blocked in 1st party contexts");

      a.onsuccess = e => {
        let db = e.target.result;
        is(db.objectStoreNames.length, 1, "We have 1 objectStore");
        is(db.objectStoreNames[0], "foobar", "We have 'foobar' objectStore");
        resolve();
      };

      a.onupgradeneeded = e => {
        let db = e.target.result;
        is(db.objectStoreNames.length, 0, "We have 0 objectStores");
        db.createObjectStore("foobar", { keyPath: "test" });
      };
    });

    await new Promise(resolve => {
      let a = win3rdParty.indexedDB.open("test", 1);
      ok(!!a, "IDB should not be blocked in 3rd party contexts");

      a.onsuccess = e => {
        let db = e.target.result;

        is(db.objectStoreNames.length, 0, "We have 0 objectStore");
        resolve();
      };
    });
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

PartitionedStorageHelper.runPartitioningTest(
  "Partitioned tabs - IndexedDB",
  "indexeddb",

  // getDataCallback
  async win => {
    return new Promise(resolve => {
      let a = win.indexedDB.open("test", 1);

      a.onupgradeneeded = e => {
        let db = e.target.result;
        db.createObjectStore("foobar", { keyPath: "id" });
      };

      a.onsuccess = e => {
        let db = e.target.result;
        db
          .transaction("foobar")
          .objectStore("foobar")
          .get(1).onsuccess = ee => {
          resolve(ee.target.result === undefined ? "" : ee.target.result.value);
        };
      };
    });
  },

  // addDataCallback
  async (win, value) => {
    return new Promise(resolve => {
      let a = win.indexedDB.open("test", 1);

      a.onsuccess = e => {
        let db = e.target.result;
        db
          .transaction("foobar", "readwrite")
          .objectStore("foobar")
          .put({ id: 1, value }).onsuccess = _ => {
          resolve(true);
        };
      };
    });
  },

  // cleanup
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

/* import-globals-from storageprincipal_head.js */

StoragePrincipalHelper.runTest("IndexedDB",
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

        if (allowed) {
          is(db.objectStoreNames.length, 1, "We have 1 objectStore");
          is(db.objectStoreNames[0], "foobar", "We have 'foobar' objectStore");
        } else {
          is(db.objectStoreNames.length, 0, "We have 0 objectStore");
        }
        resolve();
      };
    });
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });


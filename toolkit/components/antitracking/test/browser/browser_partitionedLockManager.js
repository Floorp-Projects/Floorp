/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTest(
  "LockManager works in both first and third party contexts",
  async (win3rdParty, win1stParty, allowed) => {
    let locks = [];
    ok(win1stParty.isSecureContext, "1st party is in a secure context");
    ok(win3rdParty.isSecureContext, "3rd party is in a secure context");
    await win1stParty.navigator.locks.request("foo", lock => {
      locks.push(lock);
      ok(true, "locks.request succeeded for 1st party");
    });

    await win3rdParty.navigator.locks.request("foo", lock => {
      locks.push(lock);
      ok(true, "locks.request succeeded for 3rd party");
    });

    is(locks.length, 2, "We should have granted 2 lock requests at this point");
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  /* extraPrefs */ undefined,
  { runInSecureContext: true }
);

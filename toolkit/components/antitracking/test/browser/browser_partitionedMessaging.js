/* import-globals-from storageprincipal_head.js */

StoragePrincipalHelper.runTest("BroadcastChannel",
  async (win3rdParty, win1stParty, allowed) => {
    let a = new win3rdParty.BroadcastChannel("hello");
    ok(!!a, "BroadcastChannel should be created by 3rd party iframe");

    let b = new win1stParty.BroadcastChannel("hello");
    ok(!!b, "BroadcastChannel should be created by 1st party iframe");

    // BroadcastChannel uses the incument global, this means that its CTOR will
    // always use the 3rd party iframe's window as global.
  },
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

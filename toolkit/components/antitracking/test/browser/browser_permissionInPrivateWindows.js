/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "Test whether we receive any persistent permissions in private windows",
  // Blocking callback
  async _ => {
    // Nothing to do here!
  },

  // Non blocking callback
  async _ => {
    try {
      let Services = SpecialPowers.Services;
      // We would use TEST_3RD_PARTY_DOMAIN here, except that the variable isn't
      // accessible in the context of the web page...
      let principal = SpecialPowers.wrap(document).nodePrincipal;
      for (let perm of Services.perms.getAllForPrincipal(principal)) {
        // Ignore permissions other than storage access
        if (!perm.type.startsWith("3rdPartyStorage^")) {
          continue;
        }
        is(
          perm.expireType,
          Services.perms.EXPIRE_SESSION,
          "Permission must expire at the end of session"
        );
        is(perm.expireTime, 0, "Permission must have no expiry time");
      }
    } catch (e) {
      alert(e);
    }
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  [
    [
      "privacy.partition.always_partition_third_party_non_cookie_storage",
      false,
    ],
  ], // extra prefs
  true, // run the window.open() test
  true, // run the user interaction test
  0, // don't expect blocking notifications
  true
); // run in private windows

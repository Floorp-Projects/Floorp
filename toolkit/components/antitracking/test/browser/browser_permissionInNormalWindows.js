/* import-globals-from antitracking_head.js */

AntiTracking.runTest(
  "Test whether we receive any persistent permissions in normal windows",
  // Blocking callback
  async _ => {
    // Nothing to do here!
  },

  // Non blocking callback
  async _ => {
    try {
      // We load the test script in the parent process to check permissions.
      let chromeScript = SpecialPowers.loadChromeScript(_ => {
        // eslint-disable-next-line no-undef
        addMessageListener("go", _ => {
          const { Services } = ChromeUtils.import(
            "resource://gre/modules/Services.jsm"
          );

          function ok(what, msg) {
            // eslint-disable-next-line no-undef
            sendAsyncMessage("ok", { what: !!what, msg });
          }

          function is(a, b, msg) {
            ok(a === b, msg);
          }

          // We should use the principal of the TEST_DOMAIN since the storage
          // permission is saved under it.
          let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
            "http://example.net/"
          );

          for (let perm of Services.perms.getAllForPrincipal(principal)) {
            // Ignore permissions other than storage access
            if (!perm.type.startsWith("3rdPartyStorage^")) {
              continue;
            }
            is(
              perm.expireType,
              Services.perms.EXPIRE_TIME,
              "Permission must expire at a specific time"
            );
            ok(perm.expireTime > 0, "Permission must have a expiry time");
          }

          // eslint-disable-next-line no-undef
          sendAsyncMessage("done");
        });
      });

      chromeScript.addMessageListener("ok", obj => {
        ok(obj.what, obj.msg);
      });

      await new Promise(resolve => {
        chromeScript.addMessageListener("done", _ => {
          chromeScript.destroy();
          resolve();
        });

        chromeScript.sendAsyncMessage("go");
      });

      // We check the permission in tracking processes for non-Fission mode. In
      // Fission mode, the permission won't be synced to the tracking process,
      // so we don't check it.
      if (!SpecialPowers.useRemoteSubframes) {
        let Services = SpecialPowers.Services;
        let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          "http://example.net/"
        );

        for (let perm of Services.perms.getAllForPrincipal(principal)) {
          // Ignore permissions other than storage access
          if (!perm.type.startsWith("3rdPartyStorage^")) {
            continue;
          }
          is(
            perm.expireType,
            Services.perms.EXPIRE_TIME,
            "Permission must expire at a specific time"
          );
          ok(perm.expireTime > 0, "Permission must have a expiry time");
        }
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
  null, // no extra prefs
  true, // run the window.open() test
  true, // run the user interaction test
  0, // don't expect blocking notifications
  false
); // run in normal windows

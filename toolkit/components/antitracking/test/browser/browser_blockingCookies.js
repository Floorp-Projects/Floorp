requestLongerTimeout(4);

// Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
});

AntiTracking.runTestInNormalAndPrivateMode(
  "Set/Get Cookies",
  // Blocking callback
  async _ => {
    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    for (let arg of ["?checkonly", "?redirect-checkonly"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
      // Let's do it twice.
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
    }

    is(document.cookie, "", "Still no cookies for me");
  },

  // Non blocking callback
  async _ => {
    is(document.cookie, "", "No cookies for me");

    // Note: The ?redirect test is _not_ using checkonly, so it will actually
    // set our foopy=1 cookie.
    for (let arg of ["?checkonly", "?redirect"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
    }

    document.cookie = "name=value";
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");

    for (let arg of ["", "?redirect"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-present", "We should have cookies");
        });
    }

    ok(document.cookie.length, "Some Cookies for me");
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

AntiTracking.runTestInNormalAndPrivateMode(
  "Cookies and Storage Access API",
  // Blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    for (let arg of ["", "?redirect"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
      // Let's do it twice.
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
    }

    is(document.cookie, "", "Still no cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";

    let effectiveCookieBehavior = SpecialPowers.isContentWindowPrivate(window)
      ? SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior.pbmode"
        )
      : SpecialPowers.Services.prefs.getIntPref(
          "network.cookie.cookieBehavior"
        );

    if (
      [
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT,
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
      ].includes(effectiveCookieBehavior)
    ) {
      is(document.cookie, "", "No cookies for me");
    } else {
      is(document.cookie, "name=value", "I have the cookies!");
    }
  },

  // Non blocking callback
  async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await hasStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");

    // Note: The ?redirect test is _not_ using checkonly, so it will actually
    // set our foopy=1 cookie.
    for (let arg of ["?checkonly", "?redirect"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-not-present", "We should not have cookies");
        });
    }

    document.cookie = "name=value";
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");

    for (let arg of ["", "?redirect"]) {
      info(`checking with arg=${arg}`);
      await fetch("server.sjs" + arg)
        .then(r => r.text())
        .then(text => {
          is(text, "cookie-present", "We should have cookies");
        });
    }

    ok(document.cookie.length, "Some Cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    // For non-tracking windows, calling the API is a no-op
    ok(document.cookie.length, "Still some Cookies for me");
    ok(document.cookie.includes("name=value"), "Some cookies for me");
    ok(document.cookie.includes("foopy=1"), "Some cookies for me");
  },

  // Cleanup callback
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  },
  null,
  false,
  false
);

AntiTracking._createTask({
  name: "Block cookies with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when preference is enabled",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value";
    is(document.cookie, "", "Document cookie is blocked");
    await fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have HTTP cookies");
      });
    await fetch("server.sjs?checkonly")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          "cookie-not-present",
          "We should still not have HTTP cookies after setting them via HTTP"
        );
      });
    is(
      document.cookie,
      "",
      "Document cookie is still blocked after setting via HTTP"
    );
  },
  extraPrefs: [["network.cookie.cookieBehavior.optInPartitioning", true]],
  thirdPartyPage: TEST_4TH_PARTY_PAGE,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

AntiTracking._createTask({
  name: "Block cookies with BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN when preference is enabled in pbmode",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value";
    is(document.cookie, "", "Document cookie is blocked");
    await fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have HTTP cookies");
      });
    await fetch("server.sjs?checkonly")
      .then(r => r.text())
      .then(text => {
        is(
          text,
          "cookie-not-present",
          "We should still not have HTTP cookies after setting them via HTTP"
        );
      });
    is(
      document.cookie,
      "",
      "Document cookie is still blocked after setting via HTTP"
    );
  },
  extraPrefs: [["network.cookie.cookieBehavior.optInPartitioning", true]],
  thirdPartyPage: TEST_4TH_PARTY_PAGE,
  runInPrivateWindow: true,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
});

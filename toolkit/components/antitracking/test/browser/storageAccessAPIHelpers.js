async function hasStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(hasAccess, "Has storage access");
}

async function noStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(!hasAccess, "Doesn't yet have storage access");
}

async function callRequestStorageAccess(callback) {
  let dwu = SpecialPowers.getDOMWindowUtils(window);
  let helper = dwu.setHandlingUserInput(true);

  let success = true;
  // We only grant storage exceptions when the reject tracker behavior is enabled.
  let rejectTrackers = SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior") ==
                         SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER &&
                       SpecialPowers.Services.prefs.getBoolPref("browser.contentblocking.enabled") &&
                       !isOnContentBlockingAllowList();
  if (rejectTrackers) {
    let p;
    let threw = false;
    try {
      p = document.requestStorageAccess();
    } catch (e) {
      threw = true;
    } finally {
      helper.destruct();
    }
    ok(!threw, "requestStorageAccess should not throw");
    try {
      if (callback) {
        await p.then(_ => callback(dwu));
      } else {
        await p;
      }
    } catch (e) {
      success = false;
    }
    ok(!success, "Should not have worked without user interaction");

    await noStorageAccessInitially();

    await interactWithTracker();

    helper = dwu.setHandlingUserInput(true);
  }
  if (SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior") ==
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_ACCEPT &&
      !isOnContentBlockingAllowList()) {
    try {
      if (callback) {
        await document.requestStorageAccess().then(_ => callback(dwu));
      } else {
        await document.requestStorageAccess();
      }
    } catch (e) {
      success = false;
    } finally {
      helper.destruct();
    }
    ok(success, "Should not have thrown");

    await noStorageAccessInitially();

    await interactWithTracker();

    helper = dwu.setHandlingUserInput(true);
  }

  let p;
  let threw = false;
  try {
    p = document.requestStorageAccess();
  } catch (e) {
    threw = true;
  } finally {
    helper.destruct();
  }
  let rejected = false;
  try {
    if (callback) {
      await p.then(_ => callback(dwu));
    } else {
      await p;
    }
  } catch (e) {
    rejected = true;
  }

  success = rejectTrackers && !threw && !rejected;
  let hasAccess = await document.hasStorageAccess();
  is(hasAccess, success,
     "Should " + (success ? "" : "not ") + "have storage access now");
  if (success) {
    // Wait until the permission is visible in our process to avoid race
    // conditions.
    await waitUntilPermission("http://example.net/browser/toolkit/components/antitracking/test/browser/page.html",
                              "3rdPartyStorage^https://tracking.example.org");
  }

  return [threw, rejected];
}

async function waitUntilPermission(url, name) {
  await new Promise(resolve => {
    let id = setInterval(_ => {
      let Services = SpecialPowers.Services;
      let uri = Services.io.newURI(url);
      if (Services.perms.testPermission(uri, name) ==
            Services.perms.ALLOW_ACTION) {
        clearInterval(id);
        resolve();
      }
    }, 0);
  });
}

async function interactWithTracker() {
  await new Promise(resolve => {
    onmessage = resolve;

    info("Let's interact with the tracker");
    window.open("https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/3rdPartyOpenUI.html?messageme");
  });

  // Wait until the user interaction permission becomes visible in our process
  await waitUntilPermission("https://tracking.example.org",
                            "storageAccessAPI");
}

function isOnContentBlockingAllowList() {
  let prefs = ["browser.contentblocking.allowlist.storage.enabled",
               "browser.contentblocking.allowlist.annotations.enabled"];
  function allEnabled(prev, pref) {
    return pref &&
           SpecialPowers.Services.prefs.getBoolPref(pref);
  }
  if (!prefs.reduce(allEnabled)) {
    return false;
  }

  let url = new URL(SpecialPowers.wrap(top).location.href);
  let origin = SpecialPowers.Services.io.newURI("https://" + url.host);
  let types = ["trackingprotection", "trackingprotection-pb"];
  return types.some(type => {
    return SpecialPowers.Services.perms.testPermission(origin, type) ==
             SpecialPowers.Services.perms.ALLOW_ACTION;
  });
}


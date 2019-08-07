/* global allowListed */

async function hasStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(hasAccess, "Has storage access");
}

async function noStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(!hasAccess, "Doesn't yet have storage access");
}

async function callRequestStorageAccess(callback, expectFail) {
  let dwu = SpecialPowers.getDOMWindowUtils(window);
  let helper = dwu.setHandlingUserInput(true);

  let origin = new URL(location.href).origin;

  let success = true;
  // We only grant storage exceptions when the reject tracker behavior is enabled.
  let rejectTrackers =
    [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      SpecialPowers.Ci.nsICookieService
        .BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    ].includes(
      SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior")
    ) && !isOnContentBlockingAllowList();
  // With another-tracking.example.net, we're same-eTLD+1, so the first try succeeds.
  if (origin != "https://another-tracking.example.net") {
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
          if (expectFail) {
            await p.catch(_ => callback(dwu));
            success = false;
          } else {
            await p.then(_ => callback(dwu));
          }
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
    if (
      SpecialPowers.Services.prefs.getIntPref(
        "network.cookie.cookieBehavior"
      ) == SpecialPowers.Ci.nsICookieService.BEHAVIOR_ACCEPT &&
      !isOnContentBlockingAllowList()
    ) {
      try {
        if (callback) {
          if (expectFail) {
            await document.requestStorageAccess().catch(_ => callback(dwu));
            success = false;
          } else {
            await document.requestStorageAccess().then(_ => callback(dwu));
          }
        } else {
          await document.requestStorageAccess();
        }
      } catch (e) {
        success = false;
      } finally {
        helper.destruct();
      }
      ok(success, "Should not have thrown");

      await hasStorageAccessInitially();

      await interactWithTracker();

      helper = dwu.setHandlingUserInput(true);
    }
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
      if (expectFail) {
        await p.catch(_ => callback(dwu));
        rejected = true;
      } else {
        await p.then(_ => callback(dwu));
      }
    } else {
      await p;
    }
  } catch (e) {
    rejected = true;
  }

  success = !threw && !rejected;
  let hasAccess = await document.hasStorageAccess();
  is(
    hasAccess,
    success,
    "Should " + (success ? "" : "not ") + "have storage access now"
  );
  if (
    success &&
    rejectTrackers &&
    window.location.search != "?disableWaitUntilPermission" &&
    origin != "https://another-tracking.example.net"
  ) {
    // Wait until the permission is visible in our process to avoid race
    // conditions.
    await waitUntilPermission(
      "http://example.net/browser/toolkit/components/antitracking/test/browser/page.html",
      "3rdPartyStorage^" + window.origin
    );
  }

  return [threw, rejected];
}

async function waitUntilPermission(url, name) {
  await new Promise(resolve => {
    let id = setInterval(_ => {
      let Services = SpecialPowers.Services;
      let uri = Services.io.newURI(url);
      if (
        Services.perms.testPermission(uri, name) == Services.perms.ALLOW_ACTION
      ) {
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
    window.open(
      "/browser/toolkit/components/antitracking/test/browser/3rdPartyOpenUI.html?messageme"
    );
  });

  // Wait until the user interaction permission becomes visible in our process
  await waitUntilPermission(window.origin, "storageAccessAPI");
}

function isOnContentBlockingAllowList() {
  let url = new URL(SpecialPowers.wrap(top).location.href);
  let origin = SpecialPowers.Services.io.newURI("https://" + url.host);
  let types = ["trackingprotection", "trackingprotection-pb"];
  return types.some(type => {
    return (
      SpecialPowers.Services.perms.testPermission(origin, type) ==
      SpecialPowers.Services.perms.ALLOW_ACTION
    );
  });
}

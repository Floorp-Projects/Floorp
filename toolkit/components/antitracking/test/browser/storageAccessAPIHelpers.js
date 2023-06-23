/* global allowListed */

async function hasStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(hasAccess, "Has storage access");
}

async function noStorageAccessInitially() {
  let hasAccess = await document.hasStorageAccess();
  ok(!hasAccess, "Doesn't yet have storage access");
}

async function stillNoStorageAccess() {
  let hasAccess = await document.hasStorageAccess();
  ok(!hasAccess, "Still doesn't have storage access");
}

async function callRequestStorageAccess(callback, expectFail) {
  SpecialPowers.wrap(document).notifyUserGestureActivation();

  let origin = new URL(location.href).origin;

  let effectiveCookieBehavior = SpecialPowers.isContentWindowPrivate(window)
    ? SpecialPowers.Services.prefs.getIntPref(
        "network.cookie.cookieBehavior.pbmode"
      )
    : SpecialPowers.Services.prefs.getIntPref("network.cookie.cookieBehavior");

  let success = true;
  // We only grant storage exceptions when the reject tracker behavior is enabled.
  let rejectTrackers =
    [
      SpecialPowers.Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      SpecialPowers.Ci.nsICookieService
        .BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    ].includes(effectiveCookieBehavior) && !isOnContentBlockingAllowList();
  const TEST_ANOTHER_3RD_PARTY_ORIGIN = SpecialPowers.useRemoteSubframes
    ? "http://another-tracking.example.net"
    : "https://another-tracking.example.net";
  // With another-tracking.example.net, we're same-eTLD+1, so the first try succeeds.
  if (origin != TEST_ANOTHER_3RD_PARTY_ORIGIN) {
    if (rejectTrackers) {
      let p;
      let threw = false;
      try {
        p = document.requestStorageAccess();
      } catch (e) {
        threw = true;
      }
      ok(!threw, "requestStorageAccess should not throw");
      try {
        if (callback) {
          if (expectFail) {
            await p.catch(_ => callback());
            success = false;
          } else {
            await p.then(_ => callback());
          }
        } else {
          await p;
        }
      } catch (e) {
        success = false;
      } finally {
        SpecialPowers.wrap(document).clearUserGestureActivation();
      }
      ok(!success, "Should not have worked without user interaction");

      await noStorageAccessInitially();

      await interactWithTracker();

      SpecialPowers.wrap(document).notifyUserGestureActivation();
    }
    if (
      effectiveCookieBehavior ==
        SpecialPowers.Ci.nsICookieService.BEHAVIOR_ACCEPT &&
      !isOnContentBlockingAllowList()
    ) {
      try {
        if (callback) {
          if (expectFail) {
            await document.requestStorageAccess().catch(_ => callback());
            success = false;
          } else {
            await document.requestStorageAccess().then(_ => callback());
          }
        } else {
          await document.requestStorageAccess();
        }
      } catch (e) {
        success = false;
      } finally {
        SpecialPowers.wrap(document).clearUserGestureActivation();
      }
      ok(success, "Should not have thrown");

      await hasStorageAccessInitially();

      await interactWithTracker();

      SpecialPowers.wrap(document).notifyUserGestureActivation();
    }
  }

  let p;
  let threw = false;
  try {
    p = document.requestStorageAccess();
  } catch (e) {
    threw = true;
  }
  let rejected = false;
  try {
    if (callback) {
      if (expectFail) {
        await p.catch(_ => callback());
        rejected = true;
      } else {
        await p.then(_ => callback());
      }
    } else {
      await p;
    }
  } catch (e) {
    rejected = true;
  } finally {
    SpecialPowers.wrap(document).clearUserGestureActivation();
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
    origin != TEST_ANOTHER_3RD_PARTY_ORIGIN
  ) {
    let protocol = isSecureContext ? "https" : "http";
    // Wait until the permission is visible in parent process to avoid race
    // conditions. We don't need to wait the permission to be visible in content
    // processes since the content process doesn't rely on the permission to
    // know the storage access is updated.
    let originURI = SpecialPowers.Services.io.newURI(window.origin);
    let site = SpecialPowers.Services.eTLD.getSite(originURI);
    await waitUntilPermission(
      `${protocol}://example.net/browser/toolkit/components/antitracking/test/browser/page.html`,
      "3rdPartyFrameStorage^" + site
    );
  }

  return [threw, rejected];
}

async function waitUntilPermission(
  url,
  name,
  value = SpecialPowers.Services.perms.ALLOW_ACTION
) {
  let originAttributes = SpecialPowers.isContentWindowPrivate(window)
    ? { privateBrowsingId: 1 }
    : {};
  await new Promise(resolve => {
    let id = setInterval(async _ => {
      if (
        await SpecialPowers.testPermission(name, value, {
          url,
          originAttributes,
        })
      ) {
        clearInterval(id);
        resolve();
      }
    }, 0);
  });
}

async function interactWithTracker() {
  await new Promise(resolve => {
    let orionmessage = onmessage;
    onmessage = _ => {
      onmessage = orionmessage;
      resolve();
    };

    info("Let's interact with the tracker");
    window.open(
      "/browser/toolkit/components/antitracking/test/browser/3rdPartyOpenUI.html?messageme"
    );
  });

  // Wait until the user interaction permission becomes visible in our process
  await waitUntilPermission(window.origin, "storageAccessAPI");
}

function isOnContentBlockingAllowList() {
  // We directly check the window.allowListed here instead of checking the
  // permission. The allow list permission might not be available since it is
  // not in the preload list.

  return window.allowListed;
}

async function registerServiceWorker(win, url) {
  let reg = await win.navigator.serviceWorker.register(url);
  if (reg.installing.state !== "activated") {
    await new Promise(resolve => {
      let w = reg.installing;
      w.addEventListener("statechange", function onStateChange() {
        if (w.state === "activated") {
          w.removeEventListener("statechange", onStateChange);
          resolve();
        }
      });
    });
  }

  return reg.active;
}

function sendAndWaitWorkerMessage(target, worker, message) {
  return new Promise(resolve => {
    worker.addEventListener(
      "message",
      msg => {
        resolve(msg.data);
      },
      { once: true }
    );

    target.postMessage(message);
  });
}

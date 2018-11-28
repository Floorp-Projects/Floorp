ChromeUtils.import("resource://gre/modules/Services.jsm");

const CHROME_BASE = "chrome://mochitests/content/browser/browser/modules/test/browser/";
Services.scriptloader.loadSubScript(CHROME_BASE + "head.js", this);
/* import-globals-from ../../../../../browser/modules/test/browser/head.js */

async function testDoorHanger(choice) {
  info(`Running doorhanger test with choice #${choice}`);

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({"set": [
    ["browser.contentblocking.allowlist.annotations.enabled", true],
    ["browser.contentblocking.allowlist.storage.enabled", true],
    [ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS],
    ["dom.storage_access.auto_grants", false],
    ["dom.storage_access.enabled", true],
    ["dom.storage_access.prompt.testing", false],
    ["network.cookie.cookieBehavior", Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
    ["privacy.trackingprotection.enabled", false],
    ["privacy.trackingprotection.pbmode.enabled", false],
    ["privacy.trackingprotection.annotate_channels", true],
    ["privacy.restrict3rdpartystorage.userInteractionRequiredForHosts", "tracking.example.com,tracking.example.org"],
  ]});

  await UrlClassifierTestUtils.addTestTrackers();

  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  async function runChecks() {
    await new Promise(resolve => {
      addEventListener("message", function onMessage(e) {
        if (e.data.startsWith("choice:")) {
          window.choice = e.data.split(":")[1];
          removeEventListener("message", onMessage);
          resolve();
        }
      }, false);
      parent.postMessage("getchoice", "*");
    });

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });
    // Let's do it twice.
    await fetch("server.sjs").then(r => r.text()).then(text => {
      is(text, "cookie-not-present", "We should not have cookies");
    });

    is(document.cookie, "", "Still no cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (choice == 0) {
      // We've said no, so cookies are still blocked
      is(document.cookie, "", "Still no cookies for me");
      document.cookie = "name=value";
      is(document.cookie, "", "No cookies for me");
    } else {
      // We've said yes, so cookies are allowed now
      is(document.cookie, "", "No cookies for me");
      document.cookie = "name=value";
      is(document.cookie, "name=value", "I have the cookies!");
    }
  }

  let shownPromise =
    BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown");
  shownPromise.then(async _ => {
    let notification =
      PopupNotifications.getNotification("storage-access", browser);
    Assert.ok(notification, "Should have gotten the notification");

    let permChanged = TestUtils.topicObserved("perm-changed",
      (subject, data) => {
        let result;
        if (choice == 1) {
          result = subject &&
                   subject.QueryInterface(Ci.nsIPermission)
                          .type.startsWith("3rdPartyStorage^") &&
                   subject.principal.origin == "http://example.net" &&
                   data == "added";
        } else if (choice == 2) {
          result = subject &&
                   subject.QueryInterface(Ci.nsIPermission)
                          .type == "cookie" &&
                   subject.principal.origin == "https://tracking.example.org" &&
                   data == "added";
        }
        return result;
      });
    if (choice == 0) {
      await clickMainAction();
    } else if (choice == 1) {
      await clickSecondaryAction(choice - 1);
    } else if (choice == 2) {
      await clickSecondaryAction(choice - 1);
    }
    if (choice != 0) {
      await permChanged;
    }
  });

  let url;
  if (choice == 2) {
    url = TEST_3RD_PARTY_PAGE + "?disableWaitUntilPermission";
  } else {
    url = TEST_3RD_PARTY_PAGE;
  }
  let ct = ContentTask.spawn(browser,
                             { page: url,
                               callback: runChecks.toString(),
                               choice,
                             },
                             async function(obj) {
    await new content.Promise(resolve => {
      let ifr = content.document.createElement("iframe");
      ifr.onload = function() {
        info("Sending code to the 3rd party content");
        ifr.contentWindow.postMessage(obj.callback, "*");
      };

      content.addEventListener("message", function msg(event) {
        if (event.data.type == "finish") {
          content.removeEventListener("message", msg);
          resolve();
          return;
        }

        if (event.data.type == "ok") {
          ok(event.data.what, event.data.msg);
          return;
        }

        if (event.data.type == "info") {
          info(event.data.msg);
          return;
        }

        if (event.data == "getchoice") {
          ifr.contentWindow.postMessage("choice:" + obj.choice, "*");
          return;
        }

        ok(false, "Unknown message");
      });

      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
    });
  });
  await Promise.all([ct, shownPromise]);

  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
}

async function cleanUp() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
}

async function runRound(n) {
  await testDoorHanger(n);
  await cleanUp();
}

add_task(async function() {
  await runRound(0);
  await runRound(1);
  await runRound(2);
});

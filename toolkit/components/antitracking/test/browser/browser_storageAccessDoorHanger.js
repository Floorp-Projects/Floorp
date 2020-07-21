/* eslint-disable mozilla/no-arbitrary-setTimeout */
const CHROME_BASE =
  "chrome://mochitests/content/browser/browser/modules/test/browser/";
Services.scriptloader.loadSubScript(CHROME_BASE + "head.js", this);
/* import-globals-from ../../../../../browser/modules/test/browser/head.js */

const BLOCK = 0;
const ALLOW = 1;

async function testDoorHanger(
  choice,
  showPrompt,
  useEscape,
  topPage,
  maxConcurrent
) {
  info(
    `Running doorhanger test with choice #${choice}, showPrompt: ${showPrompt} and ` +
      `useEscape: ${useEscape}, topPage: ${topPage}, maxConcurrent: ${maxConcurrent}`
  );

  if (!showPrompt) {
    is(choice, ALLOW, "When not showing a prompt, we can only auto-grant");
    ok(
      !useEscape,
      "When not showing a prompt, we should not be trying to use the Esc key"
    );
  }

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      ["dom.storage_access.auto_grants.delayed", false],
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.max_concurrent_auto_grants", maxConcurrent],
      ["dom.storage_access.prompt.testing", false],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
      [
        "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
        "tracking.example.com,tracking.example.org",
      ],
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  let tab = BrowserTestUtils.addTab(gBrowser, topPage);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  async function runChecks() {
    // We need to repeat this constant here since runChecks is stringified
    // and sent to the content process.
    const BLOCK = 0;

    await new Promise(resolve => {
      addEventListener(
        "message",
        function onMessage(e) {
          if (e.data.startsWith("choice:")) {
            window.choice = e.data.split(":")[1];
            window.useEscape = e.data.split(":")[3];
            removeEventListener("message", onMessage);
            resolve();
          }
        },
        false
      );
      parent.postMessage("getchoice", "*");
    });

    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    is(document.cookie, "", "No cookies for me");
    document.cookie = "name=value";
    is(document.cookie, "", "No cookies for me");

    await fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have cookies");
      });
    // Let's do it twice.
    await fetch("server.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie-not-present", "We should not have cookies");
      });

    is(document.cookie, "", "Still no cookies for me");

    /* import-globals-from storageAccessAPIHelpers.js */
    await callRequestStorageAccess();

    if (choice == BLOCK) {
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

  let permChanged;
  // Only create the promise when we're going to click one of the allow buttons.
  if (choice != BLOCK) {
    permChanged = TestUtils.topicObserved("perm-changed", (subject, data) => {
      let result;
      if (choice == ALLOW) {
        result =
          subject &&
          subject
            .QueryInterface(Ci.nsIPermission)
            .type.startsWith("3rdPartyStorage^") &&
          subject.principal.origin == new URL(topPage).origin &&
          data == "added";
      }
      return result;
    });
  }
  let shownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  shownPromise.then(async _ => {
    if (topPage != gBrowser.currentURI.spec) {
      return;
    }
    ok(showPrompt, "We shouldn't show the prompt when we don't intend to");
    let notification = await new Promise(function poll(resolve) {
      let notification = PopupNotifications.getNotification(
        "storage-access",
        browser
      );
      if (notification) {
        resolve(notification);
        return;
      }
      setTimeout(poll, 10);
    });
    Assert.ok(notification, "Should have gotten the notification");

    if (choice == BLOCK) {
      if (useEscape) {
        EventUtils.synthesizeKey("KEY_Escape", {}, window);
      } else {
        await clickSecondaryAction();
      }
    } else if (choice == ALLOW) {
      await clickMainAction();
    }
    if (choice != BLOCK) {
      await permChanged;
    }
  });

  let url = TEST_3RD_PARTY_PAGE + "?disableWaitUntilPermission";
  let ct = SpecialPowers.spawn(
    browser,
    [{ page: url, callback: runChecks.toString(), choice, useEscape }],
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
            ifr.contentWindow.postMessage(
              "choice:" + obj.choice + ":useEscape:" + obj.useEscape,
              "*"
            );
            return;
          }

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );
  if (showPrompt) {
    await Promise.all([ct, shownPromise]);
  } else {
    await Promise.all([ct, permChanged]);
  }

  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
}

async function preparePermissionsFromOtherSites(topPage) {
  info("Faking permissions from other sites");
  let type = "3rdPartyStorage^https://tracking.example.org";
  let permission = Services.perms.ALLOW_ACTION;
  let expireType = Services.perms.EXPIRE_SESSION;
  if (topPage == TEST_TOP_PAGE) {
    // For the first page, don't do anything
  } else if (topPage == TEST_TOP_PAGE_2) {
    // For the second page, only add the permission from the first page
    PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);
  } else if (topPage == TEST_TOP_PAGE_3) {
    // For the third page, add the permissions from the first two pages
    PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_2, type, permission, expireType, 0);
  } else if (topPage == TEST_TOP_PAGE_4) {
    // For the fourth page, add the permissions from the first three pages
    PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_2, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_3, type, permission, expireType, 0);
  } else if (topPage == TEST_TOP_PAGE_5) {
    // For the fifth page, add the permissions from the first four pages
    PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_2, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_3, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_4, type, permission, expireType, 0);
  } else if (topPage == TEST_TOP_PAGE_6) {
    // For the sixth page, add the permissions from the first five pages
    PermissionTestUtils.add(TEST_DOMAIN, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_2, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_3, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_4, type, permission, expireType, 0);
    PermissionTestUtils.add(TEST_DOMAIN_5, type, permission, expireType, 0);
  } else {
    ok(false, "Unexpected top page: " + topPage);
  }
}

async function cleanUp() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

async function runRound(topPage, showPrompt, maxConcurrent) {
  if (showPrompt) {
    await preparePermissionsFromOtherSites(topPage);
    await testDoorHanger(BLOCK, showPrompt, true, topPage, maxConcurrent);
    await cleanUp();
    await preparePermissionsFromOtherSites(topPage);
    await testDoorHanger(BLOCK, showPrompt, false, topPage, maxConcurrent);
    await cleanUp();
    await preparePermissionsFromOtherSites(topPage);
    await testDoorHanger(ALLOW, showPrompt, false, topPage, maxConcurrent);
    await cleanUp();
  } else {
    await preparePermissionsFromOtherSites(topPage);
    await testDoorHanger(ALLOW, showPrompt, false, topPage, maxConcurrent);
  }
  await cleanUp();
}

add_task(async function() {
  await runRound(TEST_TOP_PAGE, false, 1);
  await runRound(TEST_TOP_PAGE_2, true, 1);
  await runRound(TEST_TOP_PAGE, false, 5);
  await runRound(TEST_TOP_PAGE_2, false, 5);
  await runRound(TEST_TOP_PAGE_3, false, 5);
  await runRound(TEST_TOP_PAGE_4, false, 5);
  await runRound(TEST_TOP_PAGE_5, false, 5);
  await runRound(TEST_TOP_PAGE_6, true, 5);
});

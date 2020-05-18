/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * This test makes sure the when we grant the storage permission, the
 * permission is also propagated to iframes within the same tab,
 * but not to iframes in the other tabs.
 */
add_task(async function() {
  info("Starting permission propagation test");

  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
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

  let msg = {};
  let page = TEST_3RD_PARTY_PAGE_WORKER;
  msg.blockingCallback = (async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    await noStorageAccessInitially();

    await new Promise(resolve => {
      // eslint-disable-next-line no-undef
      let w = worker;
      w.addEventListener(
        "message",
        e => {
          ok(!e.data, "IDB is disabled");
          resolve();
        },
        { once: true }
      );
      w.postMessage("go");
    });
  }).toString();

  msg.nonBlockingCallback = (async _ => {
    /* import-globals-from storageAccessAPIHelpers.js */
    console.log("test hasStorageAccessInitially\n");
    await hasStorageAccessInitially();

    await new Promise(resolve => {
      // eslint-disable-next-line no-undef
      let w = worker;
      w.addEventListener(
        "message",
        e => {
          ok(e.data, "IDB is enabled");
          resolve();
        },
        { once: true }
      );
      w.postMessage("go");
    });
  }).toString();

  info("Creating the first tab");
  let tab1 = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab1;

  let browser1 = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser1);

  await SpecialPowers.spawn(browser1, [page, msg], async function(page, msg) {
    let ifr = content.document.createElement("iframe");
    await new content.Promise(resolve => {
      ifr.src = page;
      ifr.id = "ifr";
      ifr.onload = resolve;
      content.document.body.appendChild(ifr);
    });

    await new content.Promise(resolve => {
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

        ok(false, "Unknown message");
      });

      ifr.contentWindow.postMessage({ callback: msg.blockingCallback }, "*");
    });
  });

  info("Creating the second tab");
  let tab2 = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab2;

  let browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  // The second tab has two iframes
  await SpecialPowers.spawn(browser2, [page, msg], async function(page, msg) {
    let iframes = [];
    for (var i = 0; i < 2; i++) {
      iframes[i] = content.document.createElement("iframe");
      await new content.Promise(resolve => {
        iframes[i].src = page;
        iframes[i].onload = resolve;
        content.document.body.appendChild(iframes[i]);
      });

      await new content.Promise(resolve => {
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

          ok(false, "Unknown message");
        });

        iframes[i].contentWindow.postMessage(
          { callback: msg.blockingCallback },
          "*"
        );
      });
    }

    info("Grant storage permission to the first iframe in the second tab");
    await new content.Promise(resolve => {
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

        ok(false, "Unknown message");
      });

      iframes[0].contentWindow.postMessage(
        {
          callback: (async _ => {
            /* import-globals-from storageAccessAPIHelpers.js */
            await callRequestStorageAccess();
          }).toString(),
        },
        "*"
      );
    });

    info("Both iframs in the second tab should have stroage permission");
    for (i = 0; i < 2; i++) {
      await new content.Promise(resolve => {
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

          ok(false, "Unknown message");
        });

        iframes[i].contentWindow.postMessage(
          { callback: msg.nonBlockingCallback },
          "*"
        );
      });
    }
  });

  info("the iframe of the first tab should not have storage permission");
  await SpecialPowers.spawn(browser1, [page, msg], async function(page, msg) {
    let ifr = content.document.getElementById("ifr");

    await new content.Promise(resolve => {
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

        ok(false, "Unknown message");
      });

      ifr.contentWindow.postMessage({ callback: msg.blockingCallback }, "*");
    });
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

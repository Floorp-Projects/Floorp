/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * This test makes sure the when we grant the storage permission, the
 * permission is also propagated to iframes within the same agent cluster,
 * but not to iframes in the other tabs.
 */

async function createTab(topUrl, iframeCount, opener, params) {
  let newTab;
  let browser;
  if (opener) {
    let promise = BrowserTestUtils.waitForNewTab(gBrowser, topUrl);
    await SpecialPowers.spawn(opener, [topUrl], function(url) {
      content.window.open(url, "_blank");
    });
    newTab = await promise;
    browser = gBrowser.getBrowserForTab(newTab);
  } else {
    newTab = BrowserTestUtils.addTab(gBrowser, topUrl);

    browser = gBrowser.getBrowserForTab(newTab);
    await BrowserTestUtils.browserLoaded(browser);
  }

  await SpecialPowers.spawn(
    browser,
    [params, iframeCount, createTrackerFrame.toString()],
    async function(params, count, fn) {
      // eslint-disable-next-line no-eval
      let fnCreateTrackerFrame = eval(`(() => (${fn}))()`);
      await fnCreateTrackerFrame(params, count, ifr => {
        ifr.contentWindow.postMessage(
          { callback: params.msg.blockingCallback },
          "*"
        );
      });
    }
  );

  return Promise.resolve(newTab);
}

async function createTrackerFrame(params, count, callback) {
  let iframes = [];
  for (var i = 0; i < count; i++) {
    iframes[i] = content.document.createElement("iframe");
    await new content.Promise(resolve => {
      iframes[i].id = "ifr" + i;
      iframes[i].src = params.page;
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

      callback(iframes[i]);
    });
  }
}

async function testPermission(browser, block, params) {
  await SpecialPowers.spawn(browser, [block, params], async function(
    block,
    params
  ) {
    for (let i = 0; ; i++) {
      let ifr = content.document.getElementById("ifr" + i);
      if (!ifr) {
        break;
      }

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

        if (block) {
          ifr.contentWindow.postMessage(
            { callback: params.msg.blockingCallback },
            "*"
          );
        } else {
          ifr.contentWindow.postMessage(
            { callback: params.msg.nonBlockingCallback },
            "*"
          );
        }
      });
    }
  });
}

add_task(async function testPermissionGrantedOn3rdParty() {
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

  let top = TEST_TOP_PAGE;
  let page = TEST_3RD_PARTY_PAGE_WORKER;
  let pageOther =
    TEST_ANOTHER_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyWorker.html";
  let params = { page, msg, pageOther };
  // Create 4 tabs:
  // 1. The first tab has two tracker iframes, said A & B.
  // 2. The second tab is opened by the first tab, and it has one tracker iframe, said C.
  // 3. The third tab has one tracker iframe, said D.
  // 4. The fourth tab is opened by the first tab but with a different top-level url).
  //    The tab has one tracker iframe, said E.
  //
  // This test grants permission on iframe A, which then should propagate storage
  // permission to iframe B & C, but not D, E

  info("Creating the first tab");
  let tab1 = await createTab(top, 2, null, params);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  info("Creating the second tab");
  let tab2 = await createTab(top, 1, browser1 /* opener */, params);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  info("Creating the third tab");
  let tab3 = await createTab(top, 1, null, params);
  let browser3 = gBrowser.getBrowserForTab(tab3);

  info("Creating the fourth tab");
  let tab4 = await createTab(TEST_TOP_PAGE_2, 1, browser1, params);
  let browser4 = gBrowser.getBrowserForTab(tab4);

  info("Grant storage permission to the first iframe in the first tab");
  await SpecialPowers.spawn(browser1, [page, msg], async function(page, msg) {
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

      let ifr = content.document.getElementById("ifr0");
      ifr.contentWindow.postMessage(
        {
          callback: (async _ => {
            /* import-globals-from storageAccessAPIHelpers.js */
            await callRequestStorageAccess();
          }).toString(),
        },
        "*"
      );
    });
  });

  info("Both iframs of the first tab should have stroage permission");
  await testPermission(browser1, false /* block */, params);

  info("The iframe of the second tab should have storage permission");
  await testPermission(browser2, false /* block */, params);

  info("The iframe of the third tab should not have storage permission");
  await testPermission(browser3, true /* block */, params);

  info("The iframe of the fourth tab should not have storage permission");
  await testPermission(browser4, true /* block */, params);

  info("Removing the tabs");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);

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

add_task(async function testPermissionGrantedOnFirstParty() {
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
    ],
  });

  await UrlClassifierTestUtils.addTestTrackers();

  let msg = {};
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

  let top = TEST_TOP_PAGE;
  let page = TEST_3RD_PARTY_PAGE_WORKER;
  let params = { page, msg };
  // Create 4 tabs:
  // 1. The first tab has two tracker iframes, said A & B.
  // 2. The second tab is opened by the first tab, and it has one tracker iframe, said C.
  // 3. The third tab has one tracker iframe, said D.
  // 4. The fourth tab is opened by the first tab but with a different top-level url).
  //    The tab has one tracker iframe, said E.
  //
  // This test grants permission on iframe A, which then should propagate storage
  // permission to iframe B & C, but not D, E

  info("Creating the first tab");
  let tab1 = await createTab(top, 2, null, params);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  info("Creating the second tab");
  let tab2 = await createTab(top, 1, browser1 /* opener */, params);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  info("Creating the third tab");
  let tab3 = await createTab(top, 1, null, params);
  let browser3 = gBrowser.getBrowserForTab(tab3);

  info("Creating the fourth tab");
  let tab4 = await createTab(TEST_TOP_PAGE_2, 1, browser1, params);
  let browser4 = gBrowser.getBrowserForTab(tab4);

  info("Grant storage permission to the first iframe in the first tab");
  let promise = BrowserTestUtils.waitForNewTab(gBrowser, page);
  await SpecialPowers.spawn(browser1, [page], async function(page) {
    content.window.open(page, "_blank");
  });
  let tab = await promise;
  BrowserTestUtils.removeTab(tab);

  info("Both iframs of the first tab should have stroage permission");
  await testPermission(browser1, false /* block */, params);

  info("The iframe of the second tab should have storage permission");
  await testPermission(browser2, false /* block */, params);

  info("The iframe of the third tab should not have storage permission");
  await testPermission(browser3, true /* block */, params);

  info("The iframe of the fourth tab should not have storage permission");
  await testPermission(browser4, true /* block */, params);

  info("Removing the tabs");
  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
  BrowserTestUtils.removeTab(tab4);

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

/* import-globals-from antitracking_head.js */

function waitStoragePermission() {
  return new Promise(resolve => {
    let id = setInterval(async _ => {
      if (
        await SpecialPowers.testPermission(
          `3rdPartyStorage^${TEST_3RD_PARTY_DOMAIN.slice(0, -1)}`,
          SpecialPowers.Services.perms.ALLOW_ACTION,
          TEST_DOMAIN
        )
      ) {
        clearInterval(id);
        resolve();
      }
    }, 0);
  });
}

add_task(async function() {
  info("Starting subResources test");

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
});

add_task(async function testWindowOpenHeuristic() {
  info("Starting window.open() heuristic test...");

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_WO,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await noStorageAccessInitially();
      }).toString();

      msg.nonBlockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await hasStorageAccessInitially();
      }).toString();

      info("Checking if storage access is denied");
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(msg, "*");
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

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function testDoublyNestedWindowOpenHeuristic() {
  info("Starting doubly nested window.open() heuristic test...");

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_RELAY + "?" + TEST_3RD_PARTY_PAGE_WO,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await noStorageAccessInitially();
      }).toString();

      msg.nonBlockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await hasStorageAccessInitially();
      }).toString();

      info("Checking if storage access is denied");
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          info("Sending code to the 3rd party content");
          ifr.contentWindow.postMessage(msg, "*");
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

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function testUserInteractionHeuristic() {
  info("Starting user interaction heuristic test...");

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_UI,
        popup: TEST_POPUP_PAGE,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await noStorageAccessInitially();
      }).toString();

      info("Checking if storage access is denied");

      let ifr = content.document.createElement("iframe");
      let loading = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
      await loading;

      info(
        "The 3rd party content should not have access to first party storage."
      );
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

      info("Opening a window from the iframe.");
      await SpecialPowers.spawn(ifr, [obj.popup], async popup => {
        let windowClosed = new content.Promise(resolve => {
          Services.ww.registerNotification(function notification(
            aSubject,
            aTopic,
            aData
          ) {
            // We need to check the document URI for Fission. It's because the
            // 'domwindowclosed' would be triggered twice, one for the
            // 'about:blank' page and another for the tracker page.
            if (
              aTopic == "domwindowclosed" &&
              aSubject.document.documentURI ==
                "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/3rdPartyOpenUI.html"
            ) {
              Services.ww.unregisterNotification(notification);
              resolve();
            }
          });
        });

        content.open(popup);

        info("Let's wait for the window to be closed");
        await windowClosed;
      });

      info("The 3rd party content should have access to first party storage.");
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
    }
  );

  await AntiTracking.interactWithTracker();

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_UI,
        popup: TEST_POPUP_PAGE,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        await noStorageAccessInitially();
      }).toString();

      msg.nonBlockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await hasStorageAccessInitially();
      }).toString();

      info("Checking if storage access is denied");

      let ifr = content.document.createElement("iframe");
      let loading = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
      await loading;

      info(
        "The 3rd party content should not have access to first party storage."
      );
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

      info("Opening a window from the iframe.");
      await SpecialPowers.spawn(ifr, [obj.popup], async popup => {
        let windowClosed = new content.Promise(resolve => {
          Services.ww.registerNotification(function notification(
            aSubject,
            aTopic,
            aData
          ) {
            // We need to check the document URI here as well for the same
            // reason above.
            if (
              aTopic == "domwindowclosed" &&
              aSubject.document.documentURI ==
                "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/3rdPartyOpenUI.html"
            ) {
              Services.ww.unregisterNotification(notification);
              resolve();
            }
          });
        });

        content.open(popup);

        info("Let's wait for the window to be closed");
        await windowClosed;
      });

      info("The 3rd party content should have access to first party storage.");
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
        ifr.contentWindow.postMessage(
          { callback: msg.nonBlockingCallback },
          "*"
        );
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  info("Wait until the storage permission is ready before cleaning up.");
  await waitStoragePermission();

  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function testDoublyNestedUserInteractionHeuristic() {
  info("Starting doubly nested user interaction heuristic test...");

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_RELAY + "?" + TEST_3RD_PARTY_PAGE_UI,
        popup: TEST_POPUP_PAGE,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await noStorageAccessInitially();
      }).toString();

      msg.openWindowCallback = (async url => {
        open(url);
      }).toString();

      info("Checking if storage access is denied");

      let ifr = content.document.createElement("iframe");
      let loading = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
      await loading;

      info(
        "The 3rd party content should not have access to first party storage."
      );
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

      let windowClosed = new content.Promise(resolve => {
        Services.ww.registerNotification(function notification(
          aSubject,
          aTopic,
          aData
        ) {
          if (aTopic == "domwindowclosed") {
            Services.ww.unregisterNotification(notification);
            resolve();
          }
        });
      });

      info("Opening a window from the iframe.");
      ifr.contentWindow.postMessage(
        { callback: msg.openWindowCallback, arg: obj.popup },
        "*"
      );

      info("Let's wait for the window to be closed");
      await windowClosed;

      info("The 3rd party content should have access to first party storage.");
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
    }
  );

  await AntiTracking.interactWithTracker();

  info("Loading tracking scripts");
  await SpecialPowers.spawn(
    browser,
    [
      {
        page: TEST_3RD_PARTY_PAGE_RELAY + "?" + TEST_3RD_PARTY_PAGE_UI,
        popup: TEST_POPUP_PAGE,
      },
    ],
    async obj => {
      let msg = {};
      msg.blockingCallback = (async _ => {
        await noStorageAccessInitially();
      }).toString();

      msg.nonBlockingCallback = (async _ => {
        /* import-globals-from storageAccessAPIHelpers.js */
        await hasStorageAccessInitially();
      }).toString();

      msg.openWindowCallback = (async url => {
        open(url);
      }).toString();

      info("Checking if storage access is denied");

      let ifr = content.document.createElement("iframe");
      let loading = new content.Promise(resolve => {
        ifr.onload = resolve;
      });
      content.document.body.appendChild(ifr);
      ifr.src = obj.page;
      await loading;

      info(
        "The 3rd party content should not have access to first party storage."
      );
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

      let windowClosed = new content.Promise(resolve => {
        Services.ww.registerNotification(function notification(
          aSubject,
          aTopic,
          aData
        ) {
          if (aTopic == "domwindowclosed") {
            Services.ww.unregisterNotification(notification);
            resolve();
          }
        });
      });

      info("Opening a window from the iframe.");
      ifr.contentWindow.postMessage(
        { callback: msg.openWindowCallback, arg: obj.popup },
        "*"
      );

      info("Let's wait for the window to be closed");
      await windowClosed;

      info("The 3rd party content should have access to first party storage.");
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
        ifr.contentWindow.postMessage(
          { callback: msg.nonBlockingCallback },
          "*"
        );
      });
    }
  );

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  UrlClassifierTestUtils.cleanupTestTrackers();
});

add_task(async function() {
  info("Wait until the storage permission is ready before cleaning up.");
  await waitStoragePermission();

  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

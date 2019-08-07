/* import-globals-from antitracking_head.js */

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
  await ContentTask.spawn(
    browser,
    {
      page: TEST_3RD_PARTY_PAGE_WO,
    },
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
  await ContentTask.spawn(
    browser,
    {
      page: TEST_3RD_PARTY_PAGE_UI,
      popup: TEST_POPUP_PAGE,
    },
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
      ifr.contentWindow.open(obj.popup);

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
  await ContentTask.spawn(
    browser,
    {
      page: TEST_3RD_PARTY_PAGE_UI,
      popup: TEST_POPUP_PAGE,
    },
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
      ifr.contentWindow.open(obj.popup);

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
});

add_task(async function() {
  info("Cleaning up.");
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

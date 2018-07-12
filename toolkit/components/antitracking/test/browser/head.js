const TEST_DOMAIN = "http://example.net/";
const TEST_3RD_PARTY_DOMAIN = "https://tracking.example.com/";

const TEST_PATH = "browser/toolkit/components/antitracking/test/browser/";

const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_POPUP_PAGE = TEST_DOMAIN + TEST_PATH + "popup.html";
const TEST_3RD_PARTY_PAGE = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_3RD_PARTY_PAGE_WO = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyWO.html";
const TEST_3RD_PARTY_PAGE_UI = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyUI.html";

let {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

this.AntiTracking = {
  runTest(name, callbackTracking, callbackNonTracking, cleanupFunction, extraPrefs) {
    // Here we want to test that a 3rd party context is simply blocked.
    this._createTask(name, true, callbackTracking, extraPrefs);
    this._createCleanupTask(cleanupFunction);

    if (callbackNonTracking) {
      // Here we want to test that a 3rd party context is not blocked if pref is off.
      this._createTask(name, false, callbackNonTracking);
      this._createCleanupTask(cleanupFunction);

      // Permission granted when there is a window.open()
      this._createWindowOpenTask(name, callbackTracking, callbackNonTracking, extraPrefs);
      this._createCleanupTask(cleanupFunction);

      // Permission granted when there is user-interaction.
      this._createUserInteractionTask(name, callbackTracking, callbackNonTracking, extraPrefs);
      this._createCleanupTask(cleanupFunction);
    }
  },

  async _setupTest(blocking, extraPrefs) {
    await SpecialPowers.flushPrefEnv();
    await SpecialPowers.pushPrefEnv({"set": [
      ["privacy.restrict3rdpartystorage.enabled", blocking],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", blocking],
    ]});

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({"set": extraPrefs });
    }

    await UrlClassifierTestUtils.addTestTrackers();
  },

  _createTask(name, blocking, callback, extraPrefs) {
    add_task(async function() {
      info("Starting " + (blocking ? "blocking" : "non-blocking") + " test " + name);

      await AntiTracking._setupTest(blocking, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE,
                                callback: callback.toString() },
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

            ok(false, "Unknown message");
          });

          content.document.body.appendChild(ifr);
          ifr.src = obj.page;
        });
      });

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      UrlClassifierTestUtils.cleanupTestTrackers();
    });
  },

  _createCleanupTask(cleanupFunction) {
    add_task(async function() {
      info("Cleaning up.");
      if (cleanupFunction) {
        await cleanupFunction();
      }
    });
  },

  _createWindowOpenTask(name, blockingCallback, nonBlockingCallback, extraPrefs) {
    add_task(async function() {
      info("Starting window-open test " + name);
      await AntiTracking._setupTest(true, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE_WO,
                                blockingCallback: blockingCallback.toString(),
                                nonBlockingCallback: nonBlockingCallback.toString(),
                              },
                              async function(obj) {
        await new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.onload = function() {
            info("Sending code to the 3rd party content");
            ifr.contentWindow.postMessage(obj, "*");
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
      });

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      UrlClassifierTestUtils.cleanupTestTrackers();
    });
  },

  _createUserInteractionTask(name, blockingCallback, nonBlockingCallback, extraPrefs) {
    add_task(async function() {
      info("Starting user-interaction test " + name);
      await AntiTracking._setupTest(true, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE_UI,
                                popup: TEST_POPUP_PAGE,
                                blockingCallback: blockingCallback.toString(),
                                nonBlockingCallback: nonBlockingCallback.toString(),
                              },
                              async function(obj) {
        let ifr = content.document.createElement("iframe");
        let loading = new content.Promise(resolve => { ifr.onload = resolve; });
        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
        await loading;

        info("The 3rd party content should not have access to first party storage.");
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
          ifr.contentWindow.postMessage({ callback: obj.blockingCallback }, "*");
        });

        let windowClosed = new content.Promise(resolve => {
          Services.ww.registerNotification(function notification(aSubject, aTopic, aData) {
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
          ifr.contentWindow.postMessage({ callback: obj.nonBlockingCallback }, "*");
        });
      });

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      UrlClassifierTestUtils.cleanupTestTrackers();
    });
  }
};

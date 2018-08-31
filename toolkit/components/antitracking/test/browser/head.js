const TEST_DOMAIN = "http://example.net/";
const TEST_3RD_PARTY_DOMAIN = "https://tracking.example.org/";
const TEST_3RD_PARTY_DOMAIN_TP = "https://tracking.example.com/";

const TEST_PATH = "browser/toolkit/components/antitracking/test/browser/";

const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_EMBEDDER_PAGE = TEST_DOMAIN + TEST_PATH + "embedder.html";
const TEST_POPUP_PAGE = TEST_DOMAIN + TEST_PATH + "popup.html";
const TEST_3RD_PARTY_PAGE = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_3RD_PARTY_PAGE_WO = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyWO.html";
const TEST_3RD_PARTY_PAGE_UI = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyUI.html";
const TEST_3RD_PARTY_PAGE_WITH_SVG = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartySVG.html";

const BEHAVIOR_ACCEPT         = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_REJECT_FOREIGN = Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

var gFeatures = undefined;

let {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

this.AntiTracking = {
  runTest(name, callbackTracking, callbackNonTracking, cleanupFunction, extraPrefs, windowOpenTest = true, userInteractionTest = true, expectedBlockingNotifications = true) {
    // Here we want to test that a 3rd party context is simply blocked.
    this._createTask({
      name,
      cookieBehavior: BEHAVIOR_REJECT_TRACKER,
      blockingByContentBlocking: true,
      allowList: false,
      callback: callbackTracking,
      extraPrefs,
      expectedBlockingNotifications,
    });
    this._createCleanupTask(cleanupFunction);

    if (callbackNonTracking) {
      let runExtraTests = true;
      let options = {};
      if (typeof callbackNonTracking == "object") {
        callbackNonTracking = callbackNonTracking.callback;
        runExtraTests = callbackNonTracking.runExtraTests;
        if ("cookieBehavior" in callbackNonTracking) {
          options.cookieBehavior = callbackNonTracking.cookieBehavior;
        } else {
          options.cookieBehavior = BEHAVIOR_ACCEPT;
        }
        if ("blockingByContentBlocking" in callbackNonTracking) {
          options.blockingByContentBlocking =
            callbackNonTracking.blockingByContentBlocking;
        } else {
          options.blockingByContentBlocking = false;
        }
        if ("blockingByAllowList" in callbackNonTracking) {
          options.blockingByAllowList =
            callbackNonTracking.blockingByAllowList;
        } else {
          options.blockingByAllowList = false;
        }
      }

      // Phase 1: Here we want to test that a 3rd party context is not blocked if pref is off.
      if (runExtraTests) {
        // There are four ways in which the third-party context may not be blocked:
        //   * If the cookieBehavior pref causes it to not be blocked.
        //   * If the contentBlocking pref causes it to not be blocked.
        //   * If both of these prefs cause it to not be blocked.
        //   * If the top-level page is on the content blocking allow list.
        // All of these cases are tested here.
        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        // Try testing using the allow list with both reject foreign and reject tracker cookie behaviors
        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);
      } else {
        this._createTask({
          name,
          cookieBehavior: options.cookieBehavior,
          blockingByContentBlocking: options.blockingByContentBlocking,
          allowList: options.blockingByAllowList,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
        });
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 2: Here we want to test that a third-party context doesn't
      // get blocked with when the same origin is opened through window.open().
      if (windowOpenTest) {
        this._createWindowOpenTask(name, callbackTracking, callbackNonTracking, extraPrefs);
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 3: Here we want to test that a third-party context doesn't
      // get blocked with user interaction present
      if (userInteractionTest) {
        this._createUserInteractionTask(name, callbackTracking, callbackNonTracking, extraPrefs);
        this._createCleanupTask(cleanupFunction);
      }
    }
  },

  async _setupTest(cookieBehavior, blockingByContentBlocking, extraPrefs) {
    await SpecialPowers.flushPrefEnv();
    await SpecialPowers.pushPrefEnv({"set": [
      ["browser.contentblocking.enabled", blockingByContentBlocking],
      ["network.cookie.cookieBehavior", cookieBehavior],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", cookieBehavior != BEHAVIOR_ACCEPT],
      [ContentBlocking.prefIntroCount, ContentBlocking.MAX_INTROS],
    ]});

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({"set": extraPrefs });
    }

    await UrlClassifierTestUtils.addTestTrackers();
  },

  _createTask(options) {
    add_task(async function() {
      info("Starting " + (options.cookieBehavior != BEHAVIOR_ACCEPT ? "blocking" : "non-blocking") + " cookieBehavior (" + options.cookieBehavior + ") and " +
                         (options.blockingByContentBlocking ? "blocking" : "non-blocking") + " contentBlocking with" +
                         (options.allowList ? "" : "out") + " allow list test " + options.name);

     requestLongerTimeout(2);

      await AntiTracking._setupTest(options.cookieBehavior,
                                    options.blockingByContentBlocking,
                                    options.extraPrefs);

      let cookieBlocked = 0;
      let listener = {
        onSecurityChange(webProgress, request, stateFlags, status) {
          if (stateFlags & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_COOKIES) {
            ++cookieBlocked;
          }
        },
      };
      gBrowser.addProgressListener(listener);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      if (options.allowList) {
        info("Disabling content blocking for this page");
        ContentBlocking.disableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE,
                                callback: options.callback.toString() },
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

      if (options.allowList) {
        info("Enabling content blocking for this page");
        ContentBlocking.enableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      gBrowser.removeProgressListener(listener);

      is(!!cookieBlocked, options.expectedBlockingNotifications, "Checking cookie blocking notifications");

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);
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

      requestLongerTimeout(2);

      await AntiTracking._setupTest(BEHAVIOR_REJECT_TRACKER, true, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      let pageURL = TEST_3RD_PARTY_PAGE_WO;
      if (gFeatures == "noopener") {
        pageURL += "?noopener";
      }

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: pageURL,
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
    });
  },

  _createUserInteractionTask(name, blockingCallback, nonBlockingCallback, extraPrefs) {
    add_task(async function() {
      info("Starting user-interaction test " + name);

      requestLongerTimeout(2);

      await AntiTracking._setupTest(BEHAVIOR_REJECT_TRACKER, true, extraPrefs);

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
    });
  },
};

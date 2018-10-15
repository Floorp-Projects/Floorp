const TEST_DOMAIN = "http://example.net";
const TEST_3RD_PARTY_DOMAIN = "https://tracking.example.org";
const TEST_3RD_PARTY_DOMAIN_TP = "https://tracking.example.com";
const TEST_4TH_PARTY_DOMAIN = "http://not-tracking.example.com";

const TEST_PATH = "/browser/toolkit/components/antitracking/test/browser/";

const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_EMBEDDER_PAGE = TEST_DOMAIN + TEST_PATH + "embedder.html";
const TEST_POPUP_PAGE = TEST_DOMAIN + TEST_PATH + "popup.html";
const TEST_3RD_PARTY_PAGE = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_3RD_PARTY_PAGE_WO = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyWO.html";
const TEST_3RD_PARTY_PAGE_UI = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyUI.html";
const TEST_3RD_PARTY_PAGE_WITH_SVG = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartySVG.html";
const TEST_4TH_PARTY_PAGE = TEST_4TH_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";

const BEHAVIOR_ACCEPT         = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_REJECT_FOREIGN = Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

var gFeatures = undefined;

let {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

requestLongerTimeout(5);

this.AntiTracking = {
  runTest(name, callbackTracking, callbackNonTracking, cleanupFunction, extraPrefs,
          windowOpenTest = true, userInteractionTest = true, expectedBlockingNotifications = true,
          runInPrivateWindow = false, iframeSandbox = null, accessRemoval = null,
          callbackAfterRemoval = null) {
    // Here we want to test that a 3rd party context is simply blocked.
    this._createTask({
      name,
      cookieBehavior: BEHAVIOR_REJECT_TRACKER,
      blockingByContentBlocking: true,
      blockingByContentBlockingRTUI: true,
      allowList: false,
      callback: callbackTracking,
      extraPrefs,
      expectedBlockingNotifications,
      runInPrivateWindow,
      iframeSandbox,
      accessRemoval,
      callbackAfterRemoval,
    });
    this._createCleanupTask(cleanupFunction);

    this._createTask({
      name,
      cookieBehavior: BEHAVIOR_REJECT_TRACKER,
      blockingByContentBlocking: true,
      blockingByContentBlockingRTUI: false,
      allowList: true,
      callback: callbackTracking,
      extraPrefs,
      expectedBlockingNotifications,
      runInPrivateWindow,
      iframeSandbox,
      accessRemoval,
      callbackAfterRemoval,
    });
    this._createCleanupTask(cleanupFunction);

    if (callbackNonTracking) {
      let runExtraTests = true;
      let options = {};
      if (typeof callbackNonTracking == "object") {
        options.callback = callbackNonTracking.callback;
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
        if ("blockingByContentBlockingRTUI" in callbackNonTracking) {
          options.blockingByContentBlockingRTUI =
            callbackNonTracking.blockingByContentBlockingRTUI;
        } else {
          options.blockingByContentBlockingRTUI = false;
        }
        if ("blockingByAllowList" in callbackNonTracking) {
          options.blockingByAllowList =
            callbackNonTracking.blockingByAllowList;
        } else {
          options.blockingByAllowList = false;
        }
        callbackNonTracking = options.callback;
        options.accessRemoval = null;
        options.callbackAfterRemoval = null;
      }

      // Phase 1: Here we want to test that a 3rd party context is not blocked if pref is off.
      if (runExtraTests) {
        // There are five ways in which the third-party context may not be blocked:
        //   * If the cookieBehavior pref causes it to not be blocked.
        //   * If the contentBlocking pref causes it to not be blocked.
        //   * If both of these prefs cause it to not be blocked.
        //   * If the top-level page is on the content blocking allow list.
        //   * If the contentBlocking third-party cookies UI pref is off, the allow list will be ignored.
        // All of these cases are tested here.
        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: true,
          blockingByContentBlockingRTUI: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: true,
          blockingByContentBlockingRTUI: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: true,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlocking: true,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: false,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlocking: true,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);
      } else if (!options.blockingByContentBlocking) {
        // Don't run this extra test if we are in blocking mode!
        this._createTask({
          name,
          cookieBehavior: options.cookieBehavior,
          blockingByContentBlocking: options.blockingByContentBlocking,
          blockingByContentBlockingRTUI: options.blockingByContentBlockingRTUI,
          allowList: options.blockingByAllowList,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 2: Here we want to test that a third-party context doesn't
      // get blocked with when the same origin is opened through window.open().
      if (windowOpenTest) {
        this._createWindowOpenTask(name, callbackTracking, callbackNonTracking,
                                   runInPrivateWindow, iframeSandbox, extraPrefs);
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 3: Here we want to test that a third-party context doesn't
      // get blocked with user interaction present
      if (userInteractionTest) {
        this._createUserInteractionTask(name, callbackTracking, callbackNonTracking,
                                        runInPrivateWindow, iframeSandbox,
                                        expectedBlockingNotifications, extraPrefs);
        this._createCleanupTask(cleanupFunction);
      }
    }
  },

  async interactWithTracker() {
    let windowClosed = new Promise(resolve => {
      Services.ww.registerNotification(function notification(aSubject, aTopic, aData) {
        if (aTopic == "domwindowclosed") {
          Services.ww.unregisterNotification(notification);
          resolve();
        }
      });
    });

    info("Let's interact with the tracker");
    window.open(TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyOpenUI.html");
    await windowClosed;
  },

  async _setupTest(win, cookieBehavior, blockingByContentBlocking,
                   blockingByContentBlockingRTUI,
                   extraPrefs) {
    await SpecialPowers.flushPrefEnv();
    await SpecialPowers.pushPrefEnv({"set": [
      ["dom.storage_access.enabled", true],
      ["browser.contentblocking.allowlist.annotations.enabled", blockingByContentBlockingRTUI],
      ["browser.contentblocking.allowlist.storage.enabled", blockingByContentBlockingRTUI],
      ["browser.contentblocking.enabled", blockingByContentBlocking],
      ["network.cookie.cookieBehavior", cookieBehavior],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", cookieBehavior != BEHAVIOR_ACCEPT],
      [win.ContentBlocking.prefIntroCount, win.ContentBlocking.MAX_INTROS],
      ["browser.fastblock.enabled", false], // prevent intermittent failures
    ]});

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({"set": extraPrefs });
    }

    await UrlClassifierTestUtils.addTestTrackers();
  },

  _createTask(options) {
    add_task(async function() {
      info("Starting " + (options.cookieBehavior != BEHAVIOR_ACCEPT ? "blocking" : "non-blocking") + " cookieBehavior (" + options.cookieBehavior + ") and " +
                         (options.blockingByContentBlocking ? "blocking" : "non-blocking") + " contentBlocking and " +
                         (options.blockingByContentBlockingRTUI ? "" : "no") + " contentBlocking third-party cookies UI with" +
                         (options.allowList ? "" : "out") + " allow list test " + options.name +
                         " running in a " + (options.runInPrivateWindow ? "private" : "normal") + " window " +
                         " with iframe sandbox set to " + options.iframeSandbox +
                         " and access removal set to " + options.accessRemoval);

      is(!!options.callbackAfterRemoval, !!options.accessRemoval,
         "callbackAfterRemoval must be passed when accessRemoval is non-null");

      let win = window;
      if (options.runInPrivateWindow) {
        win = OpenBrowserWindow({private: true});
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, options.cookieBehavior,
                                    options.blockingByContentBlocking,
                                    options.blockingByContentBlockingRTUI,
                                    options.extraPrefs);

      let cookieBlocked = 0;
      let listener = {
        onSecurityChange(webProgress, request, oldState, state,
                         contentBlockingLogJSON) {
          if (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) {
            ++cookieBlocked;
          }
          let contentBlockingLog = {};
          try {
            contentBlockingLog = JSON.parse(contentBlockingLogJSON);
          } catch (e) {
          }

          let trackerInteractionHelper = false;
          if (request) {
            request.QueryInterface(Ci.nsIChannel);
            trackerInteractionHelper = request.URI.spec.endsWith("?messageme");
          }

          // If this is the first cookie to be blocked, our state should have
          // just changed, otherwise it should have previously contained the
          // STATE_COOKIES_BLOCKED_TRACKER bit too.
          if (options.expectedBlockingNotifications && cookieBlocked &&
              !options.allowList && !trackerInteractionHelper) {
            if (cookieBlocked == 1) {
              is(oldState & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, 0,
                 "When blocking the first cookie, old state should not have had the " +
                 "STATE_COOKIES_BLOCKED_TRACKER bit");
            }

            for (let trackerOrigin in contentBlockingLog) {
              is(trackerOrigin, TEST_3RD_PARTY_DOMAIN, "Correct tracker origin must be reported");
              let originLog = contentBlockingLog[trackerOrigin];
              if (originLog.length == 1) {
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
              } else {
                // This branch is needed here because of the tests that use the storage
                // access API to gain storage access.
                is(originLog.length, 2, "Correct origin log length");
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
                is(originLog[1][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[1][1], false,
                   "Correct blocking status reported");
                is(originLog[1][2], 1,
                   "Correct repeat count reported");
              }
            }
            // Can't assert the number of tracker origins because we may get 0
            // for web progress navigations coming from the window opening from
            // storage access API tracker interaction attempts...
          }
          if (!options.expectedBlockingNotifications) {
            is(oldState & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, 0,
               "When not blocking, old state should not have had the " +
               "STATE_COOKIES_BLOCKED_TRACKER bit");
            is(Object.keys(contentBlockingLog).length, 0,
               "Content blocking log JSON must be empty");
          }
        },
      };
      win.gBrowser.addProgressListener(listener);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      if (options.allowList) {
        info("Disabling content blocking for this page");
        win.ContentBlocking.disableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      info("Creating a 3rd party content");
      let doAccessRemovalChecks = typeof options.accessRemoval == "string" &&
                                  options.cookieBehavior == BEHAVIOR_REJECT_TRACKER &&
                                  options.blockingByContentBlocking &&
                                  options.blockingByContentBlockingRTUI &&
                                  !options.allowList;
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE,
                                nextPage: TEST_4TH_PARTY_PAGE,
                                callback: options.callback.toString(),
                                callbackAfterRemoval: options.callbackAfterRemoval ?
                                  options.callbackAfterRemoval.toString() : null,
                                accessRemoval: options.accessRemoval,
                                iframeSandbox: options.iframeSandbox,
                                doAccessRemovalChecks },
                              async function(obj) {
        let id = "id" + Math.random();
        await new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.id = id;
          ifr.onload = function() {
            info("Sending code to the 3rd party content");
            ifr.contentWindow.postMessage(obj.callback, "*");
          };
          if (typeof obj.iframeSandbox == "string") {
            ifr.setAttribute("sandbox", obj.iframeSandbox);
          }

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

        if (obj.doAccessRemovalChecks) {
          info(`Running after removal checks (${obj.accessRemoval})`);
          switch (obj.accessRemoval) {
          case "navigate-subframe":
            await new content.Promise(resolve => {
              let ifr = content.document.getElementById(id);
              let oldWindow = ifr.contentWindow;
              ifr.onload = function() {
                info("Sending code to the old 3rd party content");
                oldWindow.postMessage(obj.callbackAfterRemoval, "*");
              };
              if (typeof obj.iframeSandbox == "string") {
                ifr.setAttribute("sandbox", obj.iframeSandbox);
              }

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

              ifr.src = obj.nextPage;
            });
            break;
          default:
            ok(false, "Unexpected accessRemoval code passed: " + obj.accessRemoval);
            break;
          }
        }
      });

      if (options.allowList) {
        info("Enabling content blocking for this page");
        win.ContentBlocking.enableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      win.gBrowser.removeProgressListener(listener);

      is(!!cookieBlocked, options.expectedBlockingNotifications, "Checking cookie blocking notifications");

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (options.runInPrivateWindow) {
        win.close();
      }
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

  _createWindowOpenTask(name, blockingCallback, nonBlockingCallback, runInPrivateWindow,
                        iframeSandbox, extraPrefs) {
    add_task(async function() {
      info("Starting window-open test " + name);

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({private: true});
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, true, true, true, extraPrefs);

      let cookieBlocked = 0;
      let listener = {
        onSecurityChange(webProgress, request, oldState, state,
                         contentBlockingLogJSON) {
          if (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) {
            ++cookieBlocked;
          }
          let contentBlockingLog = {};
          try {
            contentBlockingLog = JSON.parse(contentBlockingLogJSON);
          } catch (e) {
          }
          // If this is the first cookie to be blocked, our state should have
          // just changed, otherwise it should have previously contained the
          // STATE_COOKIES_BLOCKED_TRACKER bit too.
          if (cookieBlocked) {
            if (cookieBlocked == 1) {
              is(oldState & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, 0,
                 "When blocking the first cookie, old state should not have had the " +
                 "STATE_COOKIES_BLOCKED_TRACKER bit");
            }

            for (let trackerOrigin in contentBlockingLog) {
              is(trackerOrigin, TEST_3RD_PARTY_DOMAIN, "Correct tracker origin must be reported");
              let originLog = contentBlockingLog[trackerOrigin];
              if (originLog.length == 1) {
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
              } else {
                is(originLog.length, 2, "Correct origin log length");
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
                is(originLog[1][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[1][1], false,
                   "Correct blocking status reported");
                is(originLog[1][2], 1,
                   "Correct repeat count reported");
              }
            }
            // Can't assert the number of tracker origins because we may get 0
            // for web progress navigations coming from the window opening...
          }
        },
      };
      win.gBrowser.addProgressListener(listener);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
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
                                iframeSandbox,
                              },
                              async function(obj) {
        await new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.onload = function() {
            info("Sending code to the 3rd party content");
            ifr.contentWindow.postMessage(obj, "*");
          };
          if (typeof obj.iframeSandbox == "string") {
            ifr.setAttribute("sandbox", obj.iframeSandbox);
          }

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

      win.gBrowser.removeProgressListener(listener);

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },

  _createUserInteractionTask(name, blockingCallback, nonBlockingCallback,
                             runInPrivateWindow, iframeSandbox,
                             expectedBlockingNotifications, extraPrefs) {
    add_task(async function() {
      info("Starting user-interaction test " + name);

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({private: true});
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, true, true, true, extraPrefs);

      let cookieBlocked = 0;
      let listener = {
        onSecurityChange(webProgress, request, oldState, state,
                         contentBlockingLogJSON) {
          if (state & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER) {
            ++cookieBlocked;
          }
          let contentBlockingLog = {};
          try {
            contentBlockingLog = JSON.parse(contentBlockingLogJSON);
          } catch (e) {
          }
          // If this is the first cookie to be blocked, our state should have
          // just changed, otherwise it should have previously contained the
          // STATE_COOKIES_BLOCKED_TRACKER bit too.
          if (expectedBlockingNotifications && cookieBlocked) {
            if (cookieBlocked == 1) {
              is(oldState & Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER, 0,
                 "When blocking the first cookie, old state should not have had the " +
                 "STATE_COOKIES_BLOCKED_TRACKER bit");
            }

            for (let trackerOrigin in contentBlockingLog) {
              is(trackerOrigin, TEST_3RD_PARTY_DOMAIN, "Correct tracker origin must be reported");
              let originLog = contentBlockingLog[trackerOrigin];
              if (originLog.length == 1) {
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
              } else {
                is(originLog.length, 2, "Correct origin log length");
                is(originLog[0][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[0][1], true,
                   "Correct blocking status reported");
                ok(originLog[0][2] >= 1,
                   "Correct repeat count reported");
                is(originLog[1][0], Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
                   "Correct blocking type reported");
                is(originLog[1][1], false,
                   "Correct blocking status reported");
                ok(originLog[1][2] >= 1,
                   "Correct repeat count reported");
              }
            }
            // Can't assert the number of tracker origins because we may get 0
            // for web progress navigations coming from the window opening...
          }
        },
      };
      win.gBrowser.addProgressListener(listener);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE_UI,
                                popup: TEST_POPUP_PAGE,
                                blockingCallback: blockingCallback.toString(),
                                iframeSandbox,
                              },
                              async function(obj) {
        let ifr = content.document.createElement("iframe");
        let loading = new content.Promise(resolve => { ifr.onload = resolve; });
        if (typeof obj.iframeSandbox == "string") {
          ifr.setAttribute("sandbox", obj.iframeSandbox);
        }
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

        info("First time, the 3rd party content should not have access to first party storage " +
             "because the tracker did not have user interaction");
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
      });

      await AntiTracking.interactWithTracker();

      await ContentTask.spawn(browser,
                              { page: TEST_3RD_PARTY_PAGE_UI,
                                popup: TEST_POPUP_PAGE,
                                nonBlockingCallback: nonBlockingCallback.toString(),
                                iframeSandbox,
                              },
                              async function(obj) {
        let ifr = content.document.createElement("iframe");
        let loading = new content.Promise(resolve => { ifr.onload = resolve; });
        if (typeof obj.iframeSandbox == "string") {
          ifr.setAttribute("sandbox", obj.iframeSandbox);
        }
        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
        await loading;

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

        info("The 3rd party content should now have access to first party storage.");
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

      win.gBrowser.removeProgressListener(listener);

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },
};

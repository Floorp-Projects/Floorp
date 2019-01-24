const TEST_DOMAIN = "http://example.net/";
const TEST_DOMAIN_2 = "http://xn--exmple-cua.test/";
const TEST_DOMAIN_3 = "https://xn--hxajbheg2az3al.xn--jxalpdlp/";
const TEST_DOMAIN_4 = "http://prefixexample.com/";
const TEST_DOMAIN_5 = "http://test/";
const TEST_DOMAIN_6 = "http://mochi.test:8888/";
const TEST_3RD_PARTY_DOMAIN = "https://tracking.example.org/";
const TEST_3RD_PARTY_DOMAIN_TP = "https://tracking.example.com/";
const TEST_4TH_PARTY_DOMAIN = "http://not-tracking.example.com/";
const TEST_ANOTHER_3RD_PARTY_DOMAIN = "https://another-tracking.example.net/";

const TEST_PATH = "browser/toolkit/components/antitracking/test/browser/";

const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "page.html";
const TEST_TOP_PAGE_2 = TEST_DOMAIN_2 + TEST_PATH + "page.html";
const TEST_TOP_PAGE_3 = TEST_DOMAIN_3 + TEST_PATH + "page.html";
const TEST_TOP_PAGE_4 = TEST_DOMAIN_4 + TEST_PATH + "page.html";
const TEST_TOP_PAGE_5 = TEST_DOMAIN_5 + TEST_PATH + "page.html";
const TEST_TOP_PAGE_6 = TEST_DOMAIN_6 + TEST_PATH + "page.html";
const TEST_EMBEDDER_PAGE = TEST_DOMAIN + TEST_PATH + "embedder.html";
const TEST_POPUP_PAGE = TEST_DOMAIN + TEST_PATH + "popup.html";
const TEST_3RD_PARTY_PAGE = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_3RD_PARTY_PAGE_WO = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyWO.html";
const TEST_3RD_PARTY_PAGE_UI = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyUI.html";
const TEST_3RD_PARTY_PAGE_WITH_SVG = TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartySVG.html";
const TEST_4TH_PARTY_PAGE = TEST_4TH_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";
const TEST_ANOTHER_3RD_PARTY_PAGE = TEST_ANOTHER_3RD_PARTY_DOMAIN + TEST_PATH + "3rdParty.html";

const BEHAVIOR_ACCEPT         = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_LIMIT_FOREIGN  = Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN;
const BEHAVIOR_REJECT_FOREIGN = Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN;
const BEHAVIOR_REJECT_TRACKER = Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER;

var gFeatures = undefined;

let {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

requestLongerTimeout(3);

this.AntiTracking = {
  runTest(name, callbackTracking, callbackNonTracking, cleanupFunction, extraPrefs,
          windowOpenTest = true, userInteractionTest = true,
          expectedBlockingNotifications = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER,
          runInPrivateWindow = false, iframeSandbox = null, accessRemoval = null,
          callbackAfterRemoval = null) {
    // Here we want to test that a 3rd party context is simply blocked.
    this._createTask({
      name,
      cookieBehavior: BEHAVIOR_REJECT_TRACKER,
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
          blockingByContentBlockingRTUI: true,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlockingRTUI: false,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_ACCEPT,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_LIMIT_FOREIGN,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlockingRTUI: true,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_TRACKER,
          blockingByContentBlockingRTUI: false,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
          thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE,
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
                                        runInPrivateWindow, iframeSandbox, extraPrefs);
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

  async _setupTest(win, cookieBehavior, blockingByContentBlockingRTUI,
                   extraPrefs) {
    await SpecialPowers.flushPrefEnv();
    await SpecialPowers.pushPrefEnv({"set": [
      ["dom.storage_access.enabled", true],
      ["browser.contentblocking.allowlist.annotations.enabled", blockingByContentBlockingRTUI],
      ["browser.contentblocking.allowlist.storage.enabled", blockingByContentBlockingRTUI],
      ["network.cookie.cookieBehavior", cookieBehavior],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", cookieBehavior != BEHAVIOR_ACCEPT],
      [win.ContentBlocking.prefIntroCount, win.ContentBlocking.MAX_INTROS],
      ["privacy.restrict3rdpartystorage.userInteractionRequiredForHosts", "tracking.example.com,tracking.example.org"],
    ]});

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({"set": extraPrefs });
    }

    await UrlClassifierTestUtils.addTestTrackers();
  },

  _createTask(options) {
    add_task(async function() {
      info("Starting " + (options.cookieBehavior != BEHAVIOR_ACCEPT ? "blocking" : "non-blocking") + " cookieBehavior (" + options.cookieBehavior + ") and " +
                         (options.blockingByContentBlockingRTUI ? "" : "no") + " contentBlocking third-party cookies UI with" +
                         (options.allowList ? "" : "out") + " allow list test " + options.name +
                         " running in a " + (options.runInPrivateWindow ? "private" : "normal") + " window " +
                         " with iframe sandbox set to " + options.iframeSandbox +
                         " and access removal set to " + options.accessRemoval +
                         (typeof options.thirdPartyPage == "string" ? (
                            " and third party page set to " + options.thirdPartyPage) : ""));

      is(!!options.callbackAfterRemoval, !!options.accessRemoval,
         "callbackAfterRemoval must be passed when accessRemoval is non-null");

      let win = window;
      if (options.runInPrivateWindow) {
        win = OpenBrowserWindow({private: true});
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, options.cookieBehavior,
                                    options.blockingByContentBlockingRTUI,
                                    options.extraPrefs);

      let cookieBlocked = 0;
      let listener = {
        onContentBlockingEvent(webProgress, request, event) {
          if ((event & options.expectedBlockingNotifications)) {
            ++cookieBlocked;
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
                                  options.blockingByContentBlockingRTUI &&
                                  !options.allowList;
      let thirdPartyPage;
      if (typeof options.thirdPartyPage == "string") {
        thirdPartyPage = options.thirdPartyPage;
      } else {
        thirdPartyPage = TEST_3RD_PARTY_PAGE;
      }
      await ContentTask.spawn(browser,
                              { page: thirdPartyPage,
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

      is(!!cookieBlocked, !!options.expectedBlockingNotifications, "Checking cookie blocking notifications");

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

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, true, true, extraPrefs);

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

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },

  _createUserInteractionTask(name, blockingCallback, nonBlockingCallback,
                             runInPrivateWindow, iframeSandbox, extraPrefs) {
    add_task(async function() {
      info("Starting user-interaction test " + name);

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({private: true});
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, true, true, extraPrefs);

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

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },
};

/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

var gFeatures = undefined;

this.AntiTracking = {
  runTestInNormalAndPrivateMode(
    name,
    callbackTracking,
    callbackNonTracking,
    cleanupFunction,
    extraPrefs,
    windowOpenTest = true,
    userInteractionTest = true,
    expectedBlockingNotifications = Ci.nsIWebProgressListener
      .STATE_COOKIES_BLOCKED_TRACKER,
    iframeSandbox = null,
    accessRemoval = null,
    callbackAfterRemoval = null
  ) {
    // Normal mode
    this.runTest(
      name,
      callbackTracking,
      callbackNonTracking,
      cleanupFunction,
      extraPrefs,
      windowOpenTest,
      userInteractionTest,
      expectedBlockingNotifications,
      false,
      iframeSandbox,
      accessRemoval,
      callbackAfterRemoval
    );

    // Private mode
    this.runTest(
      name,
      callbackTracking,
      callbackNonTracking,
      cleanupFunction,
      extraPrefs,
      windowOpenTest,
      userInteractionTest,
      expectedBlockingNotifications,
      true,
      iframeSandbox,
      accessRemoval,
      callbackAfterRemoval
    );
  },

  runTest(
    name,
    callbackTracking,
    callbackNonTracking,
    cleanupFunction,
    extraPrefs,
    windowOpenTest = true,
    userInteractionTest = true,
    expectedBlockingNotifications = Ci.nsIWebProgressListener
      .STATE_COOKIES_BLOCKED_TRACKER,
    runInPrivateWindow = false,
    iframeSandbox = null,
    accessRemoval = null,
    callbackAfterRemoval = null
  ) {
    let runExtraTests = true;
    let options = {};
    if (typeof callbackNonTracking == "object" && !!callbackNonTracking) {
      options.callback = callbackNonTracking.callback;
      runExtraTests = callbackNonTracking.runExtraTests;
      if ("cookieBehavior" in callbackNonTracking) {
        options.cookieBehavior = callbackNonTracking.cookieBehavior;
      } else {
        options.cookieBehavior = BEHAVIOR_ACCEPT;
      }
      if ("blockingByAllowList" in callbackNonTracking) {
        options.blockingByAllowList = callbackNonTracking.blockingByAllowList;
      } else {
        options.blockingByAllowList = false;
      }
      if ("expectedBlockingNotifications" in callbackNonTracking) {
        options.expectedBlockingNotifications =
          callbackNonTracking.expectedBlockingNotifications;
      } else {
        options.expectedBlockingNotifications = 0;
      }
      callbackNonTracking = options.callback;
      options.accessRemoval = null;
      options.callbackAfterRemoval = null;
    }

    // Here we want to test that a 3rd party context is simply blocked.
    this._createTask({
      name,
      cookieBehavior: BEHAVIOR_REJECT_TRACKER,
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

    if (callbackNonTracking) {
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
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs,
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
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs,
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT,
          allowList: false,
          callback: callbackTracking,
          extraPrefs,
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
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs,
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
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs,
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
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs,
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
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs,
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
          thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE,
        });
        this._createCleanupTask(cleanupFunction);
      } else {
        // This is only used for imageCacheWorker.js tests
        this._createTask({
          name,
          cookieBehavior: options.cookieBehavior,
          allowList: options.blockingByAllowList,
          callback: options.callback,
          extraPrefs,
          expectedBlockingNotifications: options.expectedBlockingNotifications,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: options.accessRemoval,
          callbackAfterRemoval: options.callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 2: Here we want to test that a third-party context doesn't
      // get blocked with when the same origin is opened through window.open().
      if (windowOpenTest) {
        this._createWindowOpenTask(
          name,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          extraPrefs
        );
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 3: Here we want to test that a third-party context doesn't
      // get blocked with user interaction present
      if (userInteractionTest) {
        this._createUserInteractionTask(
          name,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          extraPrefs
        );
        this._createCleanupTask(cleanupFunction);
      }
    }
  },

  async interactWithTracker() {
    let windowClosed = new Promise(resolve => {
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

    info("Let's interact with the tracker");
    window.open(TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyOpenUI.html");
    await windowClosed;
  },

  async _setupTest(win, cookieBehavior, extraPrefs) {
    await SpecialPowers.flushPrefEnv();
    await SpecialPowers.pushPrefEnv({
      set: [
        ["dom.storage_access.enabled", true],
        ["network.cookie.cookieBehavior", cookieBehavior],
        ["privacy.trackingprotection.enabled", false],
        ["privacy.trackingprotection.pbmode.enabled", false],
        [
          "privacy.trackingprotection.annotate_channels",
          cookieBehavior != BEHAVIOR_ACCEPT,
        ],
        [
          "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
          "tracking.example.com,tracking.example.org",
        ],
      ],
    });

    if (extraPrefs && Array.isArray(extraPrefs) && extraPrefs.length) {
      await SpecialPowers.pushPrefEnv({ set: extraPrefs });

      for (let item of extraPrefs) {
        // When setting up skip URLs, we need to wait to ensure our prefs
        // actually take effect.  In order to do this, we set up a skip list
        // observer and wait until it calls us back.
        if (item[0] == "urlclassifier.trackingAnnotationSkipURLs") {
          info("Waiting for the skip list service to initialize...");
          let classifier = Cc[
            "@mozilla.org/url-classifier/dbservice;1"
          ].getService(Ci.nsIURIClassifier);
          let feature = classifier.getFeatureByName("tracking-annotation");
          await TestUtils.waitForCondition(
            () => feature.skipHostList == item[1].toLowerCase(),
            "Skip list service initialized"
          );
          break;
        }
      }
    }

    await UrlClassifierTestUtils.addTestTrackers();
  },

  _createTask(options) {
    add_task(async function() {
      info(
        "Starting " +
          (options.cookieBehavior != BEHAVIOR_ACCEPT
            ? "blocking"
            : "non-blocking") +
          " cookieBehavior (" +
          options.cookieBehavior +
          ") with" +
          (options.allowList ? "" : "out") +
          " allow list test " +
          options.name +
          " running in a " +
          (options.runInPrivateWindow ? "private" : "normal") +
          " window " +
          " with iframe sandbox set to " +
          options.iframeSandbox +
          " and access removal set to " +
          options.accessRemoval +
          (typeof options.thirdPartyPage == "string"
            ? " and third party page set to " + options.thirdPartyPage
            : "") +
          (typeof options.topPage == "string"
            ? " and top page set to " + options.topPage
            : "")
      );

      is(
        !!options.callbackAfterRemoval,
        !!options.accessRemoval,
        "callbackAfterRemoval must be passed when accessRemoval is non-null"
      );

      let win = window;
      if (options.runInPrivateWindow) {
        win = OpenBrowserWindow({ private: true });
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(
        win,
        options.cookieBehavior,
        options.extraPrefs
      );

      let cookieBlocked = 0;
      let listener = {
        onContentBlockingEvent(webProgress, request, event) {
          if (event & options.expectedBlockingNotifications) {
            ++cookieBlocked;
          }
        },
      };
      win.gBrowser.addProgressListener(listener);

      let topPage;
      if (typeof options.topPage == "string") {
        topPage = options.topPage;
      } else {
        topPage = TEST_TOP_PAGE;
      }

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, topPage);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      if (options.allowList) {
        info("Disabling content blocking for this page");
        win.gProtectionsHandler.disableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      info("Creating a 3rd party content");
      let doAccessRemovalChecks =
        typeof options.accessRemoval == "string" &&
        options.cookieBehavior == BEHAVIOR_REJECT_TRACKER &&
        !options.allowList;
      let thirdPartyPage;
      if (typeof options.thirdPartyPage == "string") {
        thirdPartyPage = options.thirdPartyPage;
      } else {
        thirdPartyPage = TEST_3RD_PARTY_PAGE;
      }
      let id = await ContentTask.spawn(
        browser,
        {
          page: thirdPartyPage,
          nextPage: TEST_4TH_PARTY_PAGE,
          callback: options.callback.toString(),
          callbackAfterRemoval: options.callbackAfterRemoval
            ? options.callbackAfterRemoval.toString()
            : null,
          accessRemoval: options.accessRemoval,
          iframeSandbox: options.iframeSandbox,
          allowList: options.allowList,
          doAccessRemovalChecks,
        },
        async function(obj) {
          let id = "id" + Math.random();
          await new content.Promise(resolve => {
            let ifr = content.document.createElement("iframe");
            ifr.id = id;
            ifr.onload = function() {
              info("Sending code to the 3rd party content");
              let callback = obj.allowList + "!!!" + obj.callback;
              ifr.contentWindow.postMessage(callback, "*");
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
              case "navigate-topframe":
                // pass-through
                break;
              default:
                ok(
                  false,
                  "Unexpected accessRemoval code passed: " + obj.accessRemoval
                );
                break;
            }
          }

          return id;
        }
      );

      if (
        doAccessRemovalChecks &&
        options.accessRemoval == "navigate-topframe"
      ) {
        await BrowserTestUtils.loadURI(browser, TEST_4TH_PARTY_PAGE);
        await BrowserTestUtils.browserLoaded(browser);

        let pageshow = BrowserTestUtils.waitForContentEvent(
          tab.linkedBrowser,
          "pageshow"
        );
        gBrowser.goBack();
        await pageshow;

        await ContentTask.spawn(
          browser,
          {
            id,
            callbackAfterRemoval: options.callbackAfterRemoval
              ? options.callbackAfterRemoval.toString()
              : null,
          },
          async function(obj) {
            let ifr = content.document.getElementById(obj.id);
            ifr.contentWindow.postMessage(obj.callbackAfterRemoval, "*");

            content.addEventListener("message", function msg(event) {
              if (event.data.type == "finish") {
                content.removeEventListener("message", msg);
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
          }
        );
      }

      if (options.allowList) {
        info("Enabling content blocking for this page");
        win.gProtectionsHandler.enableForCurrentPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      win.gBrowser.removeProgressListener(listener);

      is(
        !!cookieBlocked,
        !!options.expectedBlockingNotifications,
        "Checking cookie blocking notifications"
      );

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

  _createWindowOpenTask(
    name,
    blockingCallback,
    nonBlockingCallback,
    runInPrivateWindow,
    iframeSandbox,
    extraPrefs
  ) {
    add_task(async function() {
      info("Starting window-open test " + name);

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({ private: true });
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, extraPrefs);

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
      await ContentTask.spawn(
        browser,
        {
          page: pageURL,
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
        }
      );

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },

  _createUserInteractionTask(
    name,
    blockingCallback,
    nonBlockingCallback,
    runInPrivateWindow,
    iframeSandbox,
    extraPrefs
  ) {
    add_task(async function() {
      info("Starting user-interaction test " + name);

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({ private: true });
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      await AntiTracking._setupTest(win, BEHAVIOR_REJECT_TRACKER, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await ContentTask.spawn(
        browser,
        {
          page: TEST_3RD_PARTY_PAGE_UI,
          popup: TEST_POPUP_PAGE,
          blockingCallback: blockingCallback.toString(),
          iframeSandbox,
        },
        async function(obj) {
          let ifr = content.document.createElement("iframe");
          let loading = new content.Promise(resolve => {
            ifr.onload = resolve;
          });
          if (typeof obj.iframeSandbox == "string") {
            ifr.setAttribute("sandbox", obj.iframeSandbox);
          }
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
            ifr.contentWindow.postMessage(
              { callback: obj.blockingCallback },
              "*"
            );
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

          info(
            "First time, the 3rd party content should not have access to first party storage " +
              "because the tracker did not have user interaction"
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
            ifr.contentWindow.postMessage(
              { callback: obj.blockingCallback },
              "*"
            );
          });
        }
      );

      await AntiTracking.interactWithTracker();

      await ContentTask.spawn(
        browser,
        {
          page: TEST_3RD_PARTY_PAGE_UI,
          popup: TEST_POPUP_PAGE,
          nonBlockingCallback: nonBlockingCallback.toString(),
          iframeSandbox,
        },
        async function(obj) {
          let ifr = content.document.createElement("iframe");
          let loading = new content.Promise(resolve => {
            ifr.onload = resolve;
          });
          if (typeof obj.iframeSandbox == "string") {
            ifr.setAttribute("sandbox", obj.iframeSandbox);
          }
          content.document.body.appendChild(ifr);
          ifr.src = obj.page;
          await loading;

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

          info(
            "The 3rd party content should now have access to first party storage."
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
            ifr.contentWindow.postMessage(
              { callback: obj.nonBlockingCallback },
              "*"
            );
          });
        }
      );

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },
};

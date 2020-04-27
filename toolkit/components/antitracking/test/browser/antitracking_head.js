/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

var gFeatures = undefined;
var gTestTrackersCleanedUp = false;
var gTestTrackersCleanupRegistered = false;

/**
 * Force garbage collection.
 */
function forceGC() {
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
}

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
      if ("expectedBlockingNotifications" in callbackNonTracking) {
        options.expectedBlockingNotifications =
          callbackNonTracking.expectedBlockingNotifications;
      } else {
        options.expectedBlockingNotifications = 0;
      }
      if ("blockingByAllowList" in callbackNonTracking) {
        options.blockingByAllowList = callbackNonTracking.blockingByAllowList;
        if (options.blockingByAllowList) {
          // If we're on the allow list, there won't be any blocking!
          options.expectedBlockingNotifications = 0;
        }
      } else {
        options.blockingByAllowList = false;
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
          expectedBlockingNotifications: expectedBlockingNotifications
            ? Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL
            : 0,
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
          name: name + " reject foreign without exception",
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [
            ["network.cookie.rejectForeignWithExceptions.enabled", false],
            ...(extraPrefs || []),
          ],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name: name + " reject foreign with exception",
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          allowList: true,
          callback: callbackNonTracking,
          extraPrefs: [
            ["network.cookie.rejectForeignWithExceptions.enabled", true],
            ...(extraPrefs || []),
          ],
          expectedBlockingNotifications: 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          allowList: false,
          callback: callbackTracking,
          extraPrefs: [
            ["network.cookie.rejectForeignWithExceptions.enabled", false],
            ...(extraPrefs || []),
          ],
          expectedBlockingNotifications: expectedBlockingNotifications
            ? Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN
            : 0,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval,
          callbackAfterRemoval,
        });
        this._createCleanupTask(cleanupFunction);

        this._createTask({
          name,
          cookieBehavior: BEHAVIOR_REJECT_FOREIGN,
          allowList: false,
          callback: callbackNonTracking,
          extraPrefs: [
            ["network.cookie.rejectForeignWithExceptions.enabled", true],
            ...(extraPrefs || []),
          ],
          expectedBlockingNotifications: false,
          runInPrivateWindow,
          iframeSandbox,
          accessRemoval: null, // only passed with non-blocking callback
          callbackAfterRemoval: null,
          thirdPartyPage: TEST_ANOTHER_3RD_PARTY_PAGE,
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
          BEHAVIOR_REJECT_TRACKER,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          extraPrefs
        );
        this._createCleanupTask(cleanupFunction);

        this._createWindowOpenTask(
          name,
          BEHAVIOR_REJECT_FOREIGN,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          [
            ["network.cookie.rejectForeignWithExceptions.enabled", true],
            ...(extraPrefs || []),
          ]
        );
        this._createCleanupTask(cleanupFunction);
      }

      // Phase 3: Here we want to test that a third-party context doesn't
      // get blocked with user interaction present
      if (userInteractionTest) {
        this._createUserInteractionTask(
          name,
          BEHAVIOR_REJECT_TRACKER,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          extraPrefs
        );
        this._createCleanupTask(cleanupFunction);

        this._createUserInteractionTask(
          name,
          BEHAVIOR_REJECT_FOREIGN,
          callbackTracking,
          callbackNonTracking,
          runInPrivateWindow,
          iframeSandbox,
          [
            ["network.cookie.rejectForeignWithExceptions.enabled", true],
            ...(extraPrefs || []),
          ]
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
    window.open(
      TEST_3RD_PARTY_DOMAIN + TEST_PATH + "3rdPartyOpenUI.html",
      "",
      "noopener"
    );
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
        ["privacy.restrict3rdpartystorage.console.lazy", false],
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
          await TestUtils.waitForCondition(() => {
            for (let x of item[1].toLowerCase().split(",")) {
              if (feature.skipHostList.split(",").includes(x)) {
                return true;
              }
            }
            return false;
          }, "Skip list service initialized");
          break;
        }
      }
    }

    await UrlClassifierTestUtils.addTestTrackers();
    if (!gTestTrackersCleanupRegistered) {
      registerCleanupFunction(_ => {
        if (gTestTrackersCleanedUp) {
          return;
        }
        UrlClassifierTestUtils.cleanupTestTrackers();
        gTestTrackersCleanedUp = true;
      });
      gTestTrackersCleanupRegistered = true;
    }
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

      let topPage;
      if (typeof options.topPage == "string") {
        topPage = options.topPage;
      } else {
        topPage = TEST_TOP_PAGE;
      }

      let thirdPartyPage, thirdPartyDomainURI;
      if (typeof options.thirdPartyPage == "string") {
        thirdPartyPage = options.thirdPartyPage;
        let url = new URL(thirdPartyPage);
        thirdPartyDomainURI = Services.io.newURI(url.origin);
      } else {
        thirdPartyPage = TEST_3RD_PARTY_PAGE;
        thirdPartyDomainURI = Services.io.newURI(TEST_3RD_PARTY_DOMAIN);
      }

      // It's possible that the third-party domain has been whitelisted through
      // extraPrefs, so let's try annotating it here and adjust our blocking
      // expectations as necessary.
      if (
        options.expectedBlockingNotifications ==
        Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER
      ) {
        if (
          !(await AntiTracking._isThirdPartyPageClassifiedAsTracker(
            topPage,
            thirdPartyDomainURI
          ))
        ) {
          options.expectedBlockingNotifications = 0;
        }
      }

      let cookieBlocked = 0;
      let listener = {
        onContentBlockingEvent(webProgress, request, event) {
          if (event & options.expectedBlockingNotifications) {
            ++cookieBlocked;
          }
        },
      };
      function prepareTestEnvironmentOnPage() {
        win.gBrowser.addProgressListener(listener);

        Services.console.reset();
      }

      if (!options.allowList) {
        prepareTestEnvironmentOnPage();
      }

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, topPage);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      if (options.allowList) {
        info("Disabling content blocking for this page");
        win.gProtectionsHandler.disableForCurrentPage();

        prepareTestEnvironmentOnPage();

        // The previous function reloads the browser, so wait for it to load again!
        await BrowserTestUtils.browserLoaded(browser);
      }

      info("Creating a 3rd party content");
      let doAccessRemovalChecks =
        typeof options.accessRemoval == "string" &&
        options.cookieBehavior == BEHAVIOR_REJECT_TRACKER &&
        !options.allowList;
      let id = await SpecialPowers.spawn(
        browser,
        [
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
        ],
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
                break;
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

        await SpecialPowers.spawn(
          browser,
          [
            {
              id,
              callbackAfterRemoval: options.callbackAfterRemoval
                ? options.callbackAfterRemoval.toString()
                : null,
            },
          ],
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

      let allMessages = Services.console.getMessageArray().filter(msg => {
        try {
          // Select all messages that the anti-tracking backend could generate.
          return msg
            .QueryInterface(Ci.nsIScriptError)
            .category.startsWith("cookieBlocked");
        } catch (e) {
          return false;
        }
      });
      let expectedCategory = "";
      // When changing this list, please make sure to update the corresponding
      // code in ReportBlockingToConsole().
      switch (options.expectedBlockingNotifications) {
        case Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_BY_PERMISSION:
          expectedCategory = "cookieBlockedPermission";
          break;
        case Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER:
          expectedCategory = "cookieBlockedTracker";
          break;
        case Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_ALL:
          expectedCategory = "cookieBlockedAll";
          break;
        case Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_FOREIGN:
          expectedCategory = "cookieBlockedForeign";
          break;
      }

      if (expectedCategory == "") {
        is(allMessages.length, 0, "No console messages should be generated");
      } else {
        ok(!!allMessages.length, "Some console message should be generated");
        if (options.errorMessageDomains) {
          is(
            allMessages.length,
            options.errorMessageDomains.length,
            "Enough items provided in errorMessageDomains"
          );
        }
      }
      let index = 0;
      for (let msg of allMessages) {
        is(
          msg.category,
          expectedCategory,
          "Message should be of expected category"
        );

        if (options.errorMessageDomains) {
          ok(
            msg.errorMessage.includes(options.errorMessageDomains[index]),
            `Error message domain ${options.errorMessageDomains[index]} (${index}) found in "${msg.errorMessage}"`
          );
          index++;
        }
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

      // While running these tests we typically do not have enough idle time to do
      // GC reliably, so force it here.
      forceGC();
    });
  },

  _createWindowOpenTask(
    name,
    cookieBehavior,
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

      await AntiTracking._setupTest(win, cookieBehavior, extraPrefs);

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
      await SpecialPowers.spawn(
        browser,
        [
          {
            page: pageURL,
            blockingCallback: blockingCallback.toString(),
            nonBlockingCallback: nonBlockingCallback.toString(),
            iframeSandbox,
          },
        ],
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

      await AntiTracking._maybeDoSubIframeTest(
        browser,
        cookieBehavior,
        blockingCallback,
        iframeSandbox
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
    cookieBehavior,
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

      await AntiTracking._setupTest(win, cookieBehavior, extraPrefs);

      info("Creating a new tab");
      let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab;

      let browser = win.gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      info("Creating a 3rd party content");
      await SpecialPowers.spawn(
        browser,
        [
          {
            page: TEST_3RD_PARTY_PAGE_UI,
            popup: TEST_POPUP_PAGE,
            blockingCallback: blockingCallback.toString(),
            iframeSandbox,
          },
        ],
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

      await SpecialPowers.spawn(
        browser,
        [
          {
            page: TEST_3RD_PARTY_PAGE_UI,
            popup: TEST_POPUP_PAGE,
            nonBlockingCallback: nonBlockingCallback.toString(),
            iframeSandbox,
          },
        ],
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

      await AntiTracking._maybeDoSubIframeTest(
        browser,
        cookieBehavior,
        blockingCallback,
        iframeSandbox
      );

      info("Removing the tab");
      BrowserTestUtils.removeTab(tab);

      if (runInPrivateWindow) {
        win.close();
      }
    });
  },

  async _maybeDoSubIframeTest(
    browser,
    cookieBehavior,
    blockingCallback,
    iframeSandbox
  ) {
    // We would do an additional test for sub-iframe if the cookie behavior is
    // BEHAVIOR_REJECT_TRACKER. The sub-iframes shouldn't get the the storage
    // access even they have the storage permission.
    if (cookieBehavior !== BEHAVIOR_REJECT_TRACKER) {
      return;
    }

    info("Create a first-level iframe to test sub iframes.");
    let iframeBrowsingContext = await SpecialPowers.spawn(
      browser,
      [{ page: TEST_IFRAME_PAGE }],
      async function(obj) {
        // Add an iframe.
        let ifr = content.document.createElement("iframe");
        let loading = new content.Promise(resolve => {
          ifr.onload = resolve;
        });
        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
        await loading;

        return ifr.browsingContext;
      }
    );

    info("Create a second-level 3rd party content which should be blocked");
    await SpecialPowers.spawn(
      iframeBrowsingContext,
      [
        {
          page: TEST_3RD_PARTY_PAGE_UI,
          blockingCallback: blockingCallback.toString(),
          iframeSandbox,
        },
      ],
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
  },

  async _isThirdPartyPageClassifiedAsTracker(topPage, thirdPartyDomainURI) {
    let channel;
    await new Promise((resolve, reject) => {
      channel = NetUtil.newChannel({
        uri: thirdPartyDomainURI,
        loadingPrincipal: Services.scriptSecurityManager.createContentPrincipal(
          thirdPartyDomainURI,
          {}
        ),
        securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
        contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
      });

      channel
        .QueryInterface(Ci.nsIHttpChannelInternal)
        .setTopWindowURIIfUnknown(Services.io.newURI(topPage));

      function Listener() {}
      Listener.prototype = {
        onStartRequest(request) {},
        onDataAvailable(request, stream, off, cnt) {},
        onStopRequest(request, st) {
          let status = request.QueryInterface(Ci.nsIHttpChannel).responseStatus;
          if (status == 200) {
            resolve();
          } else {
            reject();
          }
        },
      };
      let listener = new Listener();
      channel.asyncOpen(listener);
    });

    return !!(
      channel.QueryInterface(Ci.nsIClassifiedChannel).classificationFlags &
      Ci.nsIClassifiedChannel.CLASSIFIED_ANY_BASIC_TRACKING
    );
  },
};

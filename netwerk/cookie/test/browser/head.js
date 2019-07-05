const BEHAVIOR_ACCEPT = Ci.nsICookieService.BEHAVIOR_ACCEPT;
const BEHAVIOR_REJECT = Ci.nsICookieService.BEHAVIOR_REJECT;

const PERM_DEFAULT = Ci.nsICookiePermission.ACCESS_DEFAULT;
const PERM_ALLOW = Ci.nsICookiePermission.ACCESS_ALLOW;
const PERM_DENY = Ci.nsICookiePermission.ACCESS_DENY;

const TEST_DOMAIN = "https://example.com/";
const TEST_PATH = "browser/netwerk/cookie/test/browser/";
const TEST_TOP_PAGE = TEST_DOMAIN + TEST_PATH + "file_empty.html";

// Helper to eval() provided cookieJarAccessAllowed and cookieJarAccessDenied
// toString()ed optionally async function in freshly created tabs with
// BEHAVIOR_ACCEPT and BEHAVIOR_REJECT configured, respectively, in a number of
// permutations. This includes verifying that changing the permission while the
// page is open still results in the state of the permission when the
// document/global was created still applying. Code will execute in the
// ContentTask.spawn frame-script context, use content to access the underlying
// page.
this.CookiePolicyHelper = {
  runTest(testName, config) {
    // Testing allowed to blocked by cookie behavior
    this._createTest(
      testName,
      config.cookieJarAccessAllowed,
      config.cookieJarAccessDenied,
      config.prefs,
      {
        fromBehavior: BEHAVIOR_ACCEPT,
        toBehavior: BEHAVIOR_REJECT,
        fromPermission: PERM_DEFAULT,
        toPermission: PERM_DEFAULT,
      }
    );

    // Testing blocked to allowed by cookie behavior
    this._createTest(
      testName,
      config.cookieJarAccessDenied,
      config.cookieJarAccessAllowed,
      config.prefs,
      {
        fromBehavior: BEHAVIOR_REJECT,
        toBehavior: BEHAVIOR_ACCEPT,
        fromPermission: PERM_DEFAULT,
        toPermission: PERM_DEFAULT,
      }
    );

    // Testing allowed to blocked by cookie permission
    this._createTest(
      testName,
      config.cookieJarAccessAllowed,
      config.cookieJarAccessDenied,
      config.prefs,
      {
        fromBehavior: BEHAVIOR_REJECT,
        toBehavior: BEHAVIOR_REJECT,
        fromPermission: PERM_ALLOW,
        toPermission: PERM_DEFAULT,
      }
    );

    // Testing blocked to allowed by cookie permission
    this._createTest(
      testName,
      config.cookieJarAccessDenied,
      config.cookieJarAccessAllowed,
      config.prefs,
      {
        fromBehavior: BEHAVIOR_ACCEPT,
        toBehavior: BEHAVIOR_ACCEPT,
        fromPermission: PERM_DENY,
        toPermission: PERM_DEFAULT,
      }
    );
  },

  _createTest(testName, goodCb, badCb, prefs, config) {
    add_task(async _ => {
      info("Starting " + testName + ": " + config.toSource());

      await SpecialPowers.flushPrefEnv();

      if (prefs) {
        await SpecialPowers.pushPrefEnv({ set: prefs });
      }

      let uri = Services.io.newURI(TEST_DOMAIN);

      // Let's set the first cookie pref.
      Services.perms.add(uri, "cookie", config.fromPermission);
      await SpecialPowers.pushPrefEnv({
        set: [["network.cookie.cookieBehavior", config.fromBehavior]],
      });

      // Let's open a tab and load content.
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      let browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      // Let's create an iframe.
      await ContentTask.spawn(browser, { url: TEST_TOP_PAGE }, async obj => {
        return new content.Promise(resolve => {
          let ifr = content.document.createElement("iframe");
          ifr.setAttribute("id", "iframe");
          ifr.src = obj.url;
          ifr.onload = resolve;
          content.document.body.appendChild(ifr);
        });
      });

      // Let's exec the "good" callback.
      info(
        "Executing the test after setting the cookie behavior to " +
          config.fromBehavior +
          " and permission to " +
          config.fromPermission
      );
      await ContentTask.spawn(
        browser,
        { callback: goodCb.toString() },
        async obj => {
          let runnableStr = `(() => {return (${obj.callback});})();`;
          let runnable = eval(runnableStr); // eslint-disable-line no-eval
          await runnable(content);

          let ifr = content.document.getElementById("iframe");
          await runnable(ifr.contentWindow);
        }
      );

      // Now, let's change the cookie settings
      Services.perms.add(uri, "cookie", config.toPermission);
      await SpecialPowers.pushPrefEnv({
        set: [["network.cookie.cookieBehavior", config.toBehavior]],
      });

      // We still want the good callback to succeed.
      info(
        "Executing the test after setting the cookie behavior to " +
          config.toBehavior +
          " and permission to " +
          config.toPermission
      );
      await ContentTask.spawn(
        browser,
        { callback: goodCb.toString() },
        async obj => {
          let runnableStr = `(() => {return (${obj.callback});})();`;
          let runnable = eval(runnableStr); // eslint-disable-line no-eval
          await runnable(content);

          let ifr = content.document.getElementById("iframe");
          await runnable(ifr.contentWindow);
        }
      );

      // Let's close the tab.
      BrowserTestUtils.removeTab(tab);

      // Let's open a new tab and load content again.
      tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
      gBrowser.selectedTab = tab;

      browser = gBrowser.getBrowserForTab(tab);
      await BrowserTestUtils.browserLoaded(browser);

      // Let's exec the "bad" callback.
      info("Executing the test in a new tab");
      await ContentTask.spawn(
        browser,
        { callback: badCb.toString() },
        async obj => {
          let runnableStr = `(() => {return (${obj.callback});})();`;
          let runnable = eval(runnableStr); // eslint-disable-line no-eval
          await runnable(content);
        }
      );

      // Let's close the tab.
      BrowserTestUtils.removeTab(tab);

      // Cleanup.
      await new Promise(resolve => {
        Services.clearData.deleteData(
          Ci.nsIClearDataService.CLEAR_ALL,
          resolve
        );
      });
    });
  },
};

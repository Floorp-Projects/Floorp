/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

/* import-globals-from dynamicfpi_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/dynamicfpi_head.js",
  this
);

/* import-globals-from storageprincipal_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storageprincipal_head.js",
  this
);

this.PartitionedStorageHelper = {
  runTestInNormalAndPrivateMode(name, callback, cleanupFunction, extraPrefs) {
    // Normal mode
    this.runTest(name, callback, cleanupFunction, extraPrefs, false);

    // Private mode
    this.runTest(name, callback, cleanupFunction, extraPrefs, true);
  },

  runTest(
    name,
    callback,
    cleanupFunction,
    extraPrefs,
    runInPrivateWindow = false
  ) {
    DynamicFPIHelper.runTest(
      name,
      callback,
      cleanupFunction,
      extraPrefs,
      runInPrivateWindow
    );
    StoragePrincipalHelper.runTest(
      name,
      callback,
      cleanupFunction,
      extraPrefs,
      runInPrivateWindow
    );
  },

  runPartitioningTestInNormalAndPrivateMode(
    name,
    testCategory,
    getDataCallback,
    addDataCallback,
    cleanupFunction
  ) {
    // Normal mode
    this.runPartitioningTest(
      name,
      testCategory,
      getDataCallback,
      addDataCallback,
      cleanupFunction,
      false
    );

    // Private mode
    this.runPartitioningTest(
      name,
      testCategory,
      getDataCallback,
      addDataCallback,
      cleanupFunction,
      true
    );
  },

  runPartitioningTest(
    name,
    testCategory,
    getDataCallback,
    addDataCallback,
    cleanupFunction,
    runInPrivateWindow = false
  ) {
    for (let variant of ["normal", "initial-aboutblank"]) {
      for (let limitForeignContexts of [false, true]) {
        this.runPartitioningTestInner(
          name,
          testCategory,
          getDataCallback,
          addDataCallback,
          cleanupFunction,
          variant,
          runInPrivateWindow,
          limitForeignContexts
        );
      }
    }
  },

  runPartitioningTestInner(
    name,
    testCategory,
    getDataCallback,
    addDataCallback,
    cleanupFunction,
    variant,
    runInPrivateWindow,
    limitForeignContexts
  ) {
    add_task(async _ => {
      info(
        "Starting test `" +
          name +
          "' testCategory `" +
          testCategory +
          "' variant `" +
          variant +
          "' in a " +
          (runInPrivateWindow ? "private" : "normal") +
          " window " +
          (limitForeignContexts ? "with" : "without") +
          " limitForeignContexts to check that 2 tabs are correctly partititioned"
      );

      await SpecialPowers.flushPrefEnv();
      await SpecialPowers.pushPrefEnv({
        set: [
          ["dom.storage_access.enabled", true],
          [
            "network.cookie.cookieBehavior",
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
          ],
          ["privacy.dynamic_firstparty.limitForeign", limitForeignContexts],
          ["privacy.trackingprotection.enabled", false],
          ["privacy.trackingprotection.pbmode.enabled", false],
          ["privacy.trackingprotection.annotate_channels", true],
          ["privacy.storagePrincipal.enabledForTrackers", false],
          [
            "privacy.restrict3rdpartystorage.userInteractionRequiredForHosts",
            "not-tracking.example.com",
          ],
        ],
      });

      let win = window;
      if (runInPrivateWindow) {
        win = OpenBrowserWindow({ private: true });
        await TestUtils.topicObserved("browser-delayed-startup-finished");
      }

      info("Creating the first tab");
      let tab1 = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab1;

      let browser1 = win.gBrowser.getBrowserForTab(tab1);
      await BrowserTestUtils.browserLoaded(browser1);

      info("Creating the second tab");
      let tab2 = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE_6);
      win.gBrowser.selectedTab = tab2;

      let browser2 = win.gBrowser.getBrowserForTab(tab2);
      await BrowserTestUtils.browserLoaded(browser2);

      info("Creating the third tab");
      let tab3 = BrowserTestUtils.addTab(
        win.gBrowser,
        TEST_4TH_PARTY_PARTITIONED_PAGE
      );
      win.gBrowser.selectedTab = tab3;

      let browser3 = win.gBrowser.getBrowserForTab(tab3);
      await BrowserTestUtils.browserLoaded(browser3);

      // Use the same URL as first tab to check partitioned data
      info("Creating the forth tab");
      let tab4 = BrowserTestUtils.addTab(win.gBrowser, TEST_TOP_PAGE);
      win.gBrowser.selectedTab = tab4;

      let browser4 = win.gBrowser.getBrowserForTab(tab4);
      await BrowserTestUtils.browserLoaded(browser4);

      async function getDataFromThirdParty(browser, result) {
        // Overwrite the special case here since third party cookies are not
        // avilable when `limitForeignContexts` is enabled.
        if (testCategory === "cookies" && limitForeignContexts) {
          info("overwrite result to empty");
          result = "";
        }

        await SpecialPowers.spawn(
          browser,
          [
            {
              page: TEST_4TH_PARTY_PARTITIONED_PAGE + "?variant=" + variant,
              getDataCallback: getDataCallback.toString(),
              result,
            },
          ],
          async obj => {
            await new content.Promise(resolve => {
              let ifr = content.document.createElement("iframe");
              ifr.onload = __ => {
                info("Sending code to the 3rd party content");
                ifr.contentWindow.postMessage({ cb: obj.getDataCallback }, "*");
              };

              content.addEventListener(
                "message",
                function msg(event) {
                  is(
                    event.data,
                    obj.result,
                    "Partitioned cookie jar has value: " + obj.result
                  );
                  resolve();
                },
                { once: true }
              );

              content.document.body.appendChild(ifr);
              ifr.src = obj.page;
            });
          }
        );
      }

      async function getDataFromFirstParty(browser, result) {
        await SpecialPowers.spawn(
          browser,
          [
            {
              getDataCallback: getDataCallback.toString(),
              result,
              variant,
            },
          ],
          async obj => {
            let runnableStr = `(() => {return (${obj.getDataCallback});})();`;
            let runnable = eval(runnableStr); // eslint-disable-line no-eval
            let win = content;
            if (obj.variant == "initial-aboutblank") {
              let i = win.document.createElement("iframe");
              i.src = "about:blank";
              win.document.body.appendChild(i);
              // override win to make it point to the initial about:blank window
              win = i.contentWindow;
            }

            let result = await runnable.call(content, win);
            is(
              result,
              obj.result,
              "Partitioned cookie jar is empty: " + obj.result
            );
          }
        );
      }

      info("Checking 3rd party has an empty cookie jar in first tab");
      await getDataFromThirdParty(browser1, "");

      info("Checking 3rd party has an empty cookie jar in second tab");
      await getDataFromThirdParty(browser2, "");

      info("Checking first party has an empty cookie jar in third tab");
      await getDataFromFirstParty(browser3, "");

      info("Checking 3rd party has an empty cookie jar in forth tab");
      await getDataFromThirdParty(browser4, "");

      async function createDataInThirdParty(browser, value) {
        await SpecialPowers.spawn(
          browser,
          [
            {
              page: TEST_4TH_PARTY_PARTITIONED_PAGE + "?variant=" + variant,
              addDataCallback: addDataCallback.toString(),
              value,
            },
          ],
          async obj => {
            await new content.Promise(resolve => {
              let ifr = content.document.getElementsByTagName("iframe")[0];
              content.addEventListener(
                "message",
                function msg(event) {
                  ok(event.data, "Data created");
                  resolve();
                },
                { once: true }
              );

              ifr.contentWindow.postMessage(
                {
                  cb: obj.addDataCallback,
                  value: obj.value,
                },
                "*"
              );
            });
          }
        );
      }

      async function createDataInFirstParty(browser, value) {
        await SpecialPowers.spawn(
          browser,
          [
            {
              addDataCallback: addDataCallback.toString(),
              value,
              variant,
            },
          ],
          async obj => {
            let runnableStr = `(() => {return (${obj.addDataCallback});})();`;
            let runnable = eval(runnableStr); // eslint-disable-line no-eval
            let win = content;
            if (obj.variant == "initial-aboutblank") {
              let i = win.document.createElement("iframe");
              i.src = "about:blank";
              win.document.body.appendChild(i);
              // override win to make it point to the initial about:blank window
              win = i.contentWindow;
            }

            let result = await runnable.call(content, win, obj.value);
            ok(result, "Data created");
          }
        );
      }

      info("Creating data in the first tab");
      await createDataInThirdParty(browser1, "A");

      info("Creating data in the second tab");
      await createDataInThirdParty(browser2, "B");

      // Before writing browser4, check data written by browser1
      info("First tab should still have just 'A'");
      await getDataFromThirdParty(browser1, "A");
      info("Forth tab should still have just 'A'");
      await getDataFromThirdParty(browser4, "A");

      // Ensure to create data in the forth tab before the third tab,
      // otherwise cookie will be written successfully due to prior cookie
      // of the base domain exists.
      info("Creating data in the forth tab");
      await createDataInThirdParty(browser4, "D");

      info("Creating data in the third tab");
      await createDataInFirstParty(browser3, "C");

      // read all tabs
      info("First tab should be changed to 'D'");
      await getDataFromThirdParty(browser1, "D");

      info("Second tab should still have just 'B'");
      await getDataFromThirdParty(browser2, "B");

      info("Third tab should still have just 'C'");
      await getDataFromFirstParty(browser3, "C");

      info("Forth tab should still have just 'D'");
      await getDataFromThirdParty(browser4, "D");

      info("Removing the tabs");
      BrowserTestUtils.removeTab(tab1);
      BrowserTestUtils.removeTab(tab2);
      BrowserTestUtils.removeTab(tab3);
      BrowserTestUtils.removeTab(tab4);

      if (runInPrivateWindow) {
        win.close();
      }
    });

    add_task(async _ => {
      info("Cleaning up.");
      if (cleanupFunction) {
        await cleanupFunction();
      }

      // While running these tests we typically do not have enough idle time to do
      // GC reliably, so force it here.
      /* import-globals-from antitracking_head.js */
      forceGC();
    });
  },
};

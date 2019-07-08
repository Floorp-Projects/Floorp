/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { FormHistory } = ChromeUtils.import(
  "resource://gre/modules/FormHistory.jsm"
);

/** Test for Bug 472396 **/
add_task(async function test() {
  // initialization
  let windowsToClose = [];
  let testURI =
    "http://example.com/tests/toolkit/components/satchel/test/subtst_privbrowsing.html";

  async function doTest(aShouldValueExist, aWindow) {
    let browser = aWindow.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(browser, testURI);
    await BrowserTestUtils.browserLoaded(browser);

    // Wait for the page to reload itself.
    await BrowserTestUtils.browserLoaded(browser);

    let count = 0;
    let doneCounting = {};
    doneCounting.promise = new Promise(
      resolve => (doneCounting.resolve = resolve)
    );
    FormHistory.count(
      { fieldname: "field", value: "value" },
      {
        handleResult(result) {
          count = result;
        },
        handleError(error) {
          Assert.ok(false, "Error occurred searching form history: " + error);
        },
        handleCompletion(num) {
          if (aShouldValueExist) {
            is(count, 1, "In non-PB mode, we add a single entry");
          } else {
            is(count, 0, "In PB mode, we don't add any entries");
          }

          doneCounting.resolve();
        },
      }
    );
    await doneCounting.promise;
  }

  function testOnWindow(aOptions, aCallback) {
    return BrowserTestUtils.openNewBrowserWindow(aOptions).then(win => {
      windowsToClose.push(win);
      return win;
    });
  }

  await testOnWindow({ private: true }).then(aWin => doTest(false, aWin));

  // Test when not on private mode after visiting a site on private
  // mode. The form history should not exist.
  await testOnWindow({}).then(aWin => doTest(true, aWin));

  await Promise.all(
    windowsToClose.map(win => BrowserTestUtils.closeWindow(win))
  );
});

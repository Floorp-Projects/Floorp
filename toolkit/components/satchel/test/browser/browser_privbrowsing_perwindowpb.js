/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var FormHistory = (Components.utils.import("resource://gre/modules/FormHistory.jsm", {})).FormHistory;

/** Test for Bug 472396 **/
add_task(function* test() {
  // initialization
  let windowsToClose = [];
  let testURI =
    "http://example.com/tests/toolkit/components/satchel/test/subtst_privbrowsing.html";

  function* doTest(aShouldValueExist, aWindow) {
    let browser = aWindow.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(browser, testURI);
    yield BrowserTestUtils.browserLoaded(browser);

    // Wait for the page to reload itself.
    yield BrowserTestUtils.browserLoaded(browser);

    let count = 0;
    let doneCounting = {};
    doneCounting.promise = new Promise(resolve => doneCounting.resolve = resolve);
    FormHistory.count({ fieldname: "field", value: "value" },
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
                        }
                      });
    yield doneCounting.promise;
  }

  function testOnWindow(aOptions, aCallback) {
    return BrowserTestUtils.openNewBrowserWindow(aOptions)
                           .then(win => { windowsToClose.push(win); return win; });
  }


  yield testOnWindow({private: true}).then((aWin) => {
    return Task.spawn(doTest(false, aWin));
  });

  // Test when not on private mode after visiting a site on private
  // mode. The form history should not exist.
  yield testOnWindow({}).then((aWin) => {
    return Task.spawn(doTest(true, aWin));
  });

  yield Promise.all(windowsToClose.map(win => BrowserTestUtils.closeWindow(win)));
});

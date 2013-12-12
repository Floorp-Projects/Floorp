/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let FormHistory = (Components.utils.import("resource://gre/modules/FormHistory.jsm", {})).FormHistory;

/** Test for Bug 472396 **/
function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI =
    "http://example.com/tests/toolkit/components/satchel/test/subtst_privbrowsing.html";

  function doTest(aIsPrivateMode, aShouldValueExist, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      if (aWindow.content.location != testURI) {
        aWindow.gBrowser.selectedBrowser.loadURI(testURI);
        return;
      }
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

      let checks = 0;
      function doneCheck() {
        checks++;
        if (checks == 2) {
          executeSoon(aCallback);
        }
      }

      // Wait for the second load of the page to call the callback,
      // because the first load submits the form and the page reloads after
      // the form submission.
      aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
        aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
        doneCheck();
      }, true);

      let count = 0;
      FormHistory.count({ fieldname: "field", value: "value" },
        { handleResult: function(result) {
            count = result;
          },
          handleError: function (error) {
            do_throw("Error occurred searching form history: " + error);
          },
          handleCompletion: function(num) {
            is(count >= 1, aShouldValueExist, "Checking value exists in form history");
            doneCheck();
          }
        });
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      executeSoon(function() aCallback(aWin));
    });
  };

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });


  testOnWindow({private: true}, function(aWin) {
    doTest(true, false, aWin, function() {
      // Test when not on private mode after visiting a site on private
      // mode. The form history should no exist.
      testOnWindow({}, function(aWin) {
        doTest(false, false, aWin, function() {
          finish();
        });
      });
    });
  });
}

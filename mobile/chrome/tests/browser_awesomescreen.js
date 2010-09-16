/*
 * Bug 436069 - Fennec browser-chrome tests to verify correct navigation into the
 *              differents part of the awesome panel
 */

let gTests = [];
let gCurrentTest = null;

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  setTimeout(runNextTest, 200);
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    finish();
  }
}

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar readonly state
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar readonly state",

  run: function() {
    is(BrowserUI._edit.readOnly, true, "urlbar input textbox should be readonly");

    let popup = document.getElementById("popup_autocomplete");
    popup.addEventListener("popupshown", function(aEvent) {
      popup.removeEventListener("popupshown", arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    BrowserUI.doCommand("cmd_openLocation");
  },

  onPopupReady: function() {
    is(Elements.urlbarState.getAttribute("mode"), "edit", "bcast_urlbarState mode attribute should be equal to 'edit'");
    is(BrowserUI._edit.readOnly, false, "urlbar input textbox should not be readonly once it is open");

    runNextTest();
  }
});

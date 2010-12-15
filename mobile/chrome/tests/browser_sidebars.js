let testURL_01 = chromeRoot + "browser_blank_01.html";

let newTabs = [];
let gCurrentTest = null;
let gTests = [];

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
    // Close the awesome panel just in case
    BrowserUI.activePanel = null;
    finish();
  }
}


//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();

  //Add new tab
  newTabs.push(Browser.addTab(testURL_01, true));
  let tabs = document.getElementById("tabs");
  ok(tabs._columnsCount == 1, "Tabs layout should be on one column");

  runNextTest();

}

function checkSidebars(aLeftVisible, aRightVisible) {
  let [leftVisibility, rightVisibility, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
  ok(Math.abs(leftVisibility - aLeftVisible) < 0.2, (leftWidth * aLeftVisible) + "px of the left sidebar should be visible");
  ok(Math.abs(rightVisibility - aRightVisible) < 0.2, (rightWidth * aRightVisible) + "px of the right sidebar should be visible");
}

function checkOnResize(aCallback) {
  let [leftVisibility, rightVisibility, leftWidth, rightWidth] = Browser.computeSidebarVisibility();

  window.addEventListener("resize", function() {
    window.removeEventListener("resize", arguments.callee, false);
    setTimeout(function() {
      checkSidebars(leftVisibility, rightVisibility);
      window.addEventListener("resize", function() {
        window.removeEventListener("resize", arguments.callee, false);
        setTimeout(function() {
          checkSidebars(leftVisibility, rightVisibility);
          aCallback();
        }, 0);
      }, false);
      window.resizeTo(800, 480);
    }, 0);
  }, false);
  window.resizeTo(480, 800);
}

gTests.push({
  desc: "Testing horizontal positionning of the sidebars for one column",

  run: function() {
    checkSidebars(0, 0);
    checkOnResize(gCurrentTest.checkLeftVisible);
  },

  checkLeftVisible: function() {
    Browser.controlsScrollboxScroller.scrollTo(0, 0);
    checkSidebars(1, 0);
    checkOnResize(gCurrentTest.checkRightVisible);
  },

  checkRightVisible: function() {
    let [,, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
    Browser.controlsScrollboxScroller.scrollTo(leftWidth + rightWidth, 0);
    checkSidebars(0, 1);
    checkOnResize(runNextTest);
  }
});


gTests.push({
  desc: "Testing horizontal positionning of the sidebars for multiple columns",

  run: function() {
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    newTabs.push(Browser.addTab(testURL_01, true));
    let tabs = document.getElementById("tabs");
    ok(tabs._columnsCount > 1, "Tabs layout should be on multiple columns");

    checkSidebars(0, 0);
    checkOnResize(gCurrentTest.checkLeftVisible);
  },

  checkLeftVisible: function() {
    Browser.controlsScrollboxScroller.scrollTo(0, 0);
    checkSidebars(1, 0);
    checkOnResize(gCurrentTest.checkRightVisible);
  },

  checkRightVisible: function() {
    let [,, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
    Browser.controlsScrollboxScroller.scrollTo(leftWidth + rightWidth, 0);
    checkSidebars(0, 1);
    checkOnResize(function() {
      Browser.hideSidebars();
      runNextTest();
    });
  }
});

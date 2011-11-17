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
    AwesomeScreen.activePanel = null;

    for (let iTab=0; iTab<newTabs.length; iTab++)
      Browser.closeTab(newTabs[iTab], { forceClose: true });

    finish();
  }
}

function waitForNavigationPanel(aCallback, aWaitForHide) {
  let evt = aWaitForHide ? "NavigationPanelHidden" : "NavigationPanelShown";
  info("waitFor " + evt + "(" + Components.stack.caller + ")");
  window.addEventListener(evt, function(aEvent) {
    info("receive " + evt);
    window.removeEventListener(aEvent.type, arguments.callee, false);
    Util.executeSoon(aCallback);
  }, false);
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
  let left = Math.abs(leftVisibility - aLeftVisible),
      right = Math.abs(rightVisibility - aRightVisible);
  ok(left < 0.2, (leftWidth * aLeftVisible) + "px of the left sidebar should be visible (got " + left + ")");
  ok(right < 0.2, (rightWidth * aRightVisible) + "px of the right sidebar should be visible (got " + right + ")");
}

function checkOnResize(aCallback) {
  let [leftVisibility, rightVisibility, leftWidth, rightWidth] = Browser.computeSidebarVisibility();
  let oldLeftWidth = leftWidth;

  Elements.browsers.addEventListener("SizeChanged", function() {
    Elements.browsers.removeEventListener("SizeChanged", arguments.callee, false);
    setTimeout(function() { Util.executeSoon(function() {
      checkSidebars(leftVisibility, rightVisibility);

      Elements.browsers.addEventListener("SizeChanged", function() {
        Elements.browsers.removeEventListener("SizeChanged", arguments.callee, false);

        setTimeout(function() {
          checkSidebars(leftVisibility, rightVisibility);
          let [, , newLeftWidth, newRightWidth] = Browser.computeSidebarVisibility();
          is(oldLeftWidth, newLeftWidth, "Size should be the same than the size before resizing");
          aCallback();
        }, 0);
      }, false);
      window.resizeTo(480, 800);
    }); }, 0)} 
  , false);

  window.resizeTo(800, 480);
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
    checkOnResize(gCurrentTest.onFinish);
  },

  onFinish: function() {
    Browser.hideSidebars();
    runNextTest();
  }
});

gTests.push({
  desc: "Testing horizontal positionning of the sidebars for multiple columns with awesome screen open",

  run: function() {
    let tabs = document.getElementById("tabs");
    ok(tabs._columnsCount > 1, "Tabs layout should be on multiple columns");

    checkSidebars(0, 0);
    waitForNavigationPanel(function() {
      checkOnResize(gCurrentTest.checkLeftVisible);
    });
    AllPagesList.doCommand();
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
    checkOnResize(gCurrentTest.onFinish);
  },

  onFinish: function() {
    Browser.hideSidebars();
    AwesomeScreen.activePanel = null;
    runNextTest();
  }
});

gTests.push({
  desc: "Testing horizontal positionning of the sidebars for multiple columns with an undo tab",

  run: function() {
    let tabs = document.getElementById("tabs");
    ok(tabs._columnsCount > 1, "Tabs layout should be on multiple columns");

    Elements.tabs.addEventListener("TabRemove", function() {
      Elements.tabs.removeEventListener("TabRemove", arguments.callee, false);
      setTimeout(gCurrentTest.onTabClose, 0);
    }, false);

    let lastTab = newTabs.pop().chromeTab;
    lastTab._onClose();
  },

  onTabClose: function() {
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
    checkOnResize(gCurrentTest.onFinish);
  },

  onFinish: function() {
    Browser.hideSidebars();
    runNextTest();
  }
});

let baseURI = "http://mochi.test:8888/browser/mobile/chrome/";
let testURL_01 = baseURI + "browser_scrollbar.sjs?";

let gCurrentTest = null;
let gTests = [];
let gOpenedTabs = []; // for cleanup

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

function waitForPageShow(aPageURL, aCallback) {
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (aMessage.target.currentURI.spec == aPageURL) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      messageManager.addMessageListener("MozScrolledAreaChanged", function(aMessage) {
        messageManager.removeMessageListener("MozScrolledAreaChanged", arguments.callee);
        aCallback();
      });
    }
  });
};

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();
  runNextTest();
}

let horizontalScrollbar = document.getElementById("horizontal-scroller");
let verticalScrollbar = document.getElementById("vertical-scroller");

function checkScrollbars(aHorizontalVisible, aVerticalVisible, aHorizontalPosition, aVerticalPosition) {
  let browser = getBrowser();
  let width = browser.getBoundingClientRect().width;
  let height = browser.getBoundingClientRect().height;
  EventUtils.synthesizeMouse(browser, width / 2, height / 4, { type: "mousedown" });
  EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mousemove" });

  let horizontalVisible = horizontalScrollbar.hasAttribute("panning"),
      verticalVisible = verticalScrollbar.hasAttribute("panning");
  is(horizontalVisible, aHorizontalVisible, "The horizontal scrollbar should be " + (aHorizontalVisible ? "visible" : "hidden"));
  is(verticalVisible, aVerticalVisible, "The vertical scrollbar should be " + (aVerticalVisible ? "visible" : "hidden"));

  EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mouseup" });
}

gTests.push({
  desc: "Testing visibility of scrollbars",

  run: function() {
    waitForPageShow(testURL_01 + "blank", gCurrentTest.checkNotScrollable);
    gOpenedTabs.push(Browser.addTab(testURL_01 + "blank", true));
  },

  checkNotScrollable: function() {
    checkScrollbars(false, false);

    waitForPageShow(testURL_01 + "horizontal", gCurrentTest.checkHorizontalScrollable);
    gOpenedTabs.push(Browser.addTab(testURL_01 + "horizontal", true));
  },

  checkHorizontalScrollable: function() {
    checkScrollbars(true, false);

    waitForPageShow(testURL_01 + "vertical", gCurrentTest.checkVerticalScrollable);
    gOpenedTabs.push(Browser.addTab(testURL_01 + "vertical", true));
  },

  checkVerticalScrollable: function() {
    checkScrollbars(false, true);

    waitForPageShow(testURL_01 + "both", gCurrentTest.checkBothScrollable);
    gOpenedTabs.push(Browser.addTab(testURL_01 + "both", true));
  },

  checkBothScrollable: function() {
    checkScrollbars(true, true);
    Elements.browsers.addEventListener("PanFinished", function(aEvent) {
      setTimeout(function() {
        for (let iTab=0; iTab<gOpenedTabs.length; iTab++)
          Browser.closeTab(gOpenedTabs[iTab]);
        Browser.hideSidebars();
      }, 0);
    }, false);
    runNextTest();
  }
});


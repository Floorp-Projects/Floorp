"use strict";

const testURL_01 = chromeRoot + "browser_blank_01.html";
const testURL_01_Remote = serverRoot + "browser_blank_01.html";

function test() {
  waitForExplicitFinish();
  runNextTest();
}

gTests.push({
  desc: "Test that kinetic panning does not open sidebars.",
  tab: null,

  run: function() {
    gCurrentTest.tab = Browser.addTab(testURL_01, true);
    onMessageOnce(gCurrentTest.tab.browser.messageManager, "Browser:FirstPaint", gCurrentTest.checkPan);
  },

  checkPan: function() {
    let browser = gCurrentTest.tab.browser;
    let docWidth = browser.contentDocumentWidth * browser.scale;
    let winWidth = window.innerWidth;
    info("Browser document width is " + docWidth);
    info("Window width is " + winWidth);
    ok(docWidth <= winWidth,
       "Sanity check. Blank document cannot be panned left or right.");

    function dragAndCheck(dx) {
      let dragger = Elements.browsers.customDragger;
      try {
        dragger.dragStart(0, 0, null, null);
        dragger.dragMove(dx, 0, null, true);

        let [leftVis, rightVis] = Browser.computeSidebarVisibility();
        is(leftVis, 0, "Left sidebar is not visible");
        is(rightVis, 0, "Right sidebar is not visible");
      } finally {
        // Be fail tolerant and hide sidebars in case tests failed.
        Browser.hideSidebars();
        dragger.dragStop();
      }
    }

    dragAndCheck(-20);
    dragAndCheck(20);

    Browser._doCloseTab(gCurrentTest.tab);
    runNextTest();
  }
});

gTests.push({
  desc: "Test that urlbar cannot be panned in when content is captured.",
  tab: null,

  run: function() {
    gCurrentTest.tab = Browser.addTab(testURL_01_Remote, true);
    Browser.selectedTab = gCurrentTest.tab;
    onMessageOnce(gCurrentTest.tab.browser.messageManager, "MozScrolledAreaChanged", gCurrentTest.mouseMove);
  },

  mouseMove: function(json) {
    let inputHandler = gCurrentTest.tab.browser.parentNode;
    function fireMouseEvent(y, type) {
      EventUtils.synthesizeMouse(inputHandler, 0, y, { type: type });
    }

    Browser.hideTitlebar();
    let rect = Elements.browsers.getBoundingClientRect();
    is(rect.top, 0, "Titlebar begins hidden");

    let dragger = Elements.browsers.customDragger;
    try {
      dragger.contentCanCapture = true;
      dragger.dragStart(0, 0, null, null);
      dragger.dragMove(0, 20, null, true);
      dragger.dragStop();
    } finally {
      dragger.contentCanCapture = false;
    }

    rect = Elements.browsers.getBoundingClientRect();
    is(rect.top, 0, "Titlebar is still hidden");

    Browser._doCloseTab(gCurrentTest.tab);
    runNextTest();
  }
});

/*
 * Check VKB show/hide behavior
 */
let testURL_01 = chromeRoot + "browser_forms.html";
messageManager.loadFrameScript(chromeRoot + "remote_vkb.js", true);

/* ============================= Tests Utils =============================== */
let gTests = [];
let gCurrentTest = null;

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
    finish();
  }
}

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  if ("gTimeout" in window)
    setTimeout(runNextTest, gTimeout);
  else
    runNextTest();
}
/* ============================ End Utils ================================== */

Components.utils.import("resource://gre/modules/Services.jsm");
let VKBStateHasChanged = false;
let VKBObserver = {
  _enabled: false,
  observe: function(aTopic, aSubject, aData) {
    if (this._enabled != parseInt(aData)) {
      this._enabled = parseInt(aData);
      VKBStateHasChanged = true;
    }
  }
};
Services.obs.addObserver(VKBObserver, "ime-enabled-state-changed", false);

function waitForVKBChange(aCallback, aState) {
  waitForAndContinue(aCallback, function() {
    if (VKBStateHasChanged) {
      VKBStateHasChanged = false;
      return true;
    }

    return VKBStateHasChanged;
  });
}

let newTab = null;

function waitForPageShow(aPageURL, aCallback) {
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (aMessage.target.currentURI.spec == aPageURL) {
      messageManager.removeMessageListener("pageshow", arguments.callee);
      setTimeout(function() { aCallback(); }, 0);
    }
  });
};

function dragElement(element, x1, y1, x2, y2) {
  EventUtils.synthesizeMouse(element, x1, y1, { type: "mousedown" });
  EventUtils.synthesizeMouse(element, x2, y2, { type: "mousemove" });
  EventUtils.synthesizeMouse(element, x2, y2, { type: "mouseup" });
}

//------------------------------------------------------------------------------
// Case: Test interactions with a VKB from content
gTests.push({
  desc: "Test interactions with a VKB from content",
  run: function() {
    waitForPageShow(testURL_01, gCurrentTest.focusContentInputField);

    newTab = Browser.addTab(testURL_01, true);
    ok(newTab, "Tab Opened");
  },

  focusContentInputField: function() {
    is(VKBObserver._enabled, false, "Initially the VKB should be closed");

    AsyncTests.waitFor("Test:FocusRoot", {}, function(json) {
      waitForVKBChange(gCurrentTest.showLeftSidebar);
    })
  },

  showLeftSidebar: function() {
    is(VKBObserver._enabled, true, "When focusing an input field the VKB should be opened");

    let browsers = document.getElementById("browsers");
    dragElement(browsers, window.innerWidth / 2, window.innerHeight / 2, 1000, 1000);
    waitForVKBChange(gCurrentTest.showRightSidebar);
  },

  showRightSidebar: function() {
    is(VKBObserver._enabled, true, "When pannning to the leftSidebar the VKB state should not changed");

    let browsers = document.getElementById("browsers");
    dragElement(browsers, window.innerWidth / 2, window.innerHeight / 2, -1000, -1000);
    waitForVKBChange(gCurrentTest.changeTab);
  },

  changeTab: function() {
    is(VKBObserver._enabled, true, "When panning to the right sidebar the VKB state should not changed");

    let firstTab = document.getElementById("tabs").children.firstChild;
    BrowserUI.selectTab(firstTab);
    waitForVKBChange(gCurrentTest.prepareOpenRightPanel);
  },

  prepareOpenRightPanel: function() {
    is(VKBObserver._enabled, false, "Switching tab should close the VKB");

    BrowserUI.selectTab(newTab);

    // Give back the focus to the content input and launch and check
    // interaction with the right panel
    AsyncTests.waitFor("Test:FocusRoot", {}, function(json) {
      waitForVKBChange(gCurrentTest.openRightPanel);
    });
  },

  openRightPanel: function() {
    is(VKBObserver._enabled, true, "Re-cliking on an input field should re-open the VKB");

    let toolsButton = document.getElementById("tool-panel-open");
    let rect = toolsButton.getBoundingClientRect();
    EventUtils.synthesizeMouse(toolsButton, rect.width / 2, rect.height / 2, {});
    waitForVKBChange(function() {
      is(VKBObserver._enabled, false, "Opening the right panel should close the VKB");
      BrowserUI.hidePanel();
      Browser.hideSidebars();
      BrowserUI.closeTab(newTab);
      Services.obs.removeObserver(VKBObserver, "ime-enabled-state-changed");
      runNextTest();
    });
  }
});

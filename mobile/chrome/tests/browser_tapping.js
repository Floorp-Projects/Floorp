/*
 * Testing the tapping interactions:
 *   single tap, double tap & long tap
 */

let testURL = chromeRoot + "browser_tap_content.html";

let gTests = [];
let gCurrentTest = null;
let gCurrentTab;

let gEvents = [];
function dumpEvents(aEvent) {
  gEvents.push(aEvent.type);
}

function clearEvents() {
  gEvents = [];
}

function checkEvents(aEvents) {
  if (aEvents.length != gEvents.length) {
    info("---- event check: failed length (" + aEvents.length + " != " + gEvents.length + ")\n");
    info("---- expected: [" + aEvents.join(",") + "] actual: [" + gEvents.join(",") + "]\n");
    return false;
  }

  for (let i=0; i<aEvents.length; i++) {
    if (aEvents[i] != gEvents[i]) {
      info("---- event check: failed match (" + aEvents[i] + " != " + gEvents[i] + "\n");
      return false;
    }
  }
  return true;
}

let gContextTypes = "";
function dumpMessages(aMessage) {
  if (aMessage.name == "Browser:ContextMenu") {
    aMessage.json.types.forEach(function(aType) {
      gContextTypes.push(aType);
    })
  }  
}

function clearContextTypes() {
  gContextTypes = [];

  if (ContextHelper.popupState)
    ContextHelper.hide();
}

function checkContextTypes(aTypes) {
  if (aTypes.length != gContextTypes.length) {
    info("---- type check: failed length (" + aTypes.length + " != " + gContextTypes.length + ")\n");
    info("---- expected: [" + aTypes.join(",") + "] actual: [" + gContextTypes.join(",") + "]\n");
    return false;
  }

  for (let i=0; i<aTypes.length; i++) {
    if (gContextTypes.indexOf(aTypes[i]) == -1) {
      info("---- type check: failed match (" + aTypes[i] + ")\n");
      info("---- expected: [" + aTypes.join(",") + "] actual: [" + gContextTypes.join(",") + "]\n");
      return false;
    }
  }
  return true;
}


function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Add new tab
  gCurrentTab = Browser.addTab(testURL, true);
  ok(gCurrentTab, "Tab Opened");

  window.addEventListener("TapSingle", dumpEvents, true);
  window.addEventListener("TapDouble", dumpEvents, true);
  window.addEventListener("TapLong", dumpEvents, true);
  
  // Wait for the tab to load, then do the tests
  messageManager.addMessageListener("pageshow", function() {
  if (gCurrentTab.browser.currentURI.spec == testURL) {
    messageManager.removeMessageListener("pageshow", arguments.callee);
    runNextTest();
  }});
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
    window.removeEventListener("TapSingle", dumpEvents, true);
    window.removeEventListener("TapDouble", dumpEvents, true);
    window.removeEventListener("TapLong", dumpEvents, true);

    Browser.closeTab(gCurrentTab);

    finish();
  }
}

//------------------------------------------------------------------------------
// Case: Test the double tap behavior
gTests.push({
  desc: "Test the double tap behavior",

  run: function() {
    let browser = gCurrentTab.browser;
    let width = browser.getBoundingClientRect().width;
    let height = browser.getBoundingClientRect().height;

    // Should fire "TapSingle"
    // XXX not working? WTF?
    info("Test good single tap");
    clearEvents();
    EventUtils.synthesizeMouse(browser, width / 2, height / 2, {});
    todo(checkEvents(["TapSingle"]), "Fired a good single tap");
    clearEvents();

    setTimeout(function() { gCurrentTest.doubleTapTest(); }, 500);
  },

  doubleTapTest: function() {
    let width = window.innerWidth;
    let height = window.innerHeight;

    // Should fire "TapDouble"
    info("Test good double tap");
    clearEvents();
    EventUtils.synthesizeMouse(document.documentElement, width / 2, height / 2, {});
    EventUtils.synthesizeMouse(document.documentElement, width / 2, height / 2, {});
    ok(checkEvents(["TapDouble"]), "Fired a good double tap");
    clearEvents();

    setTimeout(function() { gCurrentTest.doubleTapFailTest(); }, 500);
  },

  doubleTapFailTest: function() {
    let browser = gCurrentTab.browser;
    let width = browser.getBoundingClientRect().width;
    let height = browser.getBoundingClientRect().height;

    // Should fire "TapSingle", "TapSingle"
    info("Test two single taps in different locations");
    clearEvents();
    EventUtils.synthesizeMouse(browser, width / 4, height / 4, {});
    EventUtils.synthesizeMouse(browser, width * 3 / 4, height * 3 / 4, {});
    ok(checkEvents(["TapSingle", "TapSingle"]), "Fired two single taps in different places, not a double tap");
    clearEvents();

    setTimeout(function() { gCurrentTest.tapPanTest(); }, 500);
  },

  tapPanTest: function() {
    let browser = gCurrentTab.browser;
    let width = browser.getBoundingClientRect().width;
    let height = browser.getBoundingClientRect().height;

    info("Test a pan - non-tap event");
    clearEvents();
    EventUtils.synthesizeMouse(browser, width / 2, height / 4, { type: "mousedown" });
    EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mousemove" });
    EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mouseup" });
    ok(checkEvents([]), "Fired a pan which should be seen as a non event");
    clearEvents();
    
    setTimeout(function() { gCurrentTest.longTapFailTest(); }, 500);
  },

  longTapFailTest: function() {
    let browser = gCurrentTab.browser;
    let width = browser.getBoundingClientRect().width;
    let height = browser.getBoundingClientRect().height;

    info("Test a long pan - non-tap event");
    clearEvents();
    EventUtils.synthesizeMouse(browser, width / 2, height / 4, { type: "mousedown" });
    EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mousemove" });
    setTimeout(function() {
      EventUtils.synthesizeMouse(browser, width / 2, height * 3 / 4, { type: "mouseup" });
      ok(checkEvents([]), "Fired a pan + delay which should be seen as a non-event");
      clearEvents();

      gCurrentTest.longTapPassTest();
    }, 500);
  },

  longTapPassTest: function() {
    let browser = gCurrentTab.browser;
    let width = browser.getBoundingClientRect().width;
    let height = browser.getBoundingClientRect().height;

    info("Test a good long pan");
    clearEvents();
    EventUtils.synthesizeMouse(browser, width / 2, height / 4, { type: "mousedown" });
    setTimeout(function() {
      EventUtils.synthesizeMouse(browser, width / 2, height / 4, { type: "mouseup" });
      ok(checkEvents(["TapLong"]), "Fired a good long tap");
      clearEvents();

      gCurrentTest.contextPlainLinkTest();
    }, 500);
  },

  contextPlainLinkTest: function() {
    let browser = gCurrentTab.browser;
    browser.messageManager.addMessageListener("Browser:ContextMenu", dumpMessages);

    let link = browser.contentDocument.getElementById("link-single");
    let linkRect = link.getBoundingClientRect();

    clearContextTypes();
    EventUtils.synthesizeMouseForContent(link, linkRect.width/2, linkRect.height/4, { type: "mousedown" }, window);
    setTimeout(function() {
      EventUtils.synthesizeMouseForContent(link, linkRect.width/2, linkRect.height/4, { type: "mouseup" }, window);
      ok(checkContextTypes(["link","link-saveable","link-openable"]), "Plain link context types");
      clearContextTypes();

      gCurrentTest.contextPlainImageTest();
    }, 500);
  },

  contextPlainImageTest: function() {
    let browser = gCurrentTab.browser;
    browser.messageManager.addMessageListener("Browser:ContextMenu", dumpMessages);

    let img = browser.contentDocument.getElementById("img-single");
    let imgRect = img.getBoundingClientRect();

    clearContextTypes();
    EventUtils.synthesizeMouseForContent(img, imgRect.width/2, imgRect.height/2, { type: "mousedown" }, window);
    setTimeout(function() {
      EventUtils.synthesizeMouseForContent(img, 1, 1, { type: "mouseup" }, window);
      ok(checkContextTypes(["image","image-shareable","image-loaded"]), "Plain image context types");
      clearContextTypes();

      gCurrentTest.contextNestedImageTest();
    }, 500);
  },

  contextNestedImageTest: function() {
    let browser = gCurrentTab.browser;
    browser.messageManager.addMessageListener("Browser:ContextMenu", dumpMessages);

    let img = browser.contentDocument.getElementById("img-nested");
    let imgRect = img.getBoundingClientRect();

    clearContextTypes();
    EventUtils.synthesizeMouseForContent(img, 1, 1, { type: "mousedown" }, window);
    setTimeout(function() {
      EventUtils.synthesizeMouseForContent(img, 1, 1, { type: "mouseup" }, window);
      ok(checkContextTypes(["link","link-saveable","image","image-shareable","image-loaded","link-openable"]), "Nested image context types");
      clearContextTypes();

      gCurrentTest.lastTest();
    }, 500);
  },

  lastTest: function() {
    gCurrentTab.browser.messageManager.removeMessageListener("Browser:ContextMenu", dumpMessages);

    runNextTest();
  }
});


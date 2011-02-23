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
  if (aEvent.target != gCurrentTab.browser.parentNode)
    return;

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

function waitForContextMenu(aCallback, aNextTest) {
  clearContextTypes();

  let browser = gCurrentTab.browser;
  browser.messageManager.addMessageListener("Browser:ContextMenu", function(aMessage) {
    browser.messageManager.removeMessageListener(aMessage.name, arguments.callee);
    aMessage.json.types.forEach(function(aType) {
      gContextTypes.push(aType);
    });
    setTimeout(function() {
      aCallback(aMessage.json);
      clearContextTypes();
      aNextTest();
    }, 0);
  });
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
    // Hack the allowZoom getter since we want to observe events
    // for testing purpose even if it is a local tab
    gCurrentTab.__defineGetter__("allowZoom", function() {
      return true;
    });
    setTimeout(runNextTest, 0);
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
    let inputHandler = gCurrentTab.browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    // Should fire "TapSingle"
    info("Test good single tap");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 2, {});

    // We wait a bit because of the delay allowed for double clicking on device
    // where it is not native
    setTimeout(function() {
      ok(checkEvents(["TapSingle"]), "Fired a good single tap");
      clearEvents();
      gCurrentTest.doubleTapTest();
    }, kDoubleClickInterval);
  },

  doubleTapTest: function() {
    let inputHandler = gCurrentTab.browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    // Should fire "TapDouble"
    info("Test good double tap");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 2, {});
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 2, {});

    setTimeout(function() {
      ok(checkEvents(["TapDouble"]), "Fired a good double tap");
      clearEvents();

      gCurrentTest.doubleTapFailTest();
    }, 500);
  },

  doubleTapFailTest: function() {
    let inputHandler = gCurrentTab.browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    // Should fire "TapSingle", "TapSingle"
    info("Test two single taps in different locations");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 3, height / 3, {});
    EventUtils.synthesizeMouse(inputHandler, width * 2 / 3, height * 2 / 3, {});

    setTimeout(function() {
      ok(checkEvents(["TapSingle", "TapSingle"]), "Fired two single taps in different places, not a double tap");
      clearEvents();

      gCurrentTest.tapPanTest();
    }, 500);
  },

  tapPanTest: function() {
    let inputHandler = gCurrentTab.browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    info("Test a pan - non-tap event");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 3, { type: "mousedown" });
    EventUtils.synthesizeMouse(inputHandler, width / 2, height * 2 / 3, { type: "mousemove" });
    EventUtils.synthesizeMouse(inputHandler, width / 2, height * 2 / 3, { type: "mouseup" });
    ok(checkEvents([]), "Fired a pan which should be seen as a non event");
    clearEvents();

    setTimeout(function() { gCurrentTest.longTapFailTest(); }, 500);
  },

  longTapFailTest: function() {
    let inputHandler = gCurrentTab.browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    info("Test a long pan - non-tap event");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 3, { type: "mousedown" });
    EventUtils.synthesizeMouse(inputHandler, width / 2, height * 2 / 3, { type: "mousemove" });
    setTimeout(function() {
      EventUtils.synthesizeMouse(inputHandler, width / 2, height * 2 / 3, { type: "mouseup" });
      ok(checkEvents([]), "Fired a pan + delay which should be seen as a non-event");
      clearEvents();

      gCurrentTest.longTapPassTest();
    }, 500);
  },

  longTapPassTest: function() {
    let browser = gCurrentTab.browser;
    let inputHandler = browser.parentNode;
    let width = inputHandler.getBoundingClientRect().width;
    let height = inputHandler.getBoundingClientRect().height;

    window.addEventListener("TapLong", function() {
      window.removeEventListener("TapLong", arguments.callee, true);
      EventUtils.synthesizeMouse(inputHandler, width / 2, height / 2, { type: "mouseup" });
      ok(checkEvents(["TapLong"]), "Fired a good long tap");
      clearEvents();
    }, true);

    browser.messageManager.addMessageListener("Browser:ContextMenu", function(aMessage) {
      browser.messageManager.removeMessageListener(aMessage.name, arguments.callee);
      setTimeout(gCurrentTest.contextPlainLinkTest, 0);
    });

    info("Test a good long pan");
    clearEvents();
    EventUtils.synthesizeMouse(inputHandler, width / 2, height / 2, { type: "mousedown" });
  },

  contextPlainLinkTest: function() {
    waitForContextMenu(function(aJSON) {
      is(aJSON.linkTitle, "A blank page - nothing interesting", "Text content should be the content of the second link");
      ok(checkContextTypes(["link","link-saveable","link-openable"]), "Plain link context types");
    }, gCurrentTest.contextPlainImageTest);

    let browser = gCurrentTab.browser;
    let linkDisabled = browser.contentDocument.getElementById("link-disabled");
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    linkDisabled.dispatchEvent(event);

    let link = browser.contentDocument.getElementById("link-single");
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    link.dispatchEvent(event);
  },

  contextPlainImageTest: function() {
    waitForContextMenu(function() {
      ok(checkContextTypes(["image","image-shareable","image-loaded"]), "Plain image context types");
    }, gCurrentTest.contextNestedImageTest);

    let browser = gCurrentTab.browser;
    let img = browser.contentDocument.getElementById("img-single");
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    img.dispatchEvent(event);
  },

  contextNestedImageTest: function() {
    waitForContextMenu(function() {
      ok(checkContextTypes(["link","link-saveable","image","image-shareable","image-loaded","link-openable"]), "Nested image context types");
    }, runNextTest);

    let browser = gCurrentTab.browser;
    let img = browser.contentDocument.getElementById("img-nested");
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    img.dispatchEvent(event);
  }
});


/*
 * Testing the context menus on editboxes:
 *   plain and url
 */

let testURL = chromeRoot + "browser_tap_contentedit.html";

let gTests = [];
let gCurrentTest = null;
let gCurrentTab;

const kDoubleClickIntervalPlus = kDoubleClickInterval + 100;

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

  let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
  clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);

  SelectionHelper.enabled = false;

  window.addEventListener("TapSingle", dumpEvents, true);
  window.addEventListener("TapDouble", dumpEvents, true);
  window.addEventListener("TapLong", dumpEvents, true);

  // Wait for the tab to load, then do the tests
  messageManager.addMessageListener("Browser:FirstPaint", function() {
  if (gCurrentTab.browser.currentURI.spec == testURL) {
    messageManager.removeMessageListener("Browser:FirstPaint", arguments.callee);
    // Hack the allowZoom getter since we want to observe events
    // for testing purpose even if it is a local tab
    gCurrentTab.__defineGetter__("allowZoom", function() {
      return true;
    });

    // Using setTimeout(..., 0) here result into a duplicate mousedown/mouseup
    // sequence that makes the double tap test fails (add some dump in input.js
    // to see that...)
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
    let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);

    window.removeEventListener("TapSingle", dumpEvents, true);
    window.removeEventListener("TapDouble", dumpEvents, true);
    window.removeEventListener("TapLong", dumpEvents, true);

    SelectionHelper.enabled = true;
    Browser.closeTab(gCurrentTab);

    finish();
  }
}

//------------------------------------------------------------------------------
gTests.push({
  desc: "Test empty plain textbox",

  run: function() {
    waitForContextMenu(function(aJSON) {
      ok(checkContextTypes(["input-text"]), "Editbox with no text, no selection and no clipboard");
    }, runNextTest);

    let browser = gCurrentTab.browser;
    let plainEdit = browser.contentDocument.getElementById("plain-edit");
    plainEdit.readOnly = false;
    plainEdit.value = "";

    // Try very hard to keep "paste" from if the clipboard has data
    let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
    plainEdit.readOnly = true;
    
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    plainEdit.dispatchEvent(event);
  }
});

//------------------------------------------------------------------------------
gTests.push({
  desc: "Test plain textbox with text fully selected",

  run: function() {
    waitForContextMenu(function(aJSON) {
      ok(checkContextTypes(["input-text", "copy"]), "Editbox with text and full selection, but no clipboard");
    }, runNextTest);

    let browser = gCurrentTab.browser;
    let plainEdit = browser.contentDocument.getElementById("plain-edit");
    plainEdit.readOnly = false;
    plainEdit.value = "Every time we fix a bug, Stuart call's the President";
    let plainEdit = plainEdit.QueryInterface(Ci.nsIDOMNSEditableElement);
    plainEdit.editor.selectAll();

    // Try very hard to keep "paste" from if the clipboard has data
    let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
    plainEdit.readOnly = true;
    
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    plainEdit.dispatchEvent(event);
  }
});

//------------------------------------------------------------------------------
gTests.push({
  desc: "Test plain textbox with text no selection",

  run: function() {
    waitForContextMenu(function(aJSON) {
      ok(checkContextTypes(["input-text", "copy-all", "select-all"]), "Editbox with text, but no selection and no clipboard");
    }, runNextTest);

    let browser = gCurrentTab.browser;
    let plainEdit = browser.contentDocument.getElementById("plain-edit");
    plainEdit.readOnly = false;
    plainEdit.value = "Every time we fix a bug, Stuart call's the President";
    plainEdit.selectionStart = 0;
    plainEdit.selectionEnd = 0;

    // Try very hard to keep "paste" from if the clipboard has data
    let clipboard = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    clipboard.emptyClipboard(Ci.nsIClipboard.kGlobalClipboard);
    plainEdit.readOnly = true;
    
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    plainEdit.dispatchEvent(event);
  }
});

//------------------------------------------------------------------------------
gTests.push({
  desc: "Test plain textbox with text no selection and text on clipboard",

  run: function() {
    waitForContextMenu(function(aJSON) {
      ok(checkContextTypes(["input-text", "copy-all", "select-all", "paste"]), "Editbox with text and clipboard, but no selection");
    }, runNextTest);

    let browser = gCurrentTab.browser;
    let plainEdit = browser.contentDocument.getElementById("plain-edit");
    plainEdit.readOnly = false;
    plainEdit.value = "Every time we fix a bug, Stuart call's the President";
    plainEdit.selectionStart = 0;
    plainEdit.selectionEnd = 0;

    // Put some data on the clipboard to get "paste" to be active
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString("We are testing Firefox", content.document);
    
    let event = content.document.createEvent("PopupEvents");
    event.initEvent("contextmenu", true, true);
    plainEdit.dispatchEvent(event);
  }
});

//------------------------------------------------------------------------------
gTests.push({
  desc: "Test that tapping on input box causes mouse events to fire",

  run: function() {
    let browser = gCurrentTab.browser;
    let plainEdit = browser.contentDocument.getElementById("plain-edit");
    plainEdit.blur();
    plainEdit.value = '';

    let eventArray = ['mouseover', 'mousedown', 'mouseup', 'click'];

  while (eventArray.length > 0) {
    let currentEventType = eventArray.shift();
    browser.contentWindow.addEventListener(currentEventType, function(e) {
      browser.contentWindow.removeEventListener(currentEventType, arguments.callee, true);
      ok(e.target == plainEdit, e.type + " should fire over input id=" + plainEdit.id);
      plainEdit.value += e.type + ' ';

      if (e.type == 'click') {
        ok(plainEdit.value == 'mouseover mousedown mouseup click ', 
                              'Events should fire in this order: mouseover mousedown mouseup click ');
        runNextTest();
      }
    } , true);
  }

    EventUtils.synthesizeMouse(plainEdit, browser.getBoundingClientRect().left + plainEdit.getBoundingClientRect().left + 2, 
                                          browser.getBoundingClientRect().top + plainEdit.getBoundingClientRect().top + 2, {});
  }
});


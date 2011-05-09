/*=============================================================================
  Common Helpers functions
=============================================================================*/

const kDefaultWait = 2000;
// Wait for a condition and call a supplied callback if condition is met within
// alloted time. If condition is not met, cause a hard failure, stopping the test.
function waitFor(callback, test, timeout) {
  if (test()) {
    callback();
    return;
  }

  timeout = timeout || Date.now();
  if (Date.now() - timeout > kDefaultWait)
    throw "waitFor timeout";
  setTimeout(waitFor, 50, callback, test, timeout);
};

// Wait for a condition and call a supplied callback if condition is met within
// alloted time. If condition is not met, continue anyway. Use this helper if the
// callback will test for the outcome, but not stop the entire test.
function waitForAndContinue(callback, test, timeout) {
  if (test()) {
    callback();
    return;
  }

  timeout = timeout || Date.now();
  if (Date.now() - timeout > kDefaultWait) {
    callback();
    return;
  }
  setTimeout(waitForAndContinue, 50, callback, test, timeout);
};

// Listen for the specified message once, then remove the listener.
function onMessageOnce(aMessageManager, aName, aCallback) {
  aMessageManager.addMessageListener(aName, function onMessage(aMessage) {
    aMessageManager.removeMessageListener(aName, onMessage);
    aCallback(aMessage);
  });
}

function makeURI(spec) {
  return Services.io.newURI(spec, null, null);
};

EventUtils.synthesizeString = function synthesizeString(aString, aWindow) {
  for (let i = 0; i < aString.length; i++) {
    EventUtils.synthesizeKey(aString.charAt(i), {}, aWindow);
  }
};

EventUtils.synthesizeMouseForContent = function synthesizeMouseForContent(aElement, aOffsetX, aOffsetY, aEvent, aWindow) {
  let container = document.getElementById("browsers");
  let rect = container.getBoundingClientRect();

  EventUtils.synthesizeMouse(aElement, rect.left + aOffsetX, rect.top + aOffsetY, aEvent, aWindow);
};

let AsyncTests = {
  _tests: {},
  waitFor: function(aMessage, aData, aCallback) {
    messageManager.addMessageListener(aMessage, this);
    if (!this._tests[aMessage])
      this._tests[aMessage] = [];

    this._tests[aMessage].push(aCallback || function() {});
    setTimeout(function() {
      Browser.selectedBrowser.messageManager.sendAsyncMessage(aMessage, aData || { });
    }, 0);
  },

  receiveMessage: function(aMessage) {
    let test = this._tests[aMessage.name];
    let callback = test.shift();
    if (callback)
      callback(aMessage.json);
  }
};

let gCurrentTest = null;
let gTests = [];

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

let serverRoot = "http://example.com/browser/mobile/chrome/tests/";
let chromeRoot = getRootDirectory(gTestPath);
// For some security reasons (which?), loading remote_head using chromeRoot
// instead of baseURI make the browser_formsZoom.js test fails.
let baseURI = "http://mochi.test:8888/browser/mobile/chrome/tests/";
messageManager.loadFrameScript(baseURI + "remote_head.js", true);
messageManager.loadFrameScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", true);

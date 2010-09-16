/*=============================================================================
  Common Helpers functions
=============================================================================*/
function waitFor(callback, test, timeout) {
  if (test()) {
    callback();
    return;
  }

  timeout = timeout || Date.now();
  if (Date.now() - timeout > 1000)
    throw "waitFor timeout";
  setTimeout(waitFor, 50, callback, test, timeout);
};

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
    Browser.selectedBrowser.messageManager.sendAsyncMessage(aMessage, aData || { });
  },

  receiveMessage: function(aMessage) {
    let test = this._tests[aMessage.name];
    let callback = test.shift();
    if (callback)
      callback(aMessage.json);
  }
};

messageManager.loadFrameScript("chrome://mochikit/content/browser/mobile/chrome/remote_head.js", true);
messageManager.loadFrameScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", true);

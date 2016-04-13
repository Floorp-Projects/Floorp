/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gPopupShownExpected = false;
var gPopupShownListener;
var gLastAutoCompleteResults;
var gChromeScript;

/*
 * Returns the element with the specified |name| attribute.
 */
function $_(formNum, name) {
  var form = document.getElementById("form" + formNum);
  if (!form) {
    ok(false, "$_ couldn't find requested form " + formNum);
    return null;
  }

  var element = form.elements.namedItem(name);
  if (!element) {
    ok(false, "$_ couldn't find requested element " + name);
    return null;
  }

  // Note that namedItem is a bit stupid, and will prefer an
  // |id| attribute over a |name| attribute when looking for
  // the element.

  if (element.hasAttribute("name") && element.getAttribute("name") != name) {
    ok(false, "$_ got confused.");
    return null;
  }

  return element;
}

// Mochitest gives us a sendKey(), but it's targeted to a specific element.
// This basically sends an untargeted key event, to whatever's focused.
function doKey(aKey, modifier) {
    var keyName = "DOM_VK_" + aKey.toUpperCase();
    var key = SpecialPowers.Ci.nsIDOMKeyEvent[keyName];

    // undefined --> null
    if (!modifier)
        modifier = null;

    // Window utils for sending fake key events.
    var wutils = SpecialPowers.getDOMWindowUtils(window);

    if (wutils.sendKeyEvent("keydown",  key, 0, modifier)) {
      wutils.sendKeyEvent("keypress", key, 0, modifier);
    }
    wutils.sendKeyEvent("keyup",    key, 0, modifier);
}

function registerPopupShownListener(listener) {
  if (gPopupShownListener) {
    ok(false, "got too many popupshownlisteners");
    return;
  }
  gPopupShownListener = listener;
}

function getMenuEntries() {
  if (!gLastAutoCompleteResults) {
    throw new Error("no autocomplete results");
  }

  var results = gLastAutoCompleteResults;
  gLastAutoCompleteResults = null;
  return results;
}

function checkArrayValues(actualValues, expectedValues, msg) {
  is(actualValues.length, expectedValues.length, "Checking array values: " + msg);
  for (var i = 0; i < expectedValues.length; i++)
    is(actualValues[i], expectedValues[i], msg + " Checking array entry #" + i);
}

var checkObserver = {
  verifyStack: [],
  callback: null,

  init() {
    gChromeScript.sendAsyncMessage("addObserver");
    gChromeScript.addMessageListener("satchel-storage-changed", this.observe.bind(this));
  },

  uninit() {
    gChromeScript.sendAsyncMessage("removeObserver");
  },

  waitForChecks: function(callback) {
    if (this.verifyStack.length == 0)
      callback();
    else
      this.callback = callback;
  },

  observe: function({ subject, topic, data }) {
    if (data != "formhistory-add" && data != "formhistory-update")
      return;
    ok(this.verifyStack.length > 0, "checking if saved form data was expected");

    // Make sure that every piece of data we expect to be saved is saved, and no
    // more. Here it is assumed that for every entry satchel saves or modifies, a
    // message is sent.
    //
    // We don't actually check the content of the message, but just that the right
    // quantity of messages is received.
    // - if there are too few messages, test will time out
    // - if there are too many messages, test will error out here
    //
    var expected = this.verifyStack.shift();

    countEntries(expected.name, expected.value,
      function(num) {
        ok(num > 0, expected.message);
        if (checkObserver.verifyStack.length == 0) {
          var callback = checkObserver.callback;
          checkObserver.callback = null;
          callback();
        }
      });
  }
};

function checkForSave(name, value, message) {
  checkObserver.verifyStack.push({ name : name, value: value, message: message });
}

function getFormSubmitButton(formNum) {
  var form = $("form" + formNum); // by id, not name
  ok(form != null, "getting form " + formNum);

  // we can't just call form.submit(), because that doesn't seem to
  // invoke the form onsubmit handler.
  var button = form.firstChild;
  while (button && button.type != "submit") { button = button.nextSibling; }
  ok(button != null, "getting form submit button");

  return button;
}

// Count the number of entries with the given name and value, and call then(number)
// when done. If name or value is null, then the value of that field does not matter.
function countEntries(name, value, then = null) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("countEntries", { name, value });
    gChromeScript.addMessageListener("entriesCounted", function counted(data) {
      gChromeScript.removeMessageListener("entriesCounted", counted);
      if (!data.ok) {
        ok(false, "Error occurred counting form history");
        SimpleTest.finish();
        return;
      }

      if (then) {
        then(data.count);
      }
      resolve(data.count);
    });
  });
}

// Wrapper around FormHistory.update which handles errors. Calls then() when done.
function updateFormHistory(changes, then = null) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("updateFormHistory", { changes });
    gChromeScript.addMessageListener("formHistoryUpdated", function updated({ ok }) {
      gChromeScript.removeMessageListener("formHistoryUpdated", updated);
      if (!ok) {
        ok(false, "Error occurred updating form history");
        SimpleTest.finish();
        return;
      }

      if (then) {
        then();
      }
      resolve();
    });
  });
}

function notifyMenuChanged(expectedCount, expectedFirstValue, then = null) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("waitForMenuChange",
                            { expectedCount,
                              expectedFirstValue });
    gChromeScript.addMessageListener("gotMenuChange", function changed({ results }) {
      gChromeScript.removeMessageListener("gotMenuChange", changed);
      gLastAutoCompleteResults = results;
      if (then) {
        then(results);
      }
      resolve(results);
    });
  });
}

function notifySelectedIndex(expectedIndex, then = null) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("waitForSelectedIndex", { expectedIndex });
    gChromeScript.addMessageListener("gotSelectedIndex", function changed() {
      gChromeScript.removeMessageListener("gotSelectedIndex", changed);
      if (then) {
        then();
      }
      resolve();
    });
  });
}

function getPopupState(then = null) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("getPopupState");
    gChromeScript.addMessageListener("gotPopupState", function listener(state) {
      gChromeScript.removeMessageListener("gotPopupState", listener);
      if (then) {
        then(state);
      }
      resolve(state);
    });
  });
}

function listenForUnexpectedPopupShown() {
  gChromeScript.addMessageListener("onpopupshown", function onPopupShown() {
    if (!gPopupShownExpected) {
      ok(false, "Unexpected autocomplete popupshown event");
    }
  });
}

/**
 * Resolve at the next popupshown event for the autocomplete popup
 * @return {Promise} with the results
 */
function promiseACShown() {
  gPopupShownExpected = true;
  return new Promise(resolve => {
    gChromeScript.addMessageListener("onpopupshown", ({ results }) => {
      gPopupShownExpected = false;
      resolve(results);
    });
  });
}

function satchelCommonSetup() {
  var chromeURL = SimpleTest.getTestFileURL("parent_utils.js");
  gChromeScript = SpecialPowers.loadChromeScript(chromeURL);
  gChromeScript.addMessageListener("onpopupshown", ({ results }) => {
    gLastAutoCompleteResults = results;
    if (gPopupShownListener)
      gPopupShownListener();
  });

  SimpleTest.registerCleanupFunction(() => {
    gChromeScript.sendAsyncMessage("cleanup");
    gChromeScript.destroy();
  });
}


satchelCommonSetup();

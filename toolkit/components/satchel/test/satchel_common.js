/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Services = SpecialPowers.Services;

/*
 * $_
 *
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

    // Window utils for sending fake sey events.
    var wutils = SpecialPowers.getDOMWindowUtils(window);

    if (wutils.sendKeyEvent("keydown",  key, 0, modifier)) {
      wutils.sendKeyEvent("keypress", key, 0, modifier);
    }
    wutils.sendKeyEvent("keyup",    key, 0, modifier);
}


function getAutocompletePopup() {
    var Ci = SpecialPowers.Ci;
    chromeWin = SpecialPowers.wrap(window)
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShellTreeItem)
                    .rootTreeItem
                    .QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindow)
                    .QueryInterface(Ci.nsIDOMChromeWindow);
    autocompleteMenu = chromeWin.document.getElementById("PopupAutoComplete");
    ok(autocompleteMenu, "Got autocomplete popup");

    return autocompleteMenu;
}


function cleanUpFormHist() {
  SpecialPowers.formHistory.update({ op : "remove" });
}
cleanUpFormHist();


var checkObserver = {
  verifyStack: [],
  callback: null,

  waitForChecks: function(callback) {
    if (this.verifyStack.length == 0)
      callback();
    else
      this.callback = callback;
  },

  observe: function(subject, topic, data) {
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
function countEntries(name, value, then) {
  var obj = {};
  if (name !== null)
    obj.fieldname = name;
  if (value !== null)
    obj.value = value;

  var count = 0;
  SpecialPowers.formHistory.count(obj, SpecialPowers.wrapCallbackObject({ handleResult: function (result) { count = result },
                                         handleError: function (error) {
                                           ok(false, "Error occurred searching form history: " + error.message);
                                           SimpleTest.finish();
                                         },
                                         handleCompletion: function (reason) { if (!reason) then(count); }
                                       }));
}

// Wrapper around FormHistory.update which handles errors. Calls then() when done.
function updateFormHistory(changes, then) {
  SpecialPowers.formHistory.update(changes, SpecialPowers.wrapCallbackObject({ handleError: function (error) {
                                                ok(false, "Error occurred updating form history: " + error.message);
                                                SimpleTest.finish();
                                              },
                                              handleCompletion: function (reason) { if (!reason) then(); },
                                            }));
}

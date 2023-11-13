/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint
  "no-unused-vars": ["error", {
    vars: "local",
    args: "none",
  }],
*/

var gPopupShownExpected = false;
var gPopupShownListener;
var gLastAutoCompleteResults;
var gChromeScript;

const TelemetryFilterPropsAC = Object.freeze({
  category: "form_autocomplete",
  method: "show",
  object: "logins",
});

/*
 * Returns the element with the specified |name| attribute.
 */
function getFormElementByName(formNum, name) {
  const formElement = document.querySelector(
    `#form${formNum} [name="${name}"]`
  );

  if (!formElement) {
    ok(false, `getFormElementByName: Couldn't find specified CSS selector.`);
    return null;
  }

  return formElement;
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

  let results = gLastAutoCompleteResults;
  gLastAutoCompleteResults = null;
  return results;
}

class StorageEventsObserver {
  promisesToResolve = [];

  constructor() {
    gChromeScript.sendAsyncMessage("addObserver");
    gChromeScript.addMessageListener(
      "satchel-storage-changed",
      this.observe.bind(this)
    );
  }

  async cleanup() {
    await gChromeScript.sendQuery("removeObserver");
  }

  observe({ subject, topic, data }) {
    this.promisesToResolve.shift()?.({ subject, topic, data });
  }

  promiseNextStorageEvent() {
    return new Promise(resolve => this.promisesToResolve.push(resolve));
  }
}

function getFormSubmitButton(formNum) {
  let form = $("form" + formNum); // by id, not name
  ok(form != null, "getting form " + formNum);

  // we can't just call form.submit(), because that doesn't seem to
  // invoke the form onsubmit handler.
  let button = form.firstChild;
  while (button && button.type != "submit") {
    button = button.nextSibling;
  }
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
    gChromeScript.addMessageListener(
      "formHistoryUpdated",
      function updated({ ok }) {
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
      }
    );
  });
}

async function notifyMenuChanged(expectedCount, expectedFirstValue) {
  gLastAutoCompleteResults = await gChromeScript.sendQuery(
    "waitForMenuChange",
    { expectedCount, expectedFirstValue }
  );
  return gLastAutoCompleteResults;
}

function notifySelectedIndex(expectedIndex) {
  return gChromeScript.sendQuery("waitForSelectedIndex", { expectedIndex });
}

function testMenuEntry(index, statement) {
  return new Promise(resolve => {
    gChromeScript.sendAsyncMessage("waitForMenuEntryTest", {
      index,
      statement,
    });
    gChromeScript.addMessageListener("menuEntryTested", function changed() {
      gChromeScript.removeMessageListener("menuEntryTested", changed);
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
  gPopupShownListener = function onPopupShown() {
    if (!gPopupShownExpected) {
      ok(false, "Unexpected autocomplete popupshown event");
    }
  };
}

async function popupBy(triggerFn) {
  gPopupShownExpected = true;
  const promise = new Promise(resolve => {
    gPopupShownListener = ({ results }) => {
      gPopupShownExpected = false;
      resolve(results);
    };
  });
  if (triggerFn) {
    triggerFn();
  }
  return promise;
}

async function noPopupBy(triggerFn) {
  gPopupShownExpected = false;
  listenForUnexpectedPopupShown();
  SimpleTest.requestFlakyTimeout(
    "Giving a chance for an unexpected popupshown to occur"
  );
  if (triggerFn) {
    await triggerFn();
  }
  await new Promise(resolve => setTimeout(resolve, 500));
}

async function popupByArrowDown() {
  return popupBy(() => {
    synthesizeKey("KEY_Escape"); // in case popup is already open
    synthesizeKey("KEY_ArrowDown");
  });
}

async function noPopupByArrowDown() {
  await noPopupBy(() => {
    synthesizeKey("KEY_Escape"); // in case popup is already open
    synthesizeKey("KEY_ArrowDown");
  });
}

function checkACTelemetryEvent(actualEvent, input, augmentedExtra) {
  ok(
    parseInt(actualEvent[4], 10) > 0,
    "elapsed time is a positive integer after converting from a string"
  );
  let expectedExtra = {
    acFieldName: SpecialPowers.wrap(input).getAutocompleteInfo().fieldName,
    typeWasPassword: SpecialPowers.wrap(input).hasBeenTypePassword ? "1" : "0",
    fieldType: input.type,
    stringLength: input.value.length + "",
    ...augmentedExtra,
  };
  isDeeply(actualEvent[5], expectedExtra, "Check event extra object");
}

let gStorageEventsObserver;

function promiseNextStorageEvent() {
  return gStorageEventsObserver.promiseNextStorageEvent();
}

function satchelCommonSetup() {
  let chromeURL = SimpleTest.getTestFileURL("parent_utils.js");
  gChromeScript = SpecialPowers.loadChromeScript(chromeURL);
  gChromeScript.addMessageListener("onpopupshown", ({ results }) => {
    gLastAutoCompleteResults = results;
    if (gPopupShownListener) {
      gPopupShownListener({ results });
    }
  });

  gStorageEventsObserver = new StorageEventsObserver();

  SimpleTest.registerCleanupFunction(async () => {
    await gStorageEventsObserver.cleanup();
    await gChromeScript.sendQuery("cleanup");
    gChromeScript.destroy();
  });
}

function add_named_task(name, fn) {
  return add_task(
    {
      [name]() {
        return fn();
      },
    }[name]
  );
}

function preventSubmitOnForms() {
  for (const form of document.querySelectorAll("form")) {
    form.onsubmit = e => e.preventDefault();
  }
}

/**
 * Press requested keys and assert input's value
 *
 * @param {HTMLInputElement} input
 * @param {string | Array} keys
 * @param {string} expectedValue
 */
function assertValueAfterKeys(input, keys, expectedValue) {
  if (!Array.isArray(keys)) {
    keys = [keys];
  }
  for (const key of keys) {
    synthesizeKey(key);
  }

  is(input.value, expectedValue, "input value");
}

function assertAutocompleteItems(...expectedValues) {
  const actualValues = getMenuEntries();
  isDeeply(actualValues, expectedValues, "expected autocomplete list");
}

function deleteSelectedAutocompleteItem() {
  synthesizeKey("KEY_Delete", { shiftKey: true });
}

async function openPopupOn(
  inputOrSelector,
  { inputValue = "", expectPopup = true } = {}
) {
  const input =
    typeof inputOrSelector == "string"
      ? document.querySelector(inputOrSelector)
      : inputOrSelector;
  input.value = inputValue;
  input.focus();
  const items = await (expectPopup ? popupByArrowDown() : noPopupByArrowDown());
  return { input, items };
}

satchelCommonSetup();

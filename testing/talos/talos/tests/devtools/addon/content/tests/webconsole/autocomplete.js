/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolbox,
  closeToolbox,
  runTest,
  testSetup,
  testTeardown,
} = require("../head");

const TEST_NAME = "console.autocomplete";

module.exports = async function() {
  await testSetup(`data:text/html,<meta charset=utf8><script>
    /*
     * Create an object with a null prototype in order to not have the autocomplete
     * popup polluted by Object prototype methods.
     */
    const items = Object.create(null);
    const itemsLength = 10000;
    for (let i = 0; i < itemsLength; i++) {
      const key = "item" + i;
      items[key] = i;
    }

    window.autocompleteTest = items;
  </script>`);

  const toolbox = await openToolbox("webconsole");
  const { hud } = toolbox.getPanel("webconsole");
  const { jsterm } = hud;

  const test = runTest(TEST_NAME);
  const ITERATIONS = 5;
  for (let i = 0; i < ITERATIONS; i++) {
    await showAndHideAutoCompletePopup(jsterm);
  }

  test.done();

  await closeToolbox();
  await testTeardown();
};

async function showAndHideAutoCompletePopup(jsterm) {
  await triggerAutocompletePopup(jsterm);
  await hideAutocompletePopup(jsterm);
}

async function triggerAutocompletePopup(jsterm) {
  const onPopupOpened = jsterm.autocompletePopup.once("popup-opened");
  setJsTermValueForCompletion(jsterm, "window.autocompleteTest.");
  await onPopupOpened;

  const onPopupUpdated = jsterm.once("autocomplete-updated");
  setJsTermValueForCompletion(jsterm, "window.autocompleteTest.item9");
  await onPopupUpdated;
}

function hideAutocompletePopup(jsterm) {
  let onPopUpClosed = jsterm.autocompletePopup.once("popup-closed");
  setJsTermValueForCompletion(jsterm, "");
  return onPopUpClosed;
}

function setJsTermValueForCompletion(jsterm, value) {
  // setInputValue does not trigger the autocompletion;
  // we need to call `updateAutocompletion` in order to display the popup. And since
  // setInputValue sets lastInputValue and updateAutocompletion checks it to trigger
  // the autocompletion request, we reset it.
  jsterm.setInputValue(value);
  jsterm.lastInputValue = null;
  jsterm.updateAutocompletion();
}


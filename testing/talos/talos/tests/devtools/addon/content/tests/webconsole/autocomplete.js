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
  waitForTick,
} = require("damp-test/tests/head");

const TEST_NAME = "console.autocomplete";

module.exports = async function () {
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
    await triggerAutocompletePopupAndUpdate(jsterm);
    await hideAutocompletePopup(jsterm);
  }
  test.done();

  const longInputTest = runTest(TEST_NAME + ".longInput");
  for (let i = 0; i < ITERATIONS; i++) {
    await triggerAutocompletePopup(jsterm, true);
    await hideAutocompletePopup(jsterm);
  }
  longInputTest.done();

  await closeToolbox();
  await testTeardown();
};

async function triggerAutocompletePopupAndUpdate(jsterm) {
  await triggerAutocompletePopup(jsterm);

  const onPopupUpdated = jsterm.once("autocomplete-updated");
  setJsTermValueForCompletion(jsterm, "window.autocompleteTest.item9");
  await onPopupUpdated;
  await waitForTick();
}

const LONG_INPUT_PREFIX = `var data = [ ${"{ hello : 'world', foo: [ 1, 2, 3] }, ".repeat(
  500
)}];`;

async function triggerAutocompletePopup(jsterm, withLongPrefix = false) {
  const onPopupOpened = jsterm.autocompletePopup.once("popup-opened");

  let inputValue = "window.autocompleteTest.";
  if (withLongPrefix) {
    inputValue = `${LONG_INPUT_PREFIX}\n${inputValue}`;
  }
  setJsTermValueForCompletion(jsterm, inputValue);
  await onPopupOpened;
}

async function hideAutocompletePopup(jsterm) {
  let onPopUpClosed = jsterm.autocompletePopup.once("popup-closed");
  setJsTermValueForCompletion(jsterm, "");
  await onPopUpClosed;
  await waitForTick();
}

function setJsTermValueForCompletion(jsterm, value) {
  // setInputValue does not trigger the autocompletion;
  // we need to call the `autocompleteUpdate` action in order to display the popup.
  jsterm._setValue(value);
  jsterm.props.autocompleteUpdate();
}

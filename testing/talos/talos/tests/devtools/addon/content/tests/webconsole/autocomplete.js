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
  jsterm.setInputValue("window.autocompleteTest.");
  const onPopupOpened = jsterm.autocompletePopup.once("popup-opened");
  // setInputValue does not trigger the autocompletion; we need to call `complete` in
  // order to display the popup.
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  await onPopupOpened;

  const onPopupUpdated = jsterm.once("autocomplete-updated");
  jsterm.setInputValue("window.autocompleteTest.item9");
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);

  await onPopupUpdated;
}

function hideAutocompletePopup(jsterm) {
  let onPopUpClosed = jsterm.autocompletePopup.once("popup-closed");
  jsterm.setInputValue("");
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  return onPopUpClosed;
}

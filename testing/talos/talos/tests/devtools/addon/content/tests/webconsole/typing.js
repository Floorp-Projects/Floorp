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

const TEST_NAME = "console.typing";
const LOGS_NUMBER = 500;

module.exports = async function () {
  const input = "abcdefghijklmnopqrst";
  await testSetup(`data:text/html,<!DOCTYPE html><meta charset=utf8><script>
    for (let i = 0; i < ${LOGS_NUMBER}; i++) {
      const key = "item" + i;
      console.log(i, key, [i], {key});
    }
    /* We add 2 global variables so the autocomplete popup will be displayed */
    ${input} = {};
    ${input}z = {};
  </script>`);

  const toolbox = await openToolbox("webconsole");
  const { hud } = toolbox.getPanel("webconsole");
  const { jsterm } = hud;

  // Wait for all the logs to be displayed.
  await waitFor(() => {
    const messages = Array.from(
      hud.ui.outputNode.querySelectorAll(".message-body")
    );
    return (
      messages.find(message => message.textContent.includes(`item0`)) &&
      messages.find(message =>
        message.textContent.includes(`item${LOGS_NUMBER - 1}`)
      )
    );
  });

  jsterm.focus();

  const test = runTest(TEST_NAME);

  // Simulate typing in the input.
  for (const char of Array.from(input)) {
    const onPopupOpened = jsterm.autocompletePopup.isOpen
      ? null
      : jsterm.autocompletePopup.once("popup-opened");
    const onAutocompleteUpdated = jsterm.once("autocomplete-updated");
    jsterm.insertStringAtCursor(char);
    // We need to trigger autocompletion update to show the popup.
    jsterm.props.autocompleteUpdate();
    await onAutocompleteUpdated;
    await onPopupOpened;
    await waitForTick();
  }

  test.done();
  const onPopupClosed = jsterm.autocompletePopup.isOpen
    ? jsterm.autocompletePopup.once("popup-closed")
    : null;
  jsterm.clearCompletion();
  await onPopupClosed;

  // Let's clear the output as it looks like not doing it could impact the next tests.
  const onMessagesCleared = waitFor(
    () => hud.ui.outputNode.querySelectorAll(".message").length === 0
  );
  hud.ui.clearOutput();
  await onMessagesCleared;

  await closeToolbox();
  await testTeardown();
};

async function waitFor(condition = () => true, delay = 50) {
  do {
    const res = condition();
    if (res) {
      return res;
    }
    await new Promise(resolve => setTimeout(resolve, delay));
  } while (true);
}

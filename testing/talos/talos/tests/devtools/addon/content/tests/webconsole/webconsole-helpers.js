/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  reloadPageAndLog,
  waitForDOMPredicate,
} = require("damp-test/tests/head");

/**
 * @param {String} label: The name of the test.
 * @param {Toolbox} toolbox: The DevTools toolbox.
 * @param {Number|Array} expectedMessages: This can be, either the number of messages that
 *        need to be displayed in the console, or an array of objects representing the
 *        messages that need to be in the output. The objects must have the following shape:
 *          - {String} text: A string that should be in the message.
 *          - {Number} count: If > 1, indicate how many messages with this text should be
 *                            in the output.
 *          - {Boolean} stacktrace: If true, wait for a stacktrace element to be rendered.
 */
exports.reloadConsoleAndLog = async function (
  label,
  toolbox,
  expectedMessages
) {
  const webConsole = toolbox.getPanel("webconsole");
  const onWebConsoleReload = webConsole.once("reloaded");
  const onReload = async function () {
    const { hud } = webConsole;
    const expected =
      typeof expectedMessages === "number"
        ? [{ text: "", count: expectedMessages }]
        : expectedMessages;

    let logMissingMessagesTimeoutId;

    // Wait for webconsole panel reload in order to prevent matching messages from previous
    // page load in the code below.
    await onWebConsoleReload;

    const checkMessages = consoleOutputEl => {
      if (logMissingMessagesTimeoutId) {
        clearTimeout(logMissingMessagesTimeoutId);
        logMissingMessagesTimeoutId = null;
      }

      const messages = Array.from(consoleOutputEl.querySelectorAll(".message"));
      const missing = new Map(expected.map(e => [e.text, e.count || 1]));

      for (const { text, count = 1, stacktrace } of expected) {
        let found = 0;
        for (const message of messages) {
          const messageText = message.querySelector(".message-body").innerText;
          if (
            messageText.includes(text) &&
            (!stacktrace || message.querySelector(".frames .frame"))
          ) {
            const repeat = message
              .querySelector(".message-repeats")
              ?.innerText?.trim();
            found = found + (repeat ? parseInt(repeat) : 1);
          }
        }
        const allFound = found >= count;

        if (allFound) {
          missing.delete(text);
        } else {
          missing.set(text, count - found);
        }
      }

      const foundAllMessages = missing.size == 0;
      if (!foundAllMessages) {
        // Only log missing messages after 3s, if there was no other DOM updates.
        logMissingMessagesTimeoutId = setTimeout(() => {
          dump(
            `[TEST_LOG] Still waiting for the following messages: \n${Array.from(
              missing.entries()
            )
              .map(([text, count]) => `${text || "<any text>"} (✕${count})`)
              .join("\n")}\n`
          );
          dump("---\n");
        }, 3000);
      }
      return foundAllMessages;
    };

    // Messages might already be displayed in the console
    if (checkMessages(getConsoleOutputElement(hud))) {
      return;
    }
    // if all messages weren't there, wait for mutations and check again
    await waitForConsoleOutputChildListChange(hud, checkMessages);
  };
  await reloadPageAndLog(label + ".webconsole", toolbox, onReload);
};

/**
 * Given a WebConsole instance and a predicate, listen for the console output childList
 * mutation and returns a Promise that will resolve when the `predicate` function returns
 * true.
 *
 * @param {WebConsole} hud
 * @param {Function} predicate: This is the function called on each childList mutation,
 *                              with the console output element. Make the promise resolves
 *                              when it returns `true`.
 */
async function waitForConsoleOutputChildListChange(hud, predicate) {
  const webConsoleOutputEl = getConsoleOutputElement(hud);

  await waitForDOMPredicate(
    webConsoleOutputEl,
    () => predicate(webConsoleOutputEl),
    { childList: true, subtree: true }
  );
}
exports.waitForConsoleOutputChildListChange =
  waitForConsoleOutputChildListChange;

/**
 * Return the webconsole output element from the hud.
 *
 * @param {WebConsole} hud
 * @returns {Element}
 */
function getConsoleOutputElement(hud) {
  return hud.ui.document.querySelector(".webconsole-output");
}

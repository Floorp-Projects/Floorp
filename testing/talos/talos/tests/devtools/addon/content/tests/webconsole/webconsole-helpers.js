/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { reloadPageAndLog } = require("../head");

/**
 * @param {String} label: The name of the test.
 * @param {Toolbox} toolbox: The DevTools toolbox.
 * @param {Number|Array} expectedMessages: This can be, either the number of messages that
 *        need to be displayed in the console, or an array of objects representing the
 *        messages that need to be in the output. The objects must have the following shape:
 *          - {String} text: A string that should be in the message.
 *          - {Number} count: If > 1, indicate how many messages with this text should be
 *                            in the output.
 */
exports.reloadConsoleAndLog = async function(label, toolbox, expectedMessages) {
  const onReload = async function() {
    const { hud } = toolbox.getPanel("webconsole");

    const expected =
      typeof expectedMessages === "number"
        ? [{ text: "", count: expectedMessages }]
        : expectedMessages;

    await waitForConsoleOutputChildListChange(hud, consoleOutputEl => {
      dump("[TEST_LOG] Console output changed - checking content:\n");
      const messages = Array.from(consoleOutputEl.querySelectorAll(".message"));

      const missing = new Map(expected.map(e => [e.text, e.count || 1]));

      const foundAllMessages = expected.every(({ text, count = 1 }) => {
        let found = 0;
        for (const message of messages) {
          const messageText = message.querySelector(".message-body").innerText;
          if (messageText.includes(text)) {
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

        return allFound;
      });

      if (!foundAllMessages) {
        dump(
          `[TEST_LOG] Still waiting for the following messages: \n${Array.from(
            missing.entries()
          )
            .map(([text, count]) => `${text || "<any text>"} (✕${count})`)
            .join("\n")}\n`
        );
      } else {
        dump(`[TEST_LOG] All expected messages where found\n`);
      }
      dump("---\n");
      return foundAllMessages;
    });
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
  const { window, document } = hud.ui;
  const webConsoleOutputEl = document.querySelector(".webconsole-output");

  await new Promise(resolve => {
    const observer = new window.MutationObserver((mutationsList, observer) => {
      if (predicate(webConsoleOutputEl)) {
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(webConsoleOutputEl, { childList: true });
  });
}
exports.waitForConsoleOutputChildListChange = waitForConsoleOutputChildListChange;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { reloadPageAndLog } = require("../head");

exports.reloadConsoleAndLog = async function(label, toolbox, expectedMessages) {
  const onReload = async function() {
    const { hud } = toolbox.getPanel("webconsole");

    await waitForConsoleOutputChildListChange(hud, consoleOutputEl => {
      const messageCount = consoleOutputEl.querySelectorAll(".message").length;
      return messageCount >= expectedMessages;
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

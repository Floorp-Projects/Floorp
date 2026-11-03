// SPDX-License-Identifier: MPL-2.0
/// <reference types="../../../libs/@types/gecko/index.d.ts" />
(() => {
  const fixtureGlobals =
    /** @type {import("../offscreen-test-types.ts").FixtureGlobals} */ (/** @type {unknown} */ (globalThis));
  /** @type {import("../offscreen-test-types.ts").FixtureChrome} */
  const chrome = fixtureGlobals.chrome;
  chrome.runtime.onMessage.addListener(
    /** @param {import("../offscreen-test-types.ts").FixtureMessage} message
     * @param {{id?: string}} sender
     * @param {(reply: import("../offscreen-test-types.ts").FixtureMessageReply | import("../offscreen-test-types.ts").AuxiliaryContentReport | {hasDocument: boolean; importScripts: string}) => void} respond */
    (message, sender, respond) => {
      if (message.target === "background") {
        respond({
          value: message.value,
          senderId: sender.id,
          receiverId: chrome.runtime.id,
        });
      } else if (message.target === "background-info") {
        respond({
          hasDocument: typeof document !== "undefined",
          importScripts: typeof fixtureGlobals.importScripts,
        });
      }
    },
  );
  chrome.runtime.onConnect.addListener(
    /** @param {import("../offscreen-test-types.ts").FixturePort} port */ (
      port,
    ) => {
      if (port.name === "comparison-background") {
        port.onMessage.addListener(
          /** @param {import("../offscreen-test-types.ts").FixtureMessage} message */ (
            message,
          ) =>
            port.postMessage({
              value: message.value,
              senderId: port.sender?.id,
              receiverId: chrome.runtime.id,
            }),
        );
      }
    },
  );
})();

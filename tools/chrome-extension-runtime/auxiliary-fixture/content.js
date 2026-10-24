// SPDX-License-Identifier: MPL-2.0
/// <reference types="../../../libs/@types/gecko/index.d.ts" />
(() => {
  const fixtureGlobals =
    /** @type {import("../offscreen-test-types.ts").FixtureGlobals} */ (/** @type {unknown} */ (globalThis));
  /** @type {import("../offscreen-test-types.ts").FixtureChrome} */
  const chrome = fixtureGlobals.chrome;
  const document = fixtureGlobals.document;
  chrome.runtime.onMessage.addListener(
    /** @param {import("../offscreen-test-types.ts").FixtureMessage} message
     * @param {{id?: string}} _sender
     * @param {(reply: import("../offscreen-test-types.ts").FixtureMessageReply | import("../offscreen-test-types.ts").AuxiliaryContentReport | {hasDocument: boolean; importScripts: string}) => void} respond */
    (message, _sender, respond) => {
      if (message?.auxiliaryDeclarative) {
        if (message.removeMatch) document.querySelector("#item")?.remove();
        respond({
          injected:
            /** @type {HTMLElement} */ (document.documentElement).dataset
              .nativeDeclarative || "0",
        });
        return false;
      }
      if (message?.auxiliaryProbe) {
        (async () => {
          const area = message.area || "session";
          /** @type {import("../offscreen-test-types.ts").AuxiliaryContentReport} */
          const report = { sessionType: typeof chrome.storage[area] };
          try {
            report.value = await chrome.storage[area].get("contractValue");
            if (message.setAccessLevel) {
              await chrome.storage[area].setAccessLevel({
                accessLevel: "TRUSTED_AND_UNTRUSTED_CONTEXTS",
              });
            }
          } catch (error) {
            report.error = String(error);
          }
          respond(report);
        })();
        return true;
      }
    },
  );
})();

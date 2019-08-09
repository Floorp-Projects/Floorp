/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function linkColorFrameScript() {
  addMessageListener("HistoryDelegateTest:GetLinkColor", function onMessage(
    message
  ) {
    const { selector, uri } = message.data;

    if (content.document.documentURI != uri) {
      return;
    }
    const element = content.document.querySelector(selector);
    if (!element) {
      sendAsyncMessage("HistoryDelegateTest:GetLinkColor", {
        ok: false,
        error: "No element for " + selector,
      });
      return;
    }
    const color = content.windowUtils.getVisitedDependentComputedStyle(
      element,
      "",
      "color"
    );
    sendAsyncMessage("HistoryDelegateTest:GetLinkColor", { ok: true, color });
  });
}

this.test = class extends ExtensionAPI {
  getAPI(context) {
    return {
      test: {
        /* Set prefs and returns set of saved prefs */
        async setPrefs(oldPrefs, newPrefs) {
          // Save old prefs
          Object.assign(
            oldPrefs,
            ...Object.keys(newPrefs)
              .filter(key => !(key in oldPrefs))
              .map(key => ({ [key]: Preferences.get(key, null) }))
          );

          // Set new prefs
          Preferences.set(newPrefs);
          return oldPrefs;
        },

        /* Restore prefs to old value. */
        async restorePrefs(oldPrefs) {
          for (let [name, value] of Object.entries(oldPrefs)) {
            if (value === null) {
              Preferences.reset(name);
            } else {
              Preferences.set(name, value);
            }
          }
        },

        /* Get pref values. */
        async getPrefs(prefs) {
          return Preferences.get(prefs);
        },

        /* Gets link color for a given selector. */
        async getLinkColor(uri, selector) {
          const frameScript = `data:text/javascript,(${encodeURI(
            linkColorFrameScript
          )}).call(this)`;
          Services.mm.loadFrameScript(frameScript, true);

          return new Promise((resolve, reject) => {
            const onMessage = message => {
              Services.mm.removeMessageListener(
                "HistoryDelegateTest:GetLinkColor",
                onMessage
              );
              if (message.data.ok) {
                resolve(message.data.color);
              } else {
                reject(message.data.error);
              }
            };

            Services.mm.addMessageListener(
              "HistoryDelegateTest:GetLinkColor",
              onMessage
            );
            Services.mm.broadcastAsyncMessage(
              "HistoryDelegateTest:GetLinkColor",
              { uri, selector }
            );
          });
        },

        async getRequestedLocales() {
          return Services.locale.requestedLocales;
        },

        async addHistogram(id, value) {
          return Services.telemetry.getHistogramById(id).add(value);
        },
      },
    };
  }
};

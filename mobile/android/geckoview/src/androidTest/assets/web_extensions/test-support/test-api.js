/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);
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

function setResolutionAndScaleToFrameScript(resolution) {
  addMessageListener("PanZoomControllerTest:SetResolutionAndScaleTo", () => {
    content.window.visualViewport.addEventListener("resize", () => {
      sendAsyncMessage("PanZoomControllerTest:SetResolutionAndScaleTo");
    });
    content.windowUtils.setResolutionAndScaleTo(resolution);
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
          for (const [name, value] of Object.entries(oldPrefs)) {
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

        async getPidForTab(tabId) {
          const tab = context.extension.tabManager.get(tabId);
          const pids = E10SUtils.getBrowserPids(tab.browser);
          return pids[0];
        },

        async addHistogram(id, value) {
          return Services.telemetry.getHistogramById(id).add(value);
        },

        removeCertOverride(host, port) {
          const overrideService = Cc[
            "@mozilla.org/security/certoverride;1"
          ].getService(Ci.nsICertOverrideService);
          overrideService.clearValidityOverride(host, port);
        },

        async setScalar(id, value) {
          return Services.telemetry.scalarSet(id, value);
        },

        async setResolutionAndScaleTo(resolution) {
          const frameScript = `data:text/javascript,(${encodeURI(
            setResolutionAndScaleToFrameScript
          )}).call(this, ${resolution})`;
          Services.mm.loadFrameScript(frameScript, true);

          return new Promise(resolve => {
            const onMessage = () => {
              Services.mm.removeMessageListener(
                "PanZoomControllerTest:SetResolutionAndScaleTo",
                onMessage
              );
              resolve();
            };

            Services.mm.addMessageListener(
              "PanZoomControllerTest:SetResolutionAndScaleTo",
              onMessage
            );
            Services.mm.broadcastAsyncMessage(
              "PanZoomControllerTest:SetResolutionAndScaleTo"
            );
          });
        },
      },
    };
  }
};

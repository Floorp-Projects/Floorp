/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gRuntimeTimeout",
  "extensions.webextensions.runtime.timeout",
  5000
);

this.runtime = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    // Despite not being part of PERSISTENT_EVENTS, the following events are
    // still triggered (after waking up the background context if needed):
    // - runtime.onConnect
    // - runtime.onConnectExternal
    // - runtime.onMessage
    // - runtime.onMessageExternal
    // For details, see bug 1852317 and test_ext_eventpage_messaging_wakeup.js.

    onInstalled({ fire }) {
      let { extension } = this;
      let temporary = !!extension.addonData.temporarilyInstalled;

      let listener = () => {
        switch (extension.startupReason) {
          case "APP_STARTUP":
            if (AddonManagerPrivate.browserUpdated) {
              fire.sync({ reason: "browser_update", temporary });
            }
            break;
          case "ADDON_INSTALL":
            fire.sync({ reason: "install", temporary });
            break;
          case "ADDON_UPGRADE":
            fire.sync({
              reason: "update",
              previousVersion: extension.addonData.oldVersion,
              temporary,
            });
            break;
        }
      };
      extension.on("background-first-run", listener);
      return {
        unregister() {
          extension.off("background-first-run", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onUpdateAvailable({ fire }) {
      let { extension } = this;
      let instanceID = extension.addonData.instanceID;
      AddonManager.addUpgradeListener(instanceID, upgrade => {
        extension.upgrade = upgrade;
        let details = {
          version: upgrade.version,
        };
        fire.sync(details);
      });
      return {
        unregister() {
          AddonManager.removeUpgradeListener(instanceID);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    let { extension } = context;
    return {
      runtime: {
        // onStartup is special-cased in ext-backgroundPages to cause
        // an immediate startup.  We do not prime onStartup.
        onStartup: new EventManager({
          context,
          module: "runtime",
          event: "onStartup",
          register: fire => {
            if (context.incognito || extension.startupReason != "APP_STARTUP") {
              // This event should not fire if we are operating in a private profile.
              return () => {};
            }
            let listener = () => {
              return fire.sync();
            };

            extension.on("background-first-run", listener);

            return () => {
              extension.off("background-first-run", listener);
            };
          },
        }).api(),

        onInstalled: new EventManager({
          context,
          module: "runtime",
          event: "onInstalled",
          extensionApi: this,
        }).api(),

        onUpdateAvailable: new EventManager({
          context,
          module: "runtime",
          event: "onUpdateAvailable",
          extensionApi: this,
        }).api(),

        onSuspend: new EventManager({
          context,
          name: "runtime.onSuspend",
          resetIdleOnEvent: false,
          register: fire => {
            let listener = async () => {
              let timedOut = false;
              async function promiseFire() {
                try {
                  await fire.async();
                } catch (e) {}
              }
              await Promise.race([
                promiseFire(),
                ExtensionUtils.promiseTimeout(gRuntimeTimeout).then(() => {
                  timedOut = true;
                }),
              ]);
              if (timedOut) {
                Cu.reportError(
                  `runtime.onSuspend in ${extension.id} took too long`
                );
              }
            };
            extension.on("background-script-suspend", listener);
            return () => {
              extension.off("background-script-suspend", listener);
            };
          },
        }).api(),

        onSuspendCanceled: new EventManager({
          context,
          name: "runtime.onSuspendCanceled",
          register: fire => {
            let listener = () => {
              fire.async();
            };
            extension.on("background-script-suspend-canceled", listener);
            return () => {
              extension.off("background-script-suspend-canceled", listener);
            };
          },
        }).api(),

        reload: async () => {
          if (extension.upgrade) {
            // If there is a pending update, install it now.
            extension.upgrade.install();
          } else {
            // Otherwise, reload the current extension.
            let addon = await AddonManager.getAddonByID(extension.id);
            addon.reload();
          }
        },

        get lastError() {
          // TODO(robwu): Figure out how to make sure that errors in the parent
          // process are propagated to the child process.
          // lastError should not be accessed from the parent.
          return context.lastError;
        },

        getBrowserInfo: function () {
          const { name, vendor, version, appBuildID } = Services.appinfo;
          const info = { name, vendor, version, buildID: appBuildID };
          return Promise.resolve(info);
        },

        getPlatformInfo: function () {
          return Promise.resolve(ExtensionParent.PlatformInfo);
        },

        openOptionsPage: function () {
          if (!extension.manifest.options_ui) {
            return Promise.reject({ message: "No `options_ui` declared" });
          }

          // This expects openOptionsPage to be defined in the file using this,
          // e.g. the browser/ version of ext-runtime.js
          /* global openOptionsPage:false */
          return openOptionsPage(extension).then(() => {});
        },

        setUninstallURL: function (url) {
          if (url === null || url.length === 0) {
            extension.uninstallURL = null;
            return Promise.resolve();
          }

          let uri;
          try {
            uri = new URL(url);
          } catch (e) {
            return Promise.reject({
              message: `Invalid URL: ${JSON.stringify(url)}`,
            });
          }

          if (uri.protocol != "http:" && uri.protocol != "https:") {
            return Promise.reject({
              message: "url must have the scheme http or https",
            });
          }

          extension.uninstallURL = url;
          return Promise.resolve();
        },

        // This function is not exposed to the extension js code and it is only
        // used by the alert function redefined into the background pages to be
        // able to open the BrowserConsole from the main process.
        openBrowserConsole() {
          if (AppConstants.platform !== "android") {
            DevToolsShim.openBrowserConsole();
          }
        },

        async internalWakeupBackground() {
          const { background } = extension.manifest;
          if (
            background &&
            (background.page || background.scripts) &&
            // Note: if background.service_worker is specified, it takes
            // precedence over page/scripts, and persistentBackground is false.
            !extension.persistentBackground
          ) {
            await extension.wakeupBackground();
          }
        },
      },
    };
  }
};

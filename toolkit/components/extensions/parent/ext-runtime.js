/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file expects tabTracker to be defined in the global scope (e.g.
// by ext-browser.js or ext-android.js).
/* global tabTracker */

var { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs"
);

var { DefaultWeakMap } = ExtensionUtils;

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
    onPerformanceWarning({ fire }) {
      let { extension } = this;

      let observer = subject => {
        let report = subject.QueryInterface(Ci.nsIHangReport);

        if (report?.addonId !== extension.id) {
          return;
        }

        const performanceWarningEventDetails = {
          category: "content_script",
          severity: "high",
          description:
            "Slow extension content script caused a page hang, user was warned.",
        };

        let scriptBrowser = report.scriptBrowser;
        let nativeTab =
          scriptBrowser?.ownerGlobal.gBrowser?.getTabForBrowser(scriptBrowser);
        if (nativeTab) {
          performanceWarningEventDetails.tabId = tabTracker.getId(nativeTab);
        }

        fire.async(performanceWarningEventDetails);
      };

      Services.obs.addObserver(observer, "process-hang-report");
      return {
        unregister: () => {
          Services.obs.removeObserver(observer, "process-hang-report");
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  // Although we have an internal context.contextId field, we generate a new one here because
  // the internal type is an integer and the public field a (UUID) string.
  // TODO: Move the implementation elsewhere when contextId is used anywhere other than
  // runtime.getContexts. See https://bugzilla.mozilla.org/show_bug.cgi?id=1628178#c5
  //
  // Map<ProxyContextParent, string>
  #contextUUIDMap = new DefaultWeakMap(_context =>
    String(Services.uuid.generateUUID()).slice(1, -1)
  );

  getContexts(filter) {
    const { extension } = this;
    const { proxyContexts } = ExtensionParent.ParentAPIManager;
    const results = [];
    for (const proxyContext of proxyContexts.values()) {
      if (proxyContext.extension !== extension) {
        continue;
      }
      let ctx;
      try {
        ctx = proxyContext.toExtensionContext();
      } catch (err) {
        // toExtensionContext may throw if the contextType getter
        // raised an exception due to an internal viewType has
        // not be mapped with a contextType value.
        //
        // When running in DEBUG builds we reject the getContexts
        // call, while in non DEBUG build we just omit the result
        // and log a warning in the Browser Console.
        if (AppConstants.DEBUG) {
          throw err;
        } else {
          Cu.reportError(err);
        }
      }

      if (this.matchContextFilter(filter, ctx)) {
        results.push({
          ...ctx,
          contextId: this.#contextUUIDMap.get(proxyContext),
        });
      }
    }
    return results;
  }

  matchContextFilter(filter, ctx) {
    if (!ctx) {
      // Filter out subclasses that do not return any ExtensionContext details
      // from their toExtensionContext method.
      return false;
    }
    if (filter.contextIds && !filter.contextIds.includes(ctx.contextId)) {
      return false;
    }
    if (filter.contextTypes && !filter.contextTypes.includes(ctx.contextType)) {
      return false;
    }
    if (filter.documentIds && !filter.documentIds.includes(ctx.documentId)) {
      return false;
    }
    if (
      filter.documentOrigins &&
      !filter.documentOrigins.includes(ctx.documentOrigin)
    ) {
      return false;
    }
    if (filter.documentUrls && !filter.documentUrls.includes(ctx.documentUrl)) {
      return false;
    }
    if (filter.frameIds && !filter.frameIds.includes(ctx.frameId)) {
      return false;
    }
    if (filter.tabIds && !filter.tabIds.includes(ctx.tabId)) {
      return false;
    }
    if (filter.windowIds && !filter.windowIds.includes(ctx.windowId)) {
      return false;
    }
    if (
      typeof filter.incognito === "boolean" &&
      ctx.incognito !== filter.incognito
    ) {
      return false;
    }
    return true;
  }

  getAPI(context) {
    let { extension } = context;
    return {
      runtime: {
        getContexts: filter => this.getContexts(filter),

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

        onPerformanceWarning: new EventManager({
          context,
          module: "runtime",
          event: "onPerformanceWarning",
          extensionApi: this,
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
          if (!extension.optionsPageProperties) {
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

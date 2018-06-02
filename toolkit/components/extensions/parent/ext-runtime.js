"use strict";

ChromeUtils.defineModuleGetter(this, "AddonManager",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManagerPrivate",
                               "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "ExtensionParent",
                               "resource://gre/modules/ExtensionParent.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "DevToolsShim",
                               "chrome://devtools-startup/content/DevToolsShim.jsm");

this.runtime = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      runtime: {
        onStartup: new EventManager({
          context,
          name: "runtime.onStartup",
          register: fire => {
            if (context.incognito) {
              // This event should not fire if we are operating in a private profile.
              return () => {};
            }
            let listener = () => {
              if (extension.startupReason === "APP_STARTUP") {
                fire.sync();
              }
            };
            extension.on("startup", listener);
            return () => {
              extension.off("startup", listener);
            };
          },
        }).api(),

        onInstalled: new EventManager({
          context,
          name: "runtime.onInstalled",
          register: fire => {
            let temporary = !!extension.addonData.temporarilyInstalled;

            let listener = () => {
              switch (extension.startupReason) {
                case "APP_STARTUP":
                  if (AddonManagerPrivate.browserUpdated) {
                    fire.sync({reason: "browser_update", temporary});
                  }
                  break;
                case "ADDON_INSTALL":
                  fire.sync({reason: "install", temporary});
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
            extension.on("startup", listener);
            return () => {
              extension.off("startup", listener);
            };
          },
        }).api(),

        onUpdateAvailable: new EventManager({
          context,
          name: "runtime.onUpdateAvailable",
          register: fire => {
            let instanceID = extension.addonData.instanceID;
            AddonManager.addUpgradeListener(instanceID, upgrade => {
              extension.upgrade = upgrade;
              let details = {
                version: upgrade.version,
              };
              fire.sync(details);
            });
            return () => {
              AddonManager.removeUpgradeListener(instanceID).catch(e => {
                // This can happen if we try this after shutdown is complete.
              });
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

        getBrowserInfo: function() {
          const {name, vendor, version, appBuildID} = Services.appinfo;
          const info = {name, vendor, version, buildID: appBuildID};
          return Promise.resolve(info);
        },

        getPlatformInfo: function() {
          return Promise.resolve(ExtensionParent.PlatformInfo);
        },

        openOptionsPage: function() {
          if (!extension.manifest.options_ui) {
            return Promise.reject({message: "No `options_ui` declared"});
          }

          // This expects openOptionsPage to be defined in the file using this,
          // e.g. the browser/ version of ext-runtime.js
          /* global openOptionsPage:false */
          return openOptionsPage(extension).then(() => {});
        },

        setUninstallURL: function(url) {
          if (url.length == 0) {
            return Promise.resolve();
          }

          let uri;
          try {
            uri = new URL(url);
          } catch (e) {
            return Promise.reject({message: `Invalid URL: ${JSON.stringify(url)}`});
          }

          if (uri.protocol != "http:" && uri.protocol != "https:") {
            return Promise.reject({message: "url must have the scheme http or https"});
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
      },
    };
  }
};

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

this.runtime = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      runtime: {
        onStartup: new SingletonEventManager(context, "runtime.onStartup", fire => {
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
        }).api(),

        onInstalled: new SingletonEventManager(context, "runtime.onInstalled", fire => {
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
        }).api(),

        onUpdateAvailable: new SingletonEventManager(context, "runtime.onUpdateAvailable", fire => {
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
        }).api(),

        reload: () => {
          if (extension.upgrade) {
            // If there is a pending update, install it now.
            extension.upgrade.install();
          } else {
            // Otherwise, reload the current extension.
            AddonManager.getAddonByID(extension.id, addon => {
              addon.reload();
            });
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
          return Promise.resolve(ExtensionUtils.PlatformInfo);
        },

        openOptionsPage: function() {
          if (!extension.manifest.options_ui) {
            return Promise.reject({message: "No `options_ui` declared"});
          }

          return openOptionsPage(extension).then(() => {});
        },

        setUninstallURL: function(url) {
          if (url.length == 0) {
            return Promise.resolve();
          }

          let uri;
          try {
            uri = NetUtil.newURI(url);
          } catch (e) {
            return Promise.reject({message: `Invalid URL: ${JSON.stringify(url)}`});
          }

          if (uri.scheme != "http" && uri.scheme != "https") {
            return Promise.reject({message: "url must have the scheme http or https"});
          }

          extension.uninstallURL = url;
          return Promise.resolve();
        },
      },
    };
  }
};

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  const stringSvc = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
  return stringSvc.createBundle("chrome://global/locale/extensions.properties");
});
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "promptService",
                                   "@mozilla.org/embedcomp/prompt-service;1",
                                   "nsIPromptService");
XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyGetter(this, "GlobalManager", () => {
  const {GlobalManager} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return GlobalManager;
});

var {
  ExtensionError,
} = ExtensionUtils;

function _(key, ...args) {
  if (args.length) {
    return strBundle.formatStringFromName(key, args, args.length);
  }
  return strBundle.GetStringFromName(key);
}

function installType(addon) {
  if (addon.temporarilyInstalled) {
    return "development";
  } else if (addon.foreignInstall) {
    return "sideload";
  } else if (addon.isSystem) {
    return "other";
  }
  return "normal";
}

function getExtensionInfoForAddon(extension, addon) {
  let extInfo = {
    id: addon.id,
    name: addon.name,
    description: addon.description || "",
    version: addon.version,
    mayDisable: !!(addon.permissions & AddonManager.PERM_CAN_DISABLE),
    enabled: addon.isActive,
    optionsUrl: addon.optionsURL || "",
    installType: installType(addon),
    type: addon.type,
  };

  if (extension) {
    let m = extension.manifest;

    let hostPerms = extension.whiteListedHosts.patterns.map(matcher => matcher.pattern);

    extInfo.permissions = Array.from(extension.permissions).filter(perm => {
      return !hostPerms.includes(perm);
    });
    extInfo.hostPermissions = hostPerms;

    extInfo.shortName = m.short_name || "";
    if (m.icons) {
      extInfo.icons = Object.keys(m.icons).map(key => {
        return {size: Number(key), url: m.icons[key]};
      });
    }
  }

  if (!addon.isActive) {
    extInfo.disabledReason = "unknown";
  }
  if (addon.homepageURL) {
    extInfo.homepageUrl = addon.homepageURL;
  }
  if (addon.updateURL) {
    extInfo.updateUrl = addon.updateURL;
  }
  return extInfo;
}

const listenerMap = new WeakMap();
// Some management APIs are intentionally limited.
const allowedTypes = ["theme"];

class AddonListener {
  constructor() {
    AddonManager.addAddonListener(this);
    EventEmitter.decorate(this);
  }

  release() {
    AddonManager.removeAddonListener(this);
  }

  getExtensionInfo(addon) {
    let ext = addon.isWebExtension && GlobalManager.extensionMap.get(addon.id);
    return getExtensionInfoForAddon(ext, addon);
  }

  onEnabled(addon) {
    if (!allowedTypes.includes(addon.type)) {
      return;
    }
    this.emit("onEnabled", this.getExtensionInfo(addon));
  }

  onDisabled(addon) {
    if (!allowedTypes.includes(addon.type)) {
      return;
    }
    this.emit("onDisabled", this.getExtensionInfo(addon));
  }

  onInstalled(addon) {
    if (!allowedTypes.includes(addon.type)) {
      return;
    }
    this.emit("onInstalled", this.getExtensionInfo(addon));
  }

  onUninstalled(addon) {
    if (!allowedTypes.includes(addon.type)) {
      return;
    }
    this.emit("onUninstalled", this.getExtensionInfo(addon));
  }
}

let addonListener;

function getListener(extension, context) {
  if (!listenerMap.has(extension)) {
    if (!addonListener) {
      addonListener = new AddonListener();
    }
    listenerMap.set(extension, {});
    context.callOnClose({
      close: () => {
        listenerMap.delete(extension);
        if (listenerMap.length === 0) {
          addonListener.release();
          addonListener = null;
        }
      },
    });
  }
  return addonListener;
}

this.management = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      management: {
        async getAll() {
          let addons = await AddonManager.getAddonsByTypes(allowedTypes);
          return addons.map(addon => {
            // If the extension is enabled get it and use it for more data.
            let ext = addon.isWebExtension && GlobalManager.extensionMap.get(addon.id);
            return getExtensionInfoForAddon(ext, addon);
          });
        },

        async getSelf() {
          let addon = await AddonManager.getAddonByID(extension.id);
          return getExtensionInfoForAddon(extension, addon);
        },

        async uninstallSelf(options) {
          if (options && options.showConfirmDialog) {
            let message = _("uninstall.confirmation.message", extension.name);
            if (options.dialogMessage) {
              message = `${options.dialogMessage}\n${message}`;
            }
            let title = _("uninstall.confirmation.title", extension.name);
            let buttonFlags = promptService.BUTTON_POS_0 * promptService.BUTTON_TITLE_IS_STRING +
                              promptService.BUTTON_POS_1 * promptService.BUTTON_TITLE_IS_STRING;
            let button0Title = _("uninstall.confirmation.button-0.label");
            let button1Title = _("uninstall.confirmation.button-1.label");
            let response = promptService.confirmEx(null, title, message, buttonFlags, button0Title, button1Title, null, null, {value: 0});
            if (response == 1) {
              throw new ExtensionError("User cancelled uninstall of extension");
            }
          }
          let addon = await AddonManager.getAddonByID(extension.id);
          let canUninstall = Boolean(addon.permissions & AddonManager.PERM_CAN_UNINSTALL);
          if (!canUninstall) {
            throw new ExtensionError("The add-on cannot be uninstalled");
          }
          addon.uninstall();
        },

        async setEnabled(id, enabled) {
          let addon = await AddonManager.getAddonByID(id);
          if (!addon) {
            throw new ExtensionError(`No such addon ${id}`);
          }
          if (!allowedTypes.includes(addon.type)) {
            throw new ExtensionError("setEnabled applies only to theme addons");
          }
          addon.userDisabled = !enabled;
        },

        onDisabled: new SingletonEventManager(context, "management.onDisabled", fire => {
          let listener = (event, data) => {
            fire.async(data);
          };

          getListener(extension, context).on("onDisabled", listener);
          return () => {
            getListener(extension, context).off("onDisabled", listener);
          };
        }).api(),

        onEnabled: new SingletonEventManager(context, "management.onEnabled", fire => {
          let listener = (event, data) => {
            fire.async(data);
          };

          getListener(extension, context).on("onEnabled", listener);
          return () => {
            getListener(extension, context).off("onEnabled", listener);
          };
        }).api(),

        onInstalled: new SingletonEventManager(context, "management.onInstalled", fire => {
          let listener = (event, data) => {
            fire.async(data);
          };

          getListener(extension, context).on("onInstalled", listener);
          return () => {
            getListener(extension, context).off("onInstalled", listener);
          };
        }).api(),

        onUninstalled: new SingletonEventManager(context, "management.onUninstalled", fire => {
          let listener = (event, data) => {
            fire.async(data);
          };

          getListener(extension, context).on("onUninstalled", listener);
          return () => {
            getListener(extension, context).off("onUninstalled", listener);
          };
        }).api(),

      },
    };
  }
};

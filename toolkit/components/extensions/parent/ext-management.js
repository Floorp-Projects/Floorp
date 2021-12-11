/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  return Services.strings.createBundle(
    "chrome://global/locale/extensions.properties"
  );
});
ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

// We can't use Services.prompt here at the moment, as tests need to mock
// the prompt service. We could use sinon, but that didn't seem to work
// with Android builds.
// eslint-disable-next-line mozilla/use-services
XPCOMUtils.defineLazyServiceGetter(
  this,
  "promptService",
  "@mozilla.org/embedcomp/prompt-service;1",
  "nsIPromptService"
);

var { ExtensionError } = ExtensionUtils;

const _ = (key, ...args) => {
  if (args.length) {
    return strBundle.formatStringFromName(key, args);
  }
  return strBundle.GetStringFromName(key);
};

const installType = addon => {
  if (addon.temporarilyInstalled) {
    return "development";
  } else if (addon.foreignInstall) {
    return "sideload";
  } else if (addon.isSystem) {
    return "other";
  }
  return "normal";
};

const getExtensionInfoForAddon = (extension, addon) => {
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

    let hostPerms = extension.allowedOrigins.patterns.map(
      matcher => matcher.pattern
    );

    extInfo.permissions = Array.from(extension.permissions).filter(perm => {
      return !hostPerms.includes(perm);
    });
    extInfo.hostPermissions = hostPerms;

    extInfo.shortName = m.short_name || "";
    if (m.icons) {
      extInfo.icons = Object.keys(m.icons).map(key => {
        return { size: Number(key), url: m.icons[key] };
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
};

const listenerMap = new WeakMap();
// Some management APIs are intentionally limited.
const allowedTypes = ["theme", "extension"];

function checkAllowedAddon(addon) {
  if (addon.isSystem || addon.isAPIExtension) {
    return false;
  }
  if (addon.type == "extension" && !addon.isWebExtension) {
    return false;
  }
  return allowedTypes.includes(addon.type);
}

class AddonListener extends ExtensionCommon.EventEmitter {
  constructor() {
    super();
    AddonManager.addAddonListener(this);
  }

  release() {
    AddonManager.removeAddonListener(this);
  }

  getExtensionInfo(addon) {
    let ext = WebExtensionPolicy.getByID(addon.id)?.extension;
    return getExtensionInfoForAddon(ext, addon);
  }

  onEnabled(addon) {
    if (!checkAllowedAddon(addon)) {
      return;
    }
    this.emit("onEnabled", this.getExtensionInfo(addon));
  }

  onDisabled(addon) {
    if (!checkAllowedAddon(addon)) {
      return;
    }
    this.emit("onDisabled", this.getExtensionInfo(addon));
  }

  onInstalled(addon) {
    if (!checkAllowedAddon(addon)) {
      return;
    }
    this.emit("onInstalled", this.getExtensionInfo(addon));
  }

  onUninstalled(addon) {
    if (!checkAllowedAddon(addon)) {
      return;
    }
    this.emit("onUninstalled", this.getExtensionInfo(addon));
  }
}

let addonListener;

const getManagementListener = (extension, context) => {
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
};

this.management = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      management: {
        async get(id) {
          let addon = await AddonManager.getAddonByID(id);
          if (!addon) {
            throw new ExtensionError(`No such addon ${id}`);
          }
          if (!checkAllowedAddon(addon)) {
            throw new ExtensionError("get not allowed for this addon");
          }
          // If the extension is enabled get it and use it for more data.
          let ext = WebExtensionPolicy.getByID(addon.id)?.extension;
          return getExtensionInfoForAddon(ext, addon);
        },

        async getAll() {
          let addons = await AddonManager.getAddonsByTypes(allowedTypes);
          return addons.filter(checkAllowedAddon).map(addon => {
            // If the extension is enabled get it and use it for more data.
            let ext = WebExtensionPolicy.getByID(addon.id)?.extension;
            return getExtensionInfoForAddon(ext, addon);
          });
        },

        async install({ url, hash }) {
          let listener = {
            onDownloadEnded(install) {
              if (install.addon.appDisabled || install.addon.type !== "theme") {
                install.cancel();
                return false;
              }
            },
          };

          let telemetryInfo = {
            source: "extension",
            method: "management-webext-api",
          };
          let install = await AddonManager.getInstallForURL(url, {
            hash,
            telemetryInfo,
            triggeringPrincipal: extension.principal,
          });
          install.addListener(listener);
          try {
            await install.install();
          } catch (e) {
            Cu.reportError(e);
            throw new ExtensionError("Incompatible addon");
          }
          await install.addon.enable();
          return { id: install.addon.id };
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
            let buttonFlags =
              Ci.nsIPrompt.BUTTON_POS_0 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING +
              Ci.nsIPrompt.BUTTON_POS_1 * Ci.nsIPrompt.BUTTON_TITLE_IS_STRING;
            let button0Title = _("uninstall.confirmation.button-0.label");
            let button1Title = _("uninstall.confirmation.button-1.label");
            let response = promptService.confirmEx(
              null,
              title,
              message,
              buttonFlags,
              button0Title,
              button1Title,
              null,
              null,
              { value: 0 }
            );
            if (response == 1) {
              throw new ExtensionError("User cancelled uninstall of extension");
            }
          }
          let addon = await AddonManager.getAddonByID(extension.id);
          let canUninstall = Boolean(
            addon.permissions & AddonManager.PERM_CAN_UNINSTALL
          );
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
          if (addon.type !== "theme") {
            throw new ExtensionError("setEnabled applies only to theme addons");
          }
          if (addon.isSystem) {
            throw new ExtensionError(
              "setEnabled cannot be used with a system addon"
            );
          }
          if (enabled) {
            await addon.enable();
          } else {
            await addon.disable();
          }
        },

        onDisabled: new EventManager({
          context,
          name: "management.onDisabled",
          register: fire => {
            let listener = (event, data) => {
              fire.async(data);
            };

            getManagementListener(extension, context).on(
              "onDisabled",
              listener
            );
            return () => {
              getManagementListener(extension, context).off(
                "onDisabled",
                listener
              );
            };
          },
        }).api(),

        onEnabled: new EventManager({
          context,
          name: "management.onEnabled",
          register: fire => {
            let listener = (event, data) => {
              fire.async(data);
            };

            getManagementListener(extension, context).on("onEnabled", listener);
            return () => {
              getManagementListener(extension, context).off(
                "onEnabled",
                listener
              );
            };
          },
        }).api(),

        onInstalled: new EventManager({
          context,
          name: "management.onInstalled",
          register: fire => {
            let listener = (event, data) => {
              fire.async(data);
            };

            getManagementListener(extension, context).on(
              "onInstalled",
              listener
            );
            return () => {
              getManagementListener(extension, context).off(
                "onInstalled",
                listener
              );
            };
          },
        }).api(),

        onUninstalled: new EventManager({
          context,
          name: "management.onUninstalled",
          register: fire => {
            let listener = (event, data) => {
              fire.async(data);
            };

            getManagementListener(extension, context).on(
              "onUninstalled",
              listener
            );
            return () => {
              getManagementListener(extension, context).off(
                "onUninstalled",
                listener
              );
            };
          },
        }).api(),
      },
    };
  }
};

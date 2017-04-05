/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyGetter(this, "strBundle", function() {
  const stringSvc = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
  return stringSvc.createBundle("chrome://global/locale/extensions.properties");
});
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "promptService",
                                   "@mozilla.org/embedcomp/prompt-service;1",
                                   "nsIPromptService");

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

this.management = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      management: {
        getSelf: function() {
          return new Promise((resolve, reject) => AddonManager.getAddonByID(extension.id, addon => {
            try {
              let m = extension.manifest;
              let extInfo = {
                id: extension.id,
                name: addon.name,
                shortName: m.short_name || "",
                description: addon.description || "",
                version: addon.version,
                mayDisable: !!(addon.permissions & AddonManager.PERM_CAN_DISABLE),
                enabled: addon.isActive,
                optionsUrl: addon.optionsURL || "",
                permissions: Array.from(extension.permissions).filter(perm => {
                  return !extension.whiteListedHosts.pat.includes(perm);
                }),
                hostPermissions: extension.whiteListedHosts.pat,
                installType: installType(addon),
              };
              if (addon.homepageURL) {
                extInfo.homepageUrl = addon.homepageURL;
              }
              if (addon.updateURL) {
                extInfo.updateUrl = addon.updateURL;
              }
              if (m.icons) {
                extInfo.icons = Object.keys(m.icons).map(key => {
                  return {size: Number(key), url: m.icons[key]};
                });
              }

              resolve(extInfo);
            } catch (err) {
              reject(err);
            }
          }));
        },

        uninstallSelf: function(options) {
          return new Promise((resolve, reject) => {
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
                return reject({message: "User cancelled uninstall of extension"});
              }
            }
            AddonManager.getAddonByID(extension.id, addon => {
              let canUninstall = Boolean(addon.permissions & AddonManager.PERM_CAN_UNINSTALL);
              if (!canUninstall) {
                return reject({message: "The add-on cannot be uninstalled"});
              }
              try {
                addon.uninstall();
              } catch (err) {
                return reject(err);
              }
            });
          });
        },
      },
    };
  }
};

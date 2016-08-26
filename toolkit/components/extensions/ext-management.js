/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

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

extensions.registerSchemaAPI("management", "addon_parent", context => {
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
          } catch (e) {
            reject(e);
          }
        }));
      },
    },
  };
});

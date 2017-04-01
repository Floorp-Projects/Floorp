"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionPermissions",
                                  "resource://gre/modules/ExtensionPermissions.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

const {
  ExtensionError,
} = ExtensionUtils;

XPCOMUtils.defineLazyPreferenceGetter(this, "promptsEnabled",
                                      "extensions.webextOptionalPermissionPrompts");

extensions.registerSchemaAPI("permission", "addon_parent", context => {
  return {
    permissions: {
      async request_parent(perms) {
        let {permissions, origins} = perms;

        let manifestPermissions = context.extension.manifest.optional_permissions;
        for (let perm of permissions) {
          if (!manifestPermissions.includes(perm)) {
            throw new ExtensionError(`Cannot request permission ${perm} since it was not declared in optional_permissions`);
          }
        }

        let optionalOrigins = context.extension.optionalOrigins;
        for (let origin of origins) {
          if (!optionalOrigins.subsumes(origin)) {
            throw new ExtensionError(`Cannot request origin permission for ${origin} since it was not declared in optional_permissions`);
          }
        }

        if (promptsEnabled) {
          let allow = await new Promise(resolve => {
            let subject = {
              wrappedJSObject: {
                browser: context.xulBrowser,
                name: context.extension.name,
                icon: context.extension.iconURL,
                permissions: {permissions, origins},
                resolve,
              },
            };
            Services.obs.notifyObservers(subject, "webextension-optional-permission-prompt", null);
          });
          if (!allow) {
            return false;
          }
        }

        await ExtensionPermissions.add(context.extension, perms);
        return true;
      },

      async getAll() {
        let perms = context.extension.userPermissions;
        delete perms.apis;
        return perms;
      },

      async contains(permissions) {
        for (let perm of permissions.permissions) {
          if (!context.extension.hasPermission(perm)) {
            return false;
          }
        }

        for (let origin of permissions.origins) {
          if (!context.extension.whiteListedHosts.subsumes(origin)) {
            return false;
          }
        }

        return true;
      },

      async remove(permissions) {
        await ExtensionPermissions.remove(context.extension, permissions);
        return true;
      },
    },
  };
});

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstall", extension => {
  ExtensionPermissions.removeAll(extension);
});

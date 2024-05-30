/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "promptsEnabled",
  "extensions.webextOptionalPermissionPrompts"
);

function normalizePermissions(perms) {
  perms = { ...perms };
  perms.permissions = perms.permissions.filter(
    perm => !perm.startsWith("internal:") && perm !== "<all_urls>"
  );
  return perms;
}

this.permissions = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onAdded({ fire }) {
      let { extension } = this;
      let callback = (event, change) => {
        if (change.extensionId == extension.id && change.added) {
          let perms = normalizePermissions(change.added);
          if (perms.permissions.length || perms.origins.length) {
            fire.async(perms);
          }
        }
      };

      extensions.on("change-permissions", callback);
      return {
        unregister() {
          extensions.off("change-permissions", callback);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onRemoved({ fire }) {
      let { extension } = this;
      let callback = (event, change) => {
        if (change.extensionId == extension.id && change.removed) {
          let perms = normalizePermissions(change.removed);
          if (perms.permissions.length || perms.origins.length) {
            fire.async(perms);
          }
        }
      };

      extensions.on("change-permissions", callback);
      return {
        unregister() {
          extensions.off("change-permissions", callback);
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
      permissions: {
        async request(perms) {
          let { permissions, origins } = perms;

          let { optionalPermissions } = context.extension;
          for (let perm of permissions) {
            if (!optionalPermissions.includes(perm)) {
              throw new ExtensionError(
                `Cannot request permission ${perm} since it was not declared in optional_permissions`
              );
            }
          }

          let optionalOrigins = context.extension.optionalOrigins;
          for (let origin of origins) {
            if (!optionalOrigins.subsumes(new MatchPattern(origin))) {
              throw new ExtensionError(
                `Cannot request origin permission for ${origin} since it was not declared in the manifest`
              );
            }
          }

          if (promptsEnabled) {
            permissions = permissions.filter(
              perm => !context.extension.hasPermission(perm)
            );
            origins = origins.filter(
              origin =>
                !context.extension.allowedOrigins.subsumes(
                  new MatchPattern(origin)
                )
            );

            if (!permissions.length && !origins.length) {
              return true;
            }

            let browser = context.pendingEventBrowser || context.xulBrowser;
            let allowPromise = new Promise(resolve => {
              let subject = {
                wrappedJSObject: {
                  browser,
                  name: context.extension.name,
                  id: context.extension.id,
                  icon: context.extension.getPreferredIcon(32),
                  permissions: { permissions, origins },
                  resolve,
                },
              };
              Services.obs.notifyObservers(
                subject,
                "webextension-optional-permission-prompt"
              );
            });
            if (context.isBackgroundContext) {
              extension.emit("background-script-idle-waituntil", {
                promise: allowPromise,
                reason: "permissions_request",
              });
            }
            if (!(await allowPromise)) {
              return false;
            }
          }

          await ExtensionPermissions.add(extension.id, perms, extension);
          return true;
        },

        async getAll() {
          let perms = normalizePermissions(context.extension.activePermissions);
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
            if (
              !context.extension.allowedOrigins.subsumes(
                new MatchPattern(origin)
              )
            ) {
              return false;
            }
          }

          return true;
        },

        async remove(permissions) {
          await ExtensionPermissions.remove(
            extension.id,
            permissions,
            extension
          );
          return true;
        },

        onAdded: new EventManager({
          context,
          module: "permissions",
          event: "onAdded",
          extensionApi: this,
        }).api(),

        onRemoved: new EventManager({
          context,
          module: "permissions",
          event: "onRemoved",
          extensionApi: this,
        }).api(),
      },
    };
  }
};

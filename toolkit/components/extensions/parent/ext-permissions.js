/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionPermissions",
  "resource://gre/modules/ExtensionPermissions.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

var { ExtensionError } = ExtensionUtils;

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "promptsEnabled",
  "extensions.webextOptionalPermissionPrompts"
);

this.permissions = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      permissions: {
        async request(perms) {
          let { permissions, origins } = perms;

          let manifestPermissions =
            context.extension.manifest.optional_permissions;
          for (let perm of permissions) {
            if (!manifestPermissions.includes(perm)) {
              throw new ExtensionError(
                `Cannot request permission ${perm} since it was not declared in optional_permissions`
              );
            }
          }

          let optionalOrigins = context.extension.optionalOrigins;
          for (let origin of origins) {
            if (!optionalOrigins.subsumes(new MatchPattern(origin))) {
              throw new ExtensionError(
                `Cannot request origin permission for ${origin} since it was not declared in optional_permissions`
              );
            }
          }

          if (promptsEnabled) {
            permissions = permissions.filter(
              perm => !context.extension.hasPermission(perm)
            );
            origins = origins.filter(
              origin =>
                !context.extension.whiteListedHosts.subsumes(
                  new MatchPattern(origin)
                )
            );

            if (permissions.length == 0 && origins.length == 0) {
              return true;
            }

            let browser = context.pendingEventBrowser || context.xulBrowser;
            let allow = await new Promise(resolve => {
              let subject = {
                wrappedJSObject: {
                  browser,
                  name: context.extension.name,
                  icon: context.extension.iconURL,
                  permissions: { permissions, origins },
                  resolve,
                },
              };
              Services.obs.notifyObservers(
                subject,
                "webextension-optional-permission-prompt"
              );
            });
            if (!allow) {
              return false;
            }
          }

          // Unfortunately, we treat <all_urls> as an API permission as well.
          if (origins.includes("<all_urls>")) {
            perms.permissions.push("<all_urls>");
          }

          await ExtensionPermissions.add(extension.id, perms, extension);
          return true;
        },

        async getAll() {
          let perms = { ...context.extension.activePermissions };
          delete perms.apis;
          perms.permissions = perms.permissions.filter(
            perm => !perm.startsWith("internal:")
          );
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
              !context.extension.whiteListedHosts.subsumes(
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
      },
    };
  }
};

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var PermissionsHelper = {
  _permissonTypes: ["password", "geolocation", "popup", "indexedDB",
                    "offline-app", "desktop-notification", "plugins", "native-intent",
                    "flyweb-publish-server"],
  _permissionStrings: {
    "password": {
      label: "password.logins",
      allowed: "password.save",
      denied: "password.dontSave"
    },
    "geolocation": {
      label: "geolocation.location",
      allowed: "geolocation.allow",
      denied: "geolocation.dontAllow"
    },
    "flyweb-publish-server": {
      label: "flyWebPublishServer.publishServer",
      allowed: "flyWebPublishServer.allow",
      denied: "flyWebPublishServer.dontAllow"
    },
    "popup": {
      label: "blockPopups.label2",
      allowed: "popup.show",
      denied: "popup.dontShow"
    },
    "indexedDB": {
      label: "offlineApps.offlineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.dontAllow2"
    },
    "offline-app": {
      label: "offlineApps.offlineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.dontAllow2"
    },
    "desktop-notification": {
      label: "desktopNotification.notifications",
      allowed: "desktopNotification2.allow",
      denied: "desktopNotification2.dontAllow"
    },
    "plugins": {
      label: "clickToPlayPlugins.plugins",
      allowed: "clickToPlayPlugins.activate",
      denied: "clickToPlayPlugins.dontActivate"
    },
    "native-intent": {
      label: "helperapps.openWithList2",
      allowed: "helperapps.always",
      denied: "helperapps.never"
    }
  },

  onEvent: function onEvent(event, data, callback) {
    let uri = BrowserApp.selectedBrowser.currentURI;
    let check = false;

    switch (event) {
      case "Permissions:Check":
        check = true;
        // fall-through

      case "Permissions:Get":
        let permissions = [];
        for (let i = 0; i < this._permissonTypes.length; i++) {
          let type = this._permissonTypes[i];
          let value = this.getPermission(uri, type);

          // Only add the permission if it was set by the user
          if (value == Services.perms.UNKNOWN_ACTION)
            continue;

          if (check) {
            GlobalEventDispatcher.sendRequest({
              type: "Permissions:CheckResult",
              hasPermissions: true
            });
            return;
          }
          // Get the strings that correspond to the permission type
          let typeStrings = this._permissionStrings[type];
          let label = Strings.browser.GetStringFromName(typeStrings["label"]);

          // Get the key to look up the appropriate string entity
          let valueKey = value == Services.perms.ALLOW_ACTION ?
                         "allowed" : "denied";
          let valueString = Strings.browser.GetStringFromName(typeStrings[valueKey]);

          permissions.push({
            type: type,
            setting: label,
            value: valueString
          });
        }

        if (check) {
          GlobalEventDispatcher.sendRequest({
            type: "Permissions:CheckResult",
            hasPermissions: false
          });
          return;
        }

        // Keep track of permissions, so we know which ones to clear
        this._currentPermissions = permissions;

        WindowEventDispatcher.sendRequest({
          type: "Permissions:Data",
          permissions: permissions
        });
        break;
 
      case "Permissions:Clear":
        // An array of the indices of the permissions we want to clear
        let permissionsToClear = data.permissions;
        let privacyContext = BrowserApp.selectedBrowser.docShell
                               .QueryInterface(Ci.nsILoadContext);

        for (let i = 0; i < permissionsToClear.length; i++) {
          let indexToClear = permissionsToClear[i];
          let permissionType = this._currentPermissions[indexToClear]["type"];
          this.clearPermission(uri, permissionType, privacyContext);
        }
        break;
    }
  },

  /**
   * Gets the permission value stored for a specified permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "geolocation", "indexedDB", "popup"
   *
   * @return A permission value defined in nsIPermissionManager.
   */
  getPermission: function getPermission(aURI, aType) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      // By default, login saving is enabled, so if it is disabled, the
      // user selected the never remember option
      if (!Services.logins.getLoginSavingEnabled(aURI.prePath))
        return Services.perms.DENY_ACTION;

      // Check to see if the user ever actually saved a login
      if (Services.logins.countLogins(aURI.prePath, "", ""))
        return Services.perms.ALLOW_ACTION;

      return Services.perms.UNKNOWN_ACTION;
    }

    // Geolocation consumers use testExactPermission
    if (aType == "geolocation")
      return Services.perms.testExactPermission(aURI, aType);

    return Services.perms.testPermission(aURI, aType);
  },

  /**
   * Clears a user-set permission value for the site given a permission type.
   *
   * @param aType
   *        The permission type string stored in permission manager.
   *        e.g. "geolocation", "indexedDB", "popup"
   */
  clearPermission: function clearPermission(aURI, aType, aContext) {
    // Password saving isn't a nsIPermissionManager permission type, so handle
    // it seperately.
    if (aType == "password") {
      // Get rid of exisiting stored logings
      let logins = Services.logins.findLogins({}, aURI.prePath, "", "");
      for (let i = 0; i < logins.length; i++) {
        Services.logins.removeLogin(logins[i]);
      }
      // Re-set login saving to enabled
      Services.logins.setLoginSavingEnabled(aURI.prePath, true);
    } else {
      Services.perms.remove(aURI, aType);
      // Clear content prefs set in ContentPermissionPrompt.js
      Cc["@mozilla.org/content-pref/service;1"]
        .getService(Ci.nsIContentPrefService2)
        .removeByDomainAndName(aURI.spec, aType + ".request.remember", aContext);
    }
  }
};

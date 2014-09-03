/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var PermissionsHelper = {
  _permissonTypes: ["password", "geolocation", "popup", "indexedDB",
                    "offline-app", "desktop-notification", "plugins", "native-intent"],
  _permissionStrings: {
    "password": {
      label: "password.savePassword",
      allowed: "password.save",
      denied: "password.dontSave"
    },
    "geolocation": {
      label: "geolocation.shareLocation",
      allowed: "geolocation.allow",
      denied: "geolocation.dontAllow"
    },
    "popup": {
      label: "blockPopups.label",
      allowed: "popup.show",
      denied: "popup.dontShow"
    },
    "indexedDB": {
      label: "offlineApps.storeOfflineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.dontAllow2"
    },
    "offline-app": {
      label: "offlineApps.storeOfflineData",
      allowed: "offlineApps.allow",
      denied: "offlineApps.dontAllow2"
    },
    "desktop-notification": {
      label: "desktopNotification.useNotifications",
      allowed: "desktopNotification.allow",
      denied: "desktopNotification.dontAllow"
    },
    "plugins": {
      label: "clickToPlayPlugins.activatePlugins",
      allowed: "clickToPlayPlugins.activate",
      denied: "clickToPlayPlugins.dontActivate"
    },
    "native-intent": {
      label: "helperapps.openWithList2",
      allowed: "helperapps.always",
      denied: "helperapps.never"
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    let uri = BrowserApp.selectedBrowser.currentURI;

    switch (aTopic) {
      case "Permissions:Get":
        let permissions = [];
        for (let i = 0; i < this._permissonTypes.length; i++) {
          let type = this._permissonTypes[i];
          let value = this.getPermission(uri, type);

          // Only add the permission if it was set by the user
          if (value == Services.perms.UNKNOWN_ACTION)
            continue;

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

        // Keep track of permissions, so we know which ones to clear
        this._currentPermissions = permissions;

        let host;
        try {
          host = uri.host;
        } catch(e) {
          host = uri.spec;
        }
        Messaging.sendRequest({
          type: "Permissions:Data",
          host: host,
          permissions: permissions
        });
        break;
 
      case "Permissions:Clear":
        // An array of the indices of the permissions we want to clear
        let permissionsToClear = JSON.parse(aData);
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
      Services.perms.remove(aURI.host, aType);
      // Clear content prefs set in ContentPermissionPrompt.js
      Cc["@mozilla.org/content-pref/service;1"]
        .getService(Ci.nsIContentPrefService2)
        .removeByDomainAndName(aURI.spec, aType + ".request.remember", aContext);
    }
  }
};

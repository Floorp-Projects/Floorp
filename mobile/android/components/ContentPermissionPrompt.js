/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "RuntimePermissions",
                               "resource://gre/modules/RuntimePermissions.jsm");

ChromeUtils.defineModuleGetter(this, "DoorHanger",
                               "resource://gre/modules/Prompt.jsm");

ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");

const kEntities = {
  "contacts": "contacts",
  "desktop-notification": "desktopNotification2",
  "geolocation": "geolocation",
};

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  handleExistingPermission: function handleExistingPermission(request, type, callback) {
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      callback(/* allow */ true);
      return true;
    }

    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      callback(/* allow */ false);
      return true;
    }

    return false;
  },

  getChromeWindow: function getChromeWindow(aWindow) {
     let chromeWin = aWindow.docShell.rootTreeItem.domWindow
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  },

  getChromeForRequest: function getChromeForRequest(request) {
    if (request.window) {
      let requestingWindow = request.window.top;
      return this.getChromeWindow(requestingWindow).wrappedJSObject;
    }
    return request.element.ownerGlobal;
  },

  prompt: function(request) {
    // Only allow exactly one permission rquest here.
    let types = request.types.QueryInterface(Ci.nsIArray);
    if (types.length != 1) {
      request.cancel();
      return;
    }

    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);

    let callback = (allow) => {
      if (!allow) {
        request.cancel();
        return;
      }
      if (perm.type === "geolocation") {
        RuntimePermissions.waitForPermissions(
          RuntimePermissions.ACCESS_FINE_LOCATION).then((granted) => {
            (granted ? request.allow : request.cancel)();
          });
        return;
      }
      request.allow();
    };

    // We don't want to remember permissions in private mode
    let isPrivate = PrivateBrowsingUtils.isWindowPrivate(request.window.ownerGlobal);

    // Returns true if the request was handled
    if (this.handleExistingPermission(request, perm.type, callback)) {
       return;
    }

    if (perm.type === "desktop-notification" &&
        Services.prefs.getBoolPref("dom.webnotifications.requireuserinteraction", false) &&
        !request.isHandlingUserInput) {
      request.cancel();
      return;
    }

    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let entityName = kEntities[perm.type];

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".dontAllow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again" or this is a desktopNotification, make a permanent exception
        if (aChecked || entityName == "desktopNotification2")
          Services.perms.addFromPrincipal(request.principal, perm.type, Ci.nsIPermissionManager.DENY_ACTION);

        callback(/* allow */ false);
      },
    },
    {
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      callback: function(aChecked) {
        let isPermanent = (aChecked || entityName == "desktopNotification2");
        // If the user checked "Don't ask again" or this is a desktopNotification, make a permanent exception
        // Also, we don't want to permanently store this exception if the user is in private mode
        if (!isPrivate && isPermanent) {
          Services.perms.addFromPrincipal(request.principal, perm.type, Ci.nsIPermissionManager.ALLOW_ACTION);
        // If we are in private mode, then it doesn't matter if the notification is desktop and also
        // it shouldn't matter if the Don't show checkbox was checked because it shouldn't be show in the first place
        } else if (isPrivate && isPermanent) {
          // Otherwise allow the permission for the current session if the request comes from an app
          // or if the request was made in private mode
          Services.perms.addFromPrincipal(request.principal, perm.type, Ci.nsIPermissionManager.ALLOW_ACTION, Ci.nsIPermissionManager.EXPIRE_SESSION);
        }

        callback(/* allow */ true);
      },
      positive: true,
    }];

    let chromeWin = this.getChromeForRequest(request);
    let requestor = (chromeWin.BrowserApp && chromeWin.BrowserApp.manifest) ?
        "'" + chromeWin.BrowserApp.manifest.name + "'" : request.principal.URI.host;
    let message = browserBundle.formatStringFromName(entityName + ".ask", [requestor], 1);
    // desktopNotification doesn't have a checkbox
    let options;
    if (entityName == "desktopNotification2") {
      options = {
        link: {
          label: browserBundle.GetStringFromName("doorhanger.learnMore"),
          url: "https://www.mozilla.org/firefox/push/",
        },
      };
    // it doesn't make sense to display the checkbox since we won't be remembering
    // this specific permission if the user is in Private mode
    } else if (!isPrivate) {
      options = { checkbox: browserBundle.GetStringFromName(entityName + ".dontAskAgain") };
    } else {
      options = { };
    }

    options.defaultCallback = () => {
      callback(/* allow */ false);
    };

    DoorHanger.show(request.window || request.element.ownerGlobal,
                    message, entityName + request.principal.URI.host,
                    buttons, options, entityName.toUpperCase());
  },
};


// module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);

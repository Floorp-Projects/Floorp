/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/RuntimePermissions.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const kEntities = {
  "contacts": "contacts",
  "desktop-notification": "desktopNotification2",
  "geolocation": "geolocation",
  "flyweb-publish-server": "flyWebPublishServer",
};

// For these types, prompt for permission if action is unknown.
const PROMPT_FOR_UNKNOWN = [
  "desktop-notification",
  "geolocation",
  "flyweb-publish-server",
];

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  handleExistingPermission: function handleExistingPermission(request, type, denyUnknown, callback) {
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      callback(/* allow */ true);
      return true;
    }

    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      callback(/* allow */ false);
      return true;
    }

    if (denyUnknown && result == Ci.nsIPermissionManager.UNKNOWN_ACTION) {
      callback(/* allow */ false);
      return true;
    }

    return false;
  },

  getChromeWindow: function getChromeWindow(aWindow) {
     let chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShellTreeItem)
                            .rootTreeItem
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow)
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
    let isApp = request.principal.appId !== Ci.nsIScriptSecurityManager.NO_APP_ID && request.principal.appId !== Ci.nsIScriptSecurityManager.UNKNOWN_APP_ID;

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

    // Returns true if the request was handled
    let access = (perm.access && perm.access !== "unused") ?
                 (perm.type + "-" + perm.access) : perm.type;
    if (this.handleExistingPermission(request, access,
          /* denyUnknown */ isApp || PROMPT_FOR_UNKNOWN.indexOf(perm.type) < 0, callback)) {
       return;
    }

    let chromeWin = this.getChromeForRequest(request);
    let tab = chromeWin.BrowserApp.getTabForWindow(request.window.top);
    if (!tab) {
      return;
    }

    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let entityName = kEntities[perm.type];

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".dontAllow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again" or this is a desktopNotification, make a permanent exception
        if (aChecked || entityName == "desktopNotification2")
          Services.perms.addFromPrincipal(request.principal, access, Ci.nsIPermissionManager.DENY_ACTION);

        callback(/* allow */ false);
      }
    },
    {
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again" or this is a desktopNotification, make a permanent exception
        if (aChecked || entityName == "desktopNotification2") {
          Services.perms.addFromPrincipal(request.principal, access, Ci.nsIPermissionManager.ALLOW_ACTION);
        } else if (isApp) {
          // Otherwise allow the permission for the current session if the request comes from an app
          Services.perms.addFromPrincipal(request.principal, access, Ci.nsIPermissionManager.ALLOW_ACTION, Ci.nsIPermissionManager.EXPIRE_SESSION);
        }

        callback(/* allow */ true);
      },
      positive: true
    }];

    let requestor = chromeWin.BrowserApp.manifest ? "'" + chromeWin.BrowserApp.manifest.name + "'" : request.principal.URI.host;
    let message = browserBundle.formatStringFromName(entityName + ".ask", [requestor], 1);
    // desktopNotification doesn't have a checkbox
    let options;
    if (entityName == "desktopNotification2") {
      options = {
        link: {
          label: browserBundle.GetStringFromName("doorhanger.learnMore"),
          url: "https://www.mozilla.org/firefox/push/"
        }
      };
    } else {
      options = { checkbox: browserBundle.GetStringFromName(entityName + ".dontAskAgain") };
    }

    chromeWin.NativeWindow.doorhanger.show(message, entityName + request.principal.URI.host, buttons, tab.id, options, entityName.toUpperCase());
  }
};


//module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);

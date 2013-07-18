/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kEntities = { "geolocation": "geolocation",
                    "desktop-notification": "desktopNotification" };

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  handleExistingPermission: function handleExistingPermission(request) {
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, request.type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return true;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
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
    return request.element.ownerDocument.defaultView;
  },

  prompt: function(request) {
    // Returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let chromeWin = this.getChromeForRequest(request);
    let tab = chromeWin.BrowserApp.getTabForWindow(request.window.top);
    if (!tab)
      return;

    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let entityName = kEntities[request.type];

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again", make a permanent exception
        if (aChecked) {
          Services.perms.addFromPrincipal(request.principal, request.type, Ci.nsIPermissionManager.ALLOW_ACTION);
        } else if (entityName == "desktopNotification") {
          // For notifications, it doesn't make sense to grant permission once. So when the user clicks allow,
          // we let the requestor create notifications for the session.
          Services.perms.addFromPrincipal(request.principal, request.type, Ci.nsIPermissionManager.ALLOW_ACTION, Ci.nsIPermissionManager.EXPIRE_SESSION);
        }

        request.allow();
      }
    },
    {
      label: browserBundle.GetStringFromName(entityName + ".dontAllow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again", make a permanent exception
        if (aChecked)
          Services.perms.addFromPrincipal(request.principal, request.type, Ci.nsIPermissionManager.DENY_ACTION);

        request.cancel();
      }
    }];

    let requestor = chromeWin.BrowserApp.manifest ? "'" + chromeWin.BrowserApp.manifest.name + "'" : request.principal.URI.host;
    let message = browserBundle.formatStringFromName(entityName + ".ask", [requestor], 1);
    let options = { checkbox: browserBundle.GetStringFromName(entityName + ".dontAskAgain") };

    chromeWin.NativeWindow.doorhanger.show(message, entityName + request.principal.URI.host, buttons, tab.id, options);
  }
};


//module initialization
this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);

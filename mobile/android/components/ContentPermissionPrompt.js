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
    let result = Services.perms.testExactPermission(request.uri, request.type);
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
    let tab = chromeWin.BrowserApp.getTabForWindow(request.window);
    if (!tab)
      return;

    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
    let entityName = kEntities[request.type];

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again", make a permanent exception
        if (aChecked)
          Services.perms.add(request.uri, request.type, Ci.nsIPermissionManager.ALLOW_ACTION);

        request.allow();
      }
    },
    {
      label: browserBundle.GetStringFromName(entityName + ".dontAllow"),
      callback: function(aChecked) {
        // If the user checked "Don't ask again", make a permanent exception
        if (aChecked)
          Services.perms.add(request.uri, request.type, Ci.nsIPermissionManager.DENY_ACTION);

        request.cancel();
      }
    }];

    let message = browserBundle.formatStringFromName(entityName + ".wantsTo",
                                                     [request.uri.host], 1);
    let options = { checkbox: browserBundle.GetStringFromName(entityName + ".dontAskAgain") };

    chromeWin.NativeWindow.doorhanger.show(message,
                                           entityName + request.uri.host,
                                           buttons, tab.id, options);
  }
};


//module initialization
const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);

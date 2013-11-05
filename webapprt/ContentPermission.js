/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const UNKNOWN_FAIL = ["geolocation", "desktop-notification"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://webapprt/modules/WebappRT.jsm");

function ContentPermission() {}

ContentPermission.prototype = {
  classID: Components.ID("{07ef5b2e-88fb-47bd-8cec-d3b0bef11ac4}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  _getChromeWindow: function(aWindow) { 
    return aWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIDOMChromeWindow);
  },

  prompt: function(request) {
    // Reuse any remembered permission preferences
    let result =
      Services.perms.testExactPermissionFromPrincipal(request.principal,
                                                      request.type);

    // We used to use the name "geo" for the geolocation permission, now we're
    // using "geolocation".  We need to check both to support existing
    // installations.
    if ((result == Ci.nsIPermissionManager.UNKNOWN_ACTION ||
         result == Ci.nsIPermissionManager.PROMPT_ACTION) &&
        request.type == "geolocation") {
      let geoResult = Services.perms.testExactPermission(request.principal.URI,
                                                         "geo");
      // We override the result only if the "geo" permission was allowed or
      // denied.
      if (geoResult == Ci.nsIPermissionManager.ALLOW_ACTION ||
          geoResult == Ci.nsIPermissionManager.DENY_ACTION) {
        result = geoResult;
      }
    }

    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return;
    } else if (result == Ci.nsIPermissionManager.DENY_ACTION ||
               (result == Ci.nsIPermissionManager.UNKNOWN_ACTION &&
                UNKNOWN_FAIL.indexOf(request.type) >= 0)) {
      request.cancel();
      return;
    }

    // Display a prompt at the top level
    let {name} = WebappRT.localeManifest;
    let requestingWindow = request.window.top;
    let chromeWin = this._getChromeWindow(requestingWindow);
    let bundle = Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");

    // Construct a prompt with share/don't and remember checkbox
    let remember = {value: false};
    let choice = Services.prompt.confirmEx(
      chromeWin,
      bundle.formatStringFromName(request.type + ".title", [name], 1),
      bundle.GetStringFromName(request.type + ".description"),
      // Set both buttons to strings with the cancel button being default
      Ci.nsIPromptService.BUTTON_POS_1_DEFAULT |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0 |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_1,
      bundle.GetStringFromName(request.type + ".allow"),
      bundle.GetStringFromName(request.type + ".deny"),
      null,
      bundle.GetStringFromName(request.type + ".remember"),
      remember);

    let action = Ci.nsIPermissionManager.ALLOW_ACTION;
    if (choice != 0) {
      action = Ci.nsIPermissionManager.DENY_ACTION;
    }

    if (remember.value) {
      // Persist the choice if the user wants to remember
      Services.perms.addFromPrincipal(request.principal, request.type, action);
    } else {
      // Otherwise allow the permission for the current session
      Services.perms.addFromPrincipal(request.principal, request.type, action,
                                      Ci.nsIPermissionManager.EXPIRE_SESSION);
    }

    // Trigger the selected choice
    if (choice == 0) {
      request.allow();
    }
    else {
      request.cancel();
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermission]);

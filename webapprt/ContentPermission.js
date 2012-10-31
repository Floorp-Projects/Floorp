/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://webapprt/modules/WebappRT.jsm");

function ContentPermission() {}

ContentPermission.prototype = {
  classID: Components.ID("{07ef5b2e-88fb-47bd-8cec-d3b0bef11ac4}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  prompt: function(request) {
    // Only handle geolocation requests for now
    if (request.type != "geolocation") {
      return;
    }

    // Reuse any remembered permission preferences
    let result = Services.perms.testExactPermissionFromPrincipal(request.principal, "geo");
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return;
    }
    else if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return;
    }

    function getChromeWindow(aWindow) {
      var chromeWin = aWindow
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebNavigation)
        .QueryInterface(Ci.nsIDocShellTreeItem)
        .rootTreeItem
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindow)
        .QueryInterface(Ci.nsIDOMChromeWindow);
      return chromeWin;
    }

    // Display a prompt at the top level
    let {name} = WebappRT.config.app.manifest;
    let requestingWindow = request.window.top;
    let chromeWin = getChromeWindow(requestingWindow);
    let bundle = Services.strings.createBundle("chrome://webapprt/locale/webapp.properties");

    // Construct a prompt with share/don't and remember checkbox
    let remember = {value: false};
    let choice = Services.prompt.confirmEx(
      chromeWin,
      bundle.formatStringFromName("geolocation.title", [name], 1),
      bundle.GetStringFromName("geolocation.description"),
      // Set both buttons to strings with the cancel button being default
      Ci.nsIPromptService.BUTTON_POS_1_DEFAULT |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_0 |
        Ci.nsIPromptService.BUTTON_TITLE_IS_STRING * Ci.nsIPromptService.BUTTON_POS_1,
      bundle.GetStringFromName("geolocation.sharelocation"),
      bundle.GetStringFromName("geolocation.dontshare"),
      null,
      bundle.GetStringFromName("geolocation.remember"),
      remember);

    // Persist the choice if the user wants to remember
    if (remember.value) {
      let action = Ci.nsIPermissionManager.ALLOW_ACTION;
      if (choice != 0) {
        action = Ci.nsIPermissionManager.DENY_ACTION;
      }
      Services.perms.addFromPrincipal(request.principal, "geo", action);
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

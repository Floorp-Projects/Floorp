/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Extension Manager UI.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AddonManager.jsm");

var gAddon = null;

// If the user enables the add-on through some other UI close this window
var EnableListener = {
  onEnabling: function(aAddon) {
    if (aAddon.id == gAddon.id)
      window.close();
  }
}
AddonManager.addAddonListener(EnableListener);

function initialize() {
  // About URIs don't implement nsIURL so we have to find the query string
  // manually
  let spec = document.documentURIObject.spec;
  let pos = spec.indexOf("?");
  let query = "";
  if (pos >= 0)
    query = spec.substring(pos + 1);

  let bundle = Services.strings.createBundle("chrome://mozapps/locale/extensions/newaddon.properties");

  // Just assume the query is "id=<id>"
  let id = query.substring(3);
  if (!id) {
    window.close();
    return;
  }

  AddonManager.getAddonByID(id, function(aAddon) {
    // If the add-on doesn't exist or it is already enabled or it cannot be
    // enabled then this UI is useless, just close it. This shouldn't normally
    // happen unless session restore restores the tab
    if (!aAddon || !aAddon.userDisabled ||
        !(aAddon.permissions & AddonManager.PERM_CAN_ENABLE)) {
      window.close();
      return;
    }

    gAddon = aAddon;

    document.getElementById("addon-info").setAttribute("type", aAddon.type);
    
    let icon = document.getElementById("icon");
    if (aAddon.icon64URL)
      icon.src = aAddon.icon64URL;
    else if (aAddon.iconURL)
      icon.src = aAddon.iconURL;

    let name = bundle.formatStringFromName("name", [aAddon.name, aAddon.version],
                                           2);
    document.getElementById("name").value = name

    if (aAddon.creator) {
      let creator = bundle.formatStringFromName("author", [aAddon.creator], 1);
      document.getElementById("author").value = creator;
    } else {
      document.getElementById("author").hidden = true;
    }

    let uri = "getResourceURI" in aAddon ? aAddon.getResourceURI() : null;
    let locationLabel = document.getElementById("location");
    if (uri instanceof Ci.nsIFileURL) {
      let location = bundle.formatStringFromName("location", [uri.file.path], 1);
      locationLabel.value = location;
      locationLabel.setAttribute("tooltiptext", location);
    } else {
      document.getElementById("location").hidden = true;
    }

    var event = document.createEvent("Events");
    event.initEvent("AddonDisplayed", true, true);
    document.dispatchEvent(event);
  });
}

function unload() {
  AddonManager.removeAddonListener(EnableListener);
}

function continueClicked() {
  AddonManager.removeAddonListener(EnableListener);

  if (document.getElementById("allow").checked) {
    gAddon.userDisabled = false;

    if (gAddon.pendingOperations & AddonManager.PENDING_ENABLE) {
      document.getElementById("allow").disabled = true;
      document.getElementById("buttonDeck").selectedPanel = document.getElementById("restartPanel");
      return;
    }
  }

  window.close();
}

function restartClicked() {
  let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                   createInstance(Ci.nsISupportsPRBool);
  Services.obs.notifyObservers(cancelQuit, "quit-application-requested",
                               "restart");
  if (cancelQuit.data)
    return; // somebody canceled our quit request

  window.close();

  let appStartup = Components.classes["@mozilla.org/toolkit/app-startup;1"].
                   getService(Components.interfaces.nsIAppStartup);
  appStartup.quit(Ci.nsIAppStartup.eAttemptQuit |  Ci.nsIAppStartup.eRestart);
}

function cancelClicked() {
  gAddon.userDisabled = true;
  AddonManager.addAddonListener(EnableListener);

  document.getElementById("allow").disabled = false;
  document.getElementById("buttonDeck").selectedPanel = document.getElementById("continuePanel");
}

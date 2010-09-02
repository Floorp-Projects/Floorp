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
 * The Original Code is Alerts Service.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wes Johnston <wjohnston@mozilla.com>
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

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// BlocklistPrompt Service
// -----------------------------------------------------------------------


function BlocklistPrompt() { }

BlocklistPrompt.prototype = {
  prompt: function(aAddons, aCount) {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (win.ExtensionsView.visible) {
      win.ExtensionsView.showRestart("blocked");
    } else {
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
      let notifyBox = win.getNotificationBox();
      let restartCallback = function(aNotification, aDescription) {
        // Notify all windows that an application quit has been requested
        var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
  
        // If nothing aborted, quit the app
        if (cancelQuit.data == false) {
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
          appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
        }
      };
      
      let buttons = [{accessKey: null,
                      label: bundle.GetStringFromName("notificationRestart.button"),
                      callback: restartCallback}];
      notifyBox.appendNotification(bundle.GetStringFromName("notificationRestart.blocked"),
                                   "blocked-add-on",
                                   "",
                                   "PRIORITY_CRITICAL_HIGH",
                                   buttons);
    }
    // Disable softblocked items automatically
    for (let i = 0; i < aAddons.length; i++) {
      if (aAddons[i].item instanceof Ci.nsIPluginTag)
        addonList[i].item.disabled = true;
      else
        aAddons[i].item.userDisabled = true;
    }
  },
  classID: Components.ID("{4e6ea350-b09a-11df-94e2-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBlocklistPrompt])
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([BlocklistPrompt]);


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
 * The Original Code is Feedback.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mark.finkle@gmail.com>
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

var Feedback = {
  _prefs: [],
  _device: "",
  _manufacturer: "",

  init: function(aEvent) {
    // Delay the widget initialization during startup.
    let panel = document.getElementById("feedback-container");
    panel.addEventListener("ToolPanelShown", function delayedInit(aEvent) {
      panel.removeEventListener("ToolPanelShown", delayedInit, false);

      // A simple frame script to fill in the referrer page and device info
      messageManager.loadFrameScript("chrome://feedback/content/content.js", true);

      let setting = document.getElementById("feedback-checkCompatibility");
      setting.setAttribute("pref", Feedback.compatibilityPref);
      setting.preferenceChanged();

      document.getElementById("feedback-container").hidden = false;

      let feedbackPrefs = document.getElementById("feedback-tools").childNodes;
      for (let i = 0; i < feedbackPrefs.length; i++) {
        let pref = feedbackPrefs[i].getAttribute("pref");
        if (!pref)
          continue;
  
        let value = Services.prefs.getPrefType(pref) == Ci.nsIPrefBranch.PREF_INVALID ? false : Services.prefs.getBoolPref(pref);
        Feedback._prefs.push({ "name": pref, "value": value });
      }

      let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
      Feedback._device = sysInfo.get("device");
      Feedback._manufacturer = sysInfo.get("manufacturer");
    }, false);
  },

  get compatibilityPref() {
    let result = "extensions.checkCompatibility.";
    let channel = Services.prefs.getCharPref("app.update.channel");
    if (channel == "nightly") {
      result += "nightly";
    } else {
      // Copied from toolkit/mozapps/extensions/XPIProvider.jsm
      const BRANCH_REGEXP = /^([^\.]+\.[0-9]+[a-z]*).*/gi;
      result += Services.appinfo.version.replace(BRANCH_REGEXP, "$1");
    }
    delete this.compatibilityPref;
    return this.compatibilityPref = result;
  },

  openFeedback: function(aName) {
    let pref = "extensions.feedback.url." + aName;
    let url = Services.prefs.getPrefType(pref) == Ci.nsIPrefBranch.PREF_INVALID ? "" : Services.prefs.getCharPref(pref);
    if (!url)
      return;

    let currentURL = Browser.selectedBrowser.currentURI.spec;
    let newTab = BrowserUI.newTab(url, Browser.selectedTab);

    // Tell the feedback page to fill in the referrer URL
    newTab.browser.messageManager.addMessageListener("DOMContentLoaded", function() {
      newTab.browser.messageManager.removeMessageListener("DOMContentLoaded", arguments.callee, true);
      newTab.browser.messageManager.sendAsyncMessage("Feedback:InitPage", { referrer: currentURL, device: Feedback._device, manufacturer: Feedback._manufacturer });
    });
  },

  updateRestart: function updateRestart() {
    let msg = document.getElementById("feedback-messages");
    if (msg) {
      let value = "restart-app";
      let notification = msg.getNotificationWithValue(value);
      if (notification) {
        // Check if the prefs are back to the initial state dismiss the restart
        // notification because if does not make sense anymore
        for each (let pref in this._prefs) {
          let value = Services.prefs.getPrefType(pref.name) == Ci.nsIPrefBranch.PREF_INVALID ? false : Services.prefs.getBoolPref(pref.name);
          if (value != pref.value)
            return;
        }

        notification.close();
        return;
      }
  
      let restartCallback = function(aNotification, aDescription) {
        // Notify all windows that an application quit has been requested
        let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
        Services.obs.notifyObservers(cancelQuit, "quit-application-requested", "restart");
  
        // If nothing aborted, quit the app
        if (cancelQuit.data == false) {
          let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
          appStartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
        }
      };

      let strings = Strings.browser;

      let buttons = [ {
        label: strings.GetStringFromName("notificationRestart.button"),
        accessKey: "",
        callback: restartCallback
      } ];
  
      let message = strings.GetStringFromName("notificationRestart.normal");
      msg.appendNotification(message, value, "", msg.PRIORITY_WARNING_LOW, buttons);
    }
  }
};

window.addEventListener("load", Feedback.init, false);

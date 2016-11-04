/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Constants

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const DOWNLOAD_MANAGER_URL = "chrome://mozapps/content/downloads/downloads.xul";
const PREF_FLASH_COUNT = "browser.download.manager.flashCount";

// nsDownloadManagerUI class

function nsDownloadManagerUI() {}

nsDownloadManagerUI.prototype = {
  classID: Components.ID("7dfdf0d1-aff6-4a34-bad1-d0fe74601642"),

  // nsIDownloadManagerUI

  show: function show(aWindowContext, aDownload, aReason, aUsePrivateUI)
  {
    if (!aReason)
      aReason = Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED;

    // First we see if it is already visible
    let window = this.recentWindow;
    if (window) {
      window.focus();

      // If we are being asked to show again, with a user interaction reason,
      // set the appropriate variable.
      if (aReason == Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED)
        window.gUserInteracted = true;
      return;
    }

    let parent = null;
    // We try to get a window to use as the parent here.  If we don't have one,
    // the download manager will close immediately after opening if the pref
    // browser.download.manager.closeWhenDone is set to true.
    try {
      if (aWindowContext)
        parent = aWindowContext.getInterface(Ci.nsIDOMWindow);
    } catch (e) { /* it's OK to not have a parent window */ }

    // We pass the download manager and the nsIDownload we want selected (if any)
    var params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    params.appendElement(aDownload, false);

    // Pass in the reason as well
    let reason = Cc["@mozilla.org/supports-PRInt16;1"].
                 createInstance(Ci.nsISupportsPRInt16);
    reason.data = aReason;
    params.appendElement(reason, false);

    var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
             getService(Ci.nsIWindowWatcher);
    ww.openWindow(parent,
                  DOWNLOAD_MANAGER_URL,
                  "Download:Manager",
                  "chrome,dialog=no,resizable",
                  params);
  },

  get visible() {
    return (null != this.recentWindow);
  },

  getAttention: function getAttention()
  {
    if (!this.visible)
      throw Cr.NS_ERROR_UNEXPECTED;

    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefBranch);
    // This preference may not be set, so defaulting to two.
    let flashCount = 2;
    try {
      flashCount = prefs.getIntPref(PREF_FLASH_COUNT);
    } catch (e) { }

    var win = this.recentWindow.QueryInterface(Ci.nsIDOMChromeWindow);
    win.getAttentionWithCycleCount(flashCount);
  },

  // nsDownloadManagerUI

  get recentWindow() {
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
             getService(Ci.nsIWindowMediator);
    return wm.getMostRecentWindow("Download:Manager");
  },

  // nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI])
};

// Module

var components = [nsDownloadManagerUI];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);


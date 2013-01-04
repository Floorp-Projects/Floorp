/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// Download Manager UI
// -----------------------------------------------------------------------

function DownloadManagerUI() { }

DownloadManagerUI.prototype = {
  classID: Components.ID("{93db15b1-b408-453e-9a2b-6619e168324a}"),

  show: function show(aWindowContext, aID, aReason, aUsePrivateUI) {
    if (!aReason)
      aReason = Ci.nsIDownloadManagerUI.REASON_USER_INTERACTED;

    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    if (browser)
      browser.showDownloadManager(aWindowContext, aID, aReason);
  },

  get visible() {
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    if (browser) {
      return browser.DownloadsView.visible;
    }
    return false;
  },

  getAttention: function getAttention() {
    if (this.visible)
      this.show(null, null, null);
    else
      throw Cr.NS_ERROR_UNEXPECTED;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIDownloadManagerUI])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DownloadManagerUI]);

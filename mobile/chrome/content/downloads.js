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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

var DownloadMonitor = {
  _dlmgr : null,
  _panel : null,
  _activeStr : null,
  _pausedStr : null,
  _lastTime : Infinity,
  _listening : false,

  init : function DM_init() {
    // Load the modules to help display strings
    Cu.import("resource://gre/modules/DownloadUtils.jsm");
    Cu.import("resource://gre/modules/PluralForm.jsm");

    // Initialize "private" member variables
    this._dlmgr = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    this._panel = document.getElementById("download-monitor");
    this._status = document.getElementById("download-monitor-status");
    this._time = document.getElementById("download-monitor-time");

    // Cache the status strings
    /*
    let (bundle = document.getElementById("bundle_browser")) {
      this._activeStr = bundle.getString("activeDownloads");
      this._pausedStr = bundle.getString("pausedDownloads");
    }
    */
    this._activeStr = "1 active download (#2);#1 active downloads (#2)";
    this._pausedStr = "1 paused download;#1 paused downloads";

    this._dlmgr.addListener(this);
    this._listening = true;

    this.updateStatus();
  },

  uninit: function DM_uninit() {
    if (this._listening)
      this._dlmgr.removeListener(this);
  },

  /**
   * Update status based on the number of active and paused downloads
   */
  updateStatus: function DM_updateStatus() {
    let numActive = this._dlmgr.activeDownloadCount;

    // Hide the panel and reset the "last time" if there's no downloads
    if (numActive == 0) {
      this._panel.hidePopup();
      this._lastTime = Infinity;
      return;
    }

    // Find the download with the longest remaining time
    let numPaused = 0;
    let maxTime = -Infinity;
    let dls = this._dlmgr.activeDownloads;
    while (dls.hasMoreElements()) {
      let dl = dls.getNext().QueryInterface(Ci.nsIDownload);
      if (dl.state == this._dlmgr.DOWNLOAD_DOWNLOADING) {
        // Figure out if this download takes longer
        if (dl.speed > 0 && dl.size > 0)
          maxTime = Math.max(maxTime, (dl.size - dl.amountTransferred) / dl.speed);
        else
          maxTime = -1;
      }
      else if (dl.state == this._dlmgr.DOWNLOAD_PAUSED)
        numPaused++;
    }

    // Get the remaining time string and last sec for time estimation
    let timeLeft;
    [timeLeft, this._lastTime] = DownloadUtils.getTimeLeft(maxTime, this._lastTime);

    // Figure out how many downloads are currently downloading
    let numDls = numActive - numPaused;
    let status = this._activeStr;

    // If all downloads are paused, show the paused message instead
    if (numDls == 0) {
      numDls = numPaused;
      status = this._pausedStr;
    }

    // Get the correct plural form and insert the number of downloads and time
    // left message if necessary
    status = PluralForm.get(numDls, status);
    status = status.replace("#1", numDls);

    // Update the panel and show it
    this._status.value = status;
    this._time.value = timeLeft;
    this._panel.hidden = false;
    this._panel.openPopup(null, "", 60, 50, false, false);
  },

  onProgressChange: function() {
    this.updateStatus();
  },

  onDownloadStateChange: function() {
    this.updateStatus();
  },

  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus, aDownload) {
  },

  onSecurityChange: function(aWebProgress, aRequest, aState, aDownload) {
  },

  QueryInterface : function(aIID) {
    if (aIID.equals(Components.interfaces.nsIDownloadProgressListener) ||
        aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

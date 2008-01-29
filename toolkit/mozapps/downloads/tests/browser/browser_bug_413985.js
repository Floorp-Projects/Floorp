/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

function bug413985obs(aWin)
{
  this.mWin = aWin;
  this.wasPaused = false;
  this.wasResumed = false;
}
bug413985obs.prototype = {
  observe: function(aSubject, aTopic, aData)
  {
    if ("timer-callback" == aTopic) {
      // dispatch a space keypress to resume the download
      EventUtils.synthesizeKey(" ", {}, this.mWin);
    }
  },

  onDownloadStateChange: function(aState, aDownload)
  {
    if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING &&
        !this.wasPaused) {
      this.wasPaused = true;
      // dispatch a space keypress to pause the download
      EventUtils.synthesizeKey(" ", {}, this.mWin);
    }

    if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_PAUSED &&
        !this.wasResumed) {
      this.wasResumed = true;
      // We have to do this on a timer so other JS stuff that handles the UI
      // can actually catch up to us...
      var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.init(this, 0, Ci.nsITimer.TYPE_ONE_SHOT);
    }

    if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED) {
      ok(this.wasPaused && this.wasResumed,
         "The download was paused, and then resumed to completion");
      aDownload.targetFile.remove(false);

      var dm = Cc["@mozilla.org/download-manager;1"].
                getService(Ci.nsIDownloadManager);
      dm.removeListener(this);

      finish();
    }
  },
  onStateChange: function(a, b, c, d, e) { },
  onProgressChange: function(a, b, c, d, e, f, g) { },
  onSecurityChange: function(a, b, c, d) { }
};
function test()
{
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);

  function addDownload() {
    function createURI(aObj) {
      var ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
      return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                            ios.newURI(aObj, null, null);
    }

    const nsIWBP = Ci.nsIWebBrowserPersist;
    var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);
    persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                           nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                           nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

    var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    var destFile = dirSvc.get("ProfD", Ci.nsIFile);
    destFile.append("download.result");
    if (destFile.exists())
      destFile.remove(false);

    var dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                            createURI("http://example.com/httpd.js"),
                            createURI(destFile), null, null,
                            Math.round(Date.now() * 1000), null, persist);

    persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
    persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

    return dl;
  }

  // First, we clear out the database
  dm.DBConnection.executeSimpleSQL("DELETE FROM moz_downloads");

  // See if the DM is already open, and if it is, close it!
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].
           getService(Ci.nsIWindowMediator);
  var win = wm.getMostRecentWindow("Download:Manager");
  if (win)
    win.close();

  // OK, now that all the data is in, let's pull up the UI
  Cc["@mozilla.org/download-manager-ui;1"].
  getService(Ci.nsIDownloadManagerUI).show();

  // The window doesn't open once we call show, so we need to wait a little bit
  function finishUp() {
    var win = wm.getMostRecentWindow("Download:Manager");

    dm.addListener(new bug413985obs(win));

    var dl = addDownload();
    // we need to focus the download as well
    var doc = win.document;
    doc.getElementById("downloadView").selectedIndex = 0;
  }
  
  waitForExplicitFinish();
  window.setTimeout(finishUp, 500);
}

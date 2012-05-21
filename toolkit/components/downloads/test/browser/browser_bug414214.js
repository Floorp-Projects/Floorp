/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test()
{
  const PREF_BDM_CLOSEWHENDONE = "browser.download.manager.closeWhenDone";
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var db = dm.DBConnection;

  // First, we clean up the DM
  db.executeSimpleSQL("DELETE FROM moz_downloads");

  // See if the DM is already open, and if it is, close it!
  var win = Services.wm.getMostRecentWindow("Download:Manager");
  if (win)
    win.close();

  // We need to set browser.download.manager.closeWhenDone to true to test this
  Services.prefs.setBoolPref(PREF_BDM_CLOSEWHENDONE, true);

  // register a callback to add a load listener to know when the download
  // manager opens
  Services.ww.registerNotification(function (aSubject, aTopic, aData) {
    Services.ww.unregisterNotification(arguments.callee);

    var win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
    win.addEventListener("DOMContentLoaded", finishUp, false);
  });

  // The window doesn't open once we call show, so we need to wait a little bit
  function finishUp() {
    var dmui = Cc["@mozilla.org/download-manager-ui;1"].
               getService(Ci.nsIDownloadManagerUI);
    ok(dmui.visible, "Download Manager window is open, as expected.");

    // Reset the pref to its default value
    try {
      Services.prefs.clearUserPref(PREF_BDM_CLOSEWHENDONE);
    }
    catch (err) { }

    finish();
  }
  
  // OK, let's pull up the UI
  // Linux uses y, everything else is j
  var key = navigator.platform.match("Linux") ? "y" : "j";
  EventUtils.synthesizeKey(key, {metaKey: true}, window.opener);

  waitForExplicitFinish();
}

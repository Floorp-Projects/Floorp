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

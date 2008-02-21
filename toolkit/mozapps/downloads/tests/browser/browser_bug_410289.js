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

var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var dmFile = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
dmFile.append("dmuitest.file");
dmFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
var gTestPath = ios.newFileURI(dmFile).spec;

const DownloadData = [
  { name: "Firefox 2.0.0.11.dmg",
    source: "http://mozilla-mirror.naist.jp//firefox/releases/2.0.0.11/mac/en-US/Firefox%202.0.0.11.dmg",
    target: gTestPath,
    startTime: 1200185939538521,
    endTime: 1200185939538521,
    entityID: "%22216c12-1116bd8-440070d5d2700%22/17918936/Thu, 29 Nov 2007 01:15:40 GMT",
    state: Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0 },
  { name: "Firefox 2.0.0.11.dmg",
    source: "http://mozilla-mirror.naist.jp//firefox/releases/2.0.0.11/mac/en-US/Firefox%202.0.0.11.dmg",
    target: gTestPath,
    startTime: 1200185939538520,
    endTime: 1200185939538520,
    entityID: "",
    state: Ci.nsIDownloadManager.DOWNLOAD_DOWNLOADING,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0 }
];

function test_pauseButtonCorrect(aWin)
{
  // This also tests the ordering of the display
  var doc = aWin.document;

  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);

  var richlistbox = doc.getElementById("downloadView");
  for (var i = 0; i < DownloadData.length; i++) {
    var dl = richlistbox.children[i];
    var buttons = dl.buttons;
    for (var j = 0; j < buttons.length; j++) {
      var button = buttons[j];
      if ("cmd_pause" == button.getAttribute("cmd")) {
        var id = dl.getAttribute("dlid");

        // check if it should be disabled or not
        var resumable = dm.getDownload(id).resumable;
        is(DownloadData[i].entityID ? true : false, resumable,
           "Pause button is properly disabled");

        // also check if tooltip text was updated
        if (!resumable) {
          var sb = doc.getElementById("downloadStrings");
          is(button.getAttribute("tooltiptext"), sb.getString("cannotPause"),
             "Pause button has proper text");
        }
      }
    }
  }
}

var testFuncs = [
    test_pauseButtonCorrect
];

function test()
{
  var dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  var db = dm.DBConnection;

  // First, we populate the database with some fake data
  db.executeSimpleSQL("DELETE FROM moz_downloads");
  var rawStmt = db.createStatement(
    "INSERT INTO moz_downloads (name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume, entityID) " +
    "VALUES (:name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume, :entityID)");
  var stmt = Cc["@mozilla.org/storage/statement-wrapper;1"].
             createInstance(Ci.mozIStorageStatementWrapper)
  stmt.initialize(rawStmt);
  for each (var dl in DownloadData) {
    for (var prop in dl)
      stmt.params[prop] = dl[prop];
    
    stmt.execute();
  }
  stmt.statement.finalize();

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

    // Now we can run our tests
    for each (var t in testFuncs)
      t(win);

    win.close();
    finish();
  }
  
  waitForExplicitFinish();
  // We also need to allow enough time for the DM to build up the whole list
  window.setTimeout(finishUp, 2000);
}

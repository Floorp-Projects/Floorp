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
 *   Anoop Saldanha <poonaatsoc@gmail.com>
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

/**
 * Provides utility functions for the download manager chrome tests.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

/**
 * Adds a live download to the download manager.
 *
 * @param [optional] aName
 *        The name that will be assigned to the downloaded file.
 * @returns an instance of nsIDownload.
 */
function addDownload(aName) {
    function createURI(aObj) {
      let ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
      return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                            ios.newURI(aObj, null, null);
    }

    function randomString() {
      let chars = "ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz";
      let string_length = 8;
      let randomstring = "";
      for (let i=0; i<string_length; i++) {
        let rnum = Math.floor(Math.random() * chars.length);
        randomstring += chars.substring(rnum, rnum+1);
      }
      return randomstring;
    }

    let dm = Cc["@mozilla.org/download-manager;1"].
             getService(Ci.nsIDownloadManager);
    const nsIWBP = Ci.nsIWebBrowserPersist;
    let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);
    persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                           nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                           nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;
    let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties);
    let dmFile = dirSvc.get("TmpD", Ci.nsIFile);
    dmFile.append(aName || ("dm-test-file-" + randomString()));
    if (dmFile.exists())
      throw "Download file already exists";

    let dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                            createURI("http://example.com/httpd.js"),
                            createURI(dmFile), null, null,
                            Math.round(Date.now() * 1000), null, persist);

    persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
    persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

    return dl;
  }

/**
 * Used to populate the dm with dummy download data.
 *
 * @param aDownloadData
 *        An array that contains the dummy download data to be added to the DM.
 *        Expected fields are:
 *          name, source, target, startTime, endTime, state, currBytes,
 *          maxBytes, preferredAction, and autoResume
 */
function populateDM(DownloadData)
{
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  let rawStmt = db.createStatement(
    "INSERT INTO moz_downloads (name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume) " +
    "VALUES (:name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume)");
  let stmt = Cc["@mozilla.org/storage/statement-wrapper;1"].
             createInstance(Ci.mozIStorageStatementWrapper)
  stmt.initialize(rawStmt);
  for each (let dl in DownloadData) {
    for (let prop in dl)
      stmt.params[prop] = dl[prop];

    stmt.execute();
  }
  stmt.statement.finalize();
}

/**
 * Returns an instance to the download manager window
 *
 * @return an instance of nsIDOMWindowInternal
 */
function getDMWindow()
{
  return Cc["@mozilla.org/appshell/window-mediator;1"].
         getService(Ci.nsIWindowMediator).
         getMostRecentWindow("Download:Manager");
}

/**
 * Establishes a clean state to run a test in by removing everything from the
 * database and ensuring that the download manager's window is not open.
 */
function setCleanState()
{
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);

  // Clean the dm
  dm.DBConnection.executeSimpleSQL("DELETE FROM moz_downloads");

  let win = getDMWindow();
  if (win) win.close();
}

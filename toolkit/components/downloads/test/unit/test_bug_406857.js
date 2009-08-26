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
 * The Original Code is Download Manager UI Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * Test bug 406857 to make sure a download's referrer doesn't disappear when
 * retrying the download.
 */

function run_test()
{
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (source, target, state, referrer) " +
    "VALUES (?1, ?2, ?3, ?4)");

  // Download from the test http server
  stmt.bindStringParameter(0, "http://example.com/httpd.js");

  // Download to a temp local file
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
  file.append("retry");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  stmt.bindStringParameter(1, Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService).newFileURI(file).spec);

  // Start it as canceled
  stmt.bindInt32Parameter(2, dm.DOWNLOAD_CANCELED);

  // Add a referrer to make sure it doesn't disappear
  let referrer = "http://referrer.goes/here";
  stmt.bindStringParameter(3, referrer);

  // Add it!
  stmt.execute();
  stmt.finalize();

  let listener = {
    onDownloadStateChange: function(aState, aDownload)
    {
      switch (aDownload.state) {
        case dm.DOWNLOAD_DOWNLOADING:
          do_check_eq(aDownload.referrer.spec, referrer);
          break;
        case dm.DOWNLOAD_FINISHED:
          do_check_eq(aDownload.referrer.spec, referrer);

          dm.removeListener(listener);
          try { file.remove(false); } catch(e) { /* stupid windows box */ }
          do_test_finished();
          break;
        case dm.DOWNLOAD_FAILED:
        case dm.DOWNLOAD_CANCELED:
          do_throw("Unexpected download state change received, state: " +
                   aDownload.state);
          break;
      }
    }
  };
  dm.addListener(listener);

  // Retry the download, and wait.
  dm.retryDownload(1);

  do_test_pending();
}

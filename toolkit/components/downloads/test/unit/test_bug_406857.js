/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 406857 to make sure a download's referrer doesn't disappear when
 * retrying the download.
 */

const HTTP_SERVER_PORT = 4444;

function run_test()
{
  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;
  var httpserv = new HttpServer();
  httpserv.start(HTTP_SERVER_PORT);

  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (source, target, state, referrer) " +
    "VALUES (?1, ?2, ?3, ?4)");

  // Download from the test http server
  stmt.bindByIndex(0, "http://localhost:"+HTTP_SERVER_PORT+"/httpd.js");

  // Download to a temp local file
  let file = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
  file.append("retry");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0666);
  stmt.bindByIndex(1, Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService).newFileURI(file).spec);

  // Start it as canceled
  stmt.bindByIndex(2, dm.DOWNLOAD_CANCELED);

  // Add a referrer to make sure it doesn't disappear
  let referrer = "http://referrer.goes/here";
  stmt.bindByIndex(3, referrer);

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
          httpserv.stop(do_test_finished);
          break;
        case dm.DOWNLOAD_FAILED:
        case dm.DOWNLOAD_CANCELED:
          httpserv.stop(function () {});
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

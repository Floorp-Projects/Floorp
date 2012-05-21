/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test download resume to do real-resume instead of stalling-the-channel
 * resume. Also test bug 395537 for resuming files that are deleted before
 * they're resumed. Bug 398216 is checked by making sure the reported progress
 * and file size are updated for finished downloads.
 */

const nsIF = Ci.nsIFile;
const nsIDM = Ci.nsIDownloadManager;
const nsIWBP = Ci.nsIWebBrowserPersist;
const nsIWPL = Ci.nsIWebProgressListener;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDM);

function run_test()
{
  /**
   * 1. Create data for http server to send
   */
  // data starts at 10 bytes
  var data = "1234567890";
  // data * 10^4 = 100,000 bytes (actually 101,111 bytes with newline)
  for (var i = 0; i < 4; i++) {
    data = [data,data,data,data,data,data,data,data,data,data,"\n"].join("");
  }

  /**
   * 2. Start the http server that can handle resume
   */
  var httpserv = new nsHttpServer();
  var didResumeServer = false;
  httpserv.registerPathHandler("/resume", function(meta, resp) {
    var body = data;
    resp.setHeader("Content-Type", "text/html", false);
    if (meta.hasHeader("Range")) {
      // track that we resumed for testing
      didResumeServer = true;
      // Syntax: bytes=[from]-[to] (we don't support multiple ranges)
      var matches = meta.getHeader("Range").match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
      var from = (matches[1] === undefined) ? 0 : matches[1];
      var to = (matches[2] === undefined) ? data.length - 1 : matches[2];
      if (from >= data.length) {
        resp.setStatusLine(meta.httpVersion, 416, "Start pos too high");
        resp.setHeader("Content-Range", "*/" + data.length, false);
        return;
      }
      body = body.substring(from, to + 1);
      // always respond to successful range requests with 206
      resp.setStatusLine(meta.httpVersion, 206, "Partial Content");
      resp.setHeader("Content-Range", from + "-" + to + "/" + data.length, false);
    }
    resp.bodyOutputStream.write(body, body.length);
  });
  httpserv.start(4444);

  /**
   * 3. Perform various actions for certain download states
   */
  var didPause = false;
  var didResumeDownload = false;
  dm.addListener({
    onDownloadStateChange: function(a, aDl) {
      if (aDl.state == nsIDM.DOWNLOAD_DOWNLOADING && !didPause) {
        /**
         * (1) queued -> downloading = pause the download
         */
        dm.pauseDownload(aDl.id);
      } else if (aDl.state == nsIDM.DOWNLOAD_PAUSED) {
        /**
         * (2) downloading -> paused = remove the file
         */
        didPause = true;
        // Test bug 395537 by removing the file before we actually resume
        aDl.targetFile.remove(false);
      } else if (aDl.state == nsIDM.DOWNLOAD_FINISHED) {
        /**
         * (4) downloading (resumed) -> finished = check tests
         */
        // did we pause at all?
        do_check_true(didPause);
        // did we real-resume and not fake-resume?
        do_check_true(didResumeDownload);
        // extra real-resume check for the server
        do_check_true(didResumeServer);
        // did we download the whole file?
        do_check_eq(data.length, aDl.targetFile.fileSize);
        // extra sanity checks on size (test bug 398216)
        do_check_eq(data.length, aDl.amountTransferred);
        do_check_eq(data.length, aDl.size);

        httpserv.stop(do_test_finished);
      }
    },
    onStateChange: function(a, b, aState, d, aDl) {
      if ((aState & nsIWPL.STATE_STOP) && didPause && !didResumeServer &&
          !didResumeDownload) {
        /**
         * (3) paused -> stopped = resume the download
         */
        dm.resumeDownload(aDl.id);
        didResumeDownload = true;
      }
    },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  });
  dm.addListener(getDownloadListener());

  /**
   * 4. Start the download
   */
  var destFile = dirSvc.get("ProfD", nsIF);
  destFile.append("resumed");
  if (destFile.exists())
    destFile.remove(false);
  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
                createInstance(nsIWBP);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;
  var dl = dm.addDownload(nsIDM.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI("http://localhost:4444/resume"),
                          createURI(destFile), null, null,
                          Math.round(Date.now() * 1000), null, persist);
  persist.progressListener = dl.QueryInterface(nsIWPL);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

  // Mark as pending, so clear this when we actually finish the download
  do_test_pending();
}

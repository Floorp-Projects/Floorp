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
 * The Original Code is Download Manager Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2007
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
        resp.setHeader("Content-Range", "*/" + data.length);
        return;
      }
      body = body.substring(from, to + 1);
      // always respond to successful range requests with 206
      resp.setStatusLine(meta.httpVersion, 206, "Partial Content");
      resp.setHeader("Content-Range", from + "-" + to + "/" + data.length);
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

        httpserv.stop();
        // we're done with the test!
        do_test_finished();
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
    onStatusChange: function(a, b, c, d, e) { },
    onLocationChange: function(a, b, c, d) { },
    onSecurityChange: function(a, b, c, d) { }
  });

  /**
   * 4. Start the download
   */
  var destFile = dirSvc.get("ProfD", nsIF);
  destFile.append("resumed");
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

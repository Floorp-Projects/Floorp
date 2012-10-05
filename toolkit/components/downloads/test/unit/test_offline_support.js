/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 437415 by making sure the download manager responds to offline and
 * online notifications by pausing and resuming downloads.
 */

/**
 * Used to indicate if we should error out or not.  See bug 431745 for more
 * details.
 */
let doNotError = false;

const nsIF = Ci.nsIFile;
const nsIDM = Ci.nsIDownloadManager;
const nsIWBP = Ci.nsIWebBrowserPersist;
const nsIWPL = Ci.nsIWebProgressListener;
const dm = Cc["@mozilla.org/download-manager;1"].getService(nsIDM);
dm.cleanUp();

function setOnlineState(aOnline)
{
  // We do not actually set the offline state because that introduces some neat
  // conditions when being called within a listener.  We do dispatch the right
  // observer topics though, so this tests just the download manager.
  let topic = aOnline ?
    "network:offline-status-changed" :
    "network:offline-about-to-go-offline";
  let state = aOnline ? "online" : "offline";
  Cc["@mozilla.org/observer-service;1"].
  getService(Ci.nsIObserverService).
  notifyObservers(null, topic, state);
}

function run_test()
{
  /**
   * 1. Create data for http server to send
   */
  // data starts at 10 bytes
  let data = "1234567890";
  // data * 10^4 = 100,000 bytes (actually 101,111 bytes with newline)
  for (let i = 0; i < 4; i++)
    data = [data,data,data,data,data,data,data,data,data,data,"\n"].join("");

  /**
   * 2. Start the http server that can handle resume
   */
  let httpserv = new HttpServer();
  let didResumeServer = false;
  httpserv.registerPathHandler("/resume", function(meta, resp) {
    let body = data;
    resp.setHeader("Content-Type", "text/html", false);
    if (meta.hasHeader("Range")) {
      // track that we resumed for testing
      didResumeServer = true;
      // Syntax: bytes=[from]-[to] (we don't support multiple ranges)
      let matches = meta.getHeader("Range").match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
      let from = (matches[1] === undefined) ? 0 : matches[1];
      let to = (matches[2] === undefined) ? data.length - 1 : matches[2];
      if (from >= data.length) {
        resp.setStatusLine(meta.httpVersion, 416, "Start pos too high");
        resp.setHeader("Content-Range", "*/" + data.length);
        dump("Returning early - from >= data.length.  Not an error (bug 431745)\n");
        doNotError = true;
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
  let didPause = false;
  let didResumeDownload = false;
  dm.addListener({
    onDownloadStateChange: function(a, aDl) {
      if (aDl.state == nsIDM.DOWNLOAD_DOWNLOADING && !didPause) {
        /**
         * (1) queued -> downloading = pause the download by going offline
         */
        setOnlineState(false);
      } else if (aDl.state == nsIDM.DOWNLOAD_PAUSED) {
        /**
         * (2) downloading -> paused
         */
        didPause = true;
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

        httpserv.stop(do_test_finished);
        aDl.targetFile.remove(false);
      }
      else if (aDl.state == nsIDM.DOWNLOAD_FAILED) {
        // this is only ok if we are not supposed to fail
        do_check_true(doNotError);
        httpserv.stop(do_test_finished);
      }
    },
    onStateChange: function(a, b, aState, d, aDl) {
      if ((aState & nsIWPL.STATE_STOP) && didPause && !didResumeServer &&
          !didResumeDownload) {
        /**
         * (3) paused -> stopped = resume the download by coming online
         */
        setOnlineState(true);
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
  let destFile = dirSvc.get("ProfD", nsIF);
  destFile.append("offline_online");
  if (destFile.exists())
    destFile.remove(false);
  let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
                createInstance(nsIWBP);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;
  let dl = dm.addDownload(nsIDM.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI("http://localhost:4444/resume"),
                          createURI(destFile), null, null,
                          Math.round(Date.now() * 1000), null, persist, false);
  persist.progressListener = dl.QueryInterface(nsIWPL);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile, null);

  // Mark as pending, so clear this when we actually finish the download
  do_test_pending();
}

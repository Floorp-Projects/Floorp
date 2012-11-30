/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let HttpServer =
  Cu.import("resource://testing-common/httpd.js", {}).HttpServer;

function createURI(aObj) {
  return (aObj instanceof Ci.nsIFile) ?
          Services.io.newFileURI(aObj) : Services.io.newURI(aObj, null, null);
}

var gDownloadCount = 0;
/**
 * Adds a download to the DM, and starts it.
 * @param aParams (optional): an optional object which contains the function
 *                            parameters:
 *                              resultFileName: leaf node for the target file
 *                              targetFile: nsIFile for the target (overrides resultFileName)
 *                              sourceURI: the download source URI
 *                              downloadName: the display name of the download
 *                              runBeforeStart: a function to run before starting the download
 *                              isPrivate: whether the download is private or not
 */
function addDownload(aParams) {
  if (!aParams)
    aParams = {};
  if (!("resultFileName" in aParams))
    aParams.resultFileName = "download.result";
  if (!("targetFile" in aParams)) {
    aParams.targetFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
    aParams.targetFile.append(aParams.resultFileName);
  }
  if (!("sourceURI" in aParams))
    aParams.sourceURI = "http://localhost:4444/head.js";
  if (!("downloadName" in aParams))
    aParams.downloadName = null;
  if (!("runBeforeStart" in aParams))
    aParams.runBeforeStart = function () {};

  const nsIWBP = Ci.nsIWebBrowserPersist;
  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
                createInstance(Ci.nsIWebBrowserPersist);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  // it is part of the active downloads the moment addDownload is called
  gDownloadCount++;

  var dl =
    Services.downloads.addDownload(
      Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
      createURI(aParams.sourceURI), createURI(aParams.targetFile),
      aParams.downloadName, null, Math.round(Date.now() * 1000), null,
      persist, aParams.isPrivate);

  // This will throw if it isn't found, and that would mean test failure, so no
  // try catch block
  if (!aParams.isPrivate)
    Services.downloads.getDownload(dl.id);

  aParams.runBeforeStart.call(undefined, dl);

  persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
  persist.savePrivacyAwareURI(dl.source, null, null, null, null, dl.targetFile,
                              aParams.isPrivate);

  return dl;
}

function whenNewWindowLoaded(aIsPrivate, aCallback) {
  let win = OpenBrowserWindow({private: aIsPrivate});
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(function() aCallback(win));
  }, false);
}

function startServer() {
  let httpServer = new HttpServer();
  let currentWorkDir = Services.dirsvc.get("CurWorkD", Ci.nsILocalFile);
  httpServer.registerDirectory("/file/", currentWorkDir);
  httpServer.registerPathHandler("/noresume", function (meta, response) {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Accept-Ranges", "none", false);
    response.write("foo");
  });
  httpServer.start(4444);
  return httpServer;
}
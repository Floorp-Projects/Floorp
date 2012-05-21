/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the download manager backend

do_load_httpd_js();
do_get_profile();

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var downloadUtils = { };
XPCOMUtils.defineLazyServiceGetter(downloadUtils,
                                   "downloadManager",
                                   "@mozilla.org/download-manager;1",
                                   Ci.nsIDownloadManager);

function createURI(aObj)
{
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return (aObj instanceof Ci.nsIFile) ? ios.newFileURI(aObj) :
                                        ios.newURI(aObj, null, null);
}

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

var provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == "DLoads") {
      var file = dirSvc.get("ProfD", Ci.nsILocalFile);
      file.append("downloads.rdf");
      return file;
     }
    print("*** Throwing trying to get " + prop);
    throw Cr.NS_ERROR_FAILURE;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);

/**
 * Imports a download test file to use.  Works with rdf and sqlite files.
 *
 * @param aFName
 *        The name of the file to import.  This file should be located in the
 *        same directory as this file.
 */
function importDownloadsFile(aFName)
{
  var file = do_get_file(aFName);
  var newFile = dirSvc.get("ProfD", Ci.nsIFile);
  if (/\.rdf$/i.test(aFName))
    file.copyTo(newFile, "downloads.rdf");
  else if (/\.sqlite$/i.test(aFName))
    file.copyTo(newFile, "downloads.sqlite");
  else
    do_throw("Unexpected filename!");
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
 */
function addDownload(aParams)
{
  if (!aParams)
    aParams = {};
  if (!("resultFileName" in aParams))
    aParams.resultFileName = "download.result";
  if (!("targetFile" in aParams)) {
    aParams.targetFile = dirSvc.get("ProfD", Ci.nsIFile);
    aParams.targetFile.append(aParams.resultFileName);
  }
  if (!("sourceURI" in aParams))
    aParams.sourceURI = "http://localhost:4444/head_download_manager.js";
  if (!("downloadName" in aParams))
    aParams.downloadName = null;
  if (!("runBeforeStart" in aParams))
    aParams.runBeforeStart = function () {};

  const nsIWBP = Ci.nsIWebBrowserPersist;
  var persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                .createInstance(Ci.nsIWebBrowserPersist);
  persist.persistFlags = nsIWBP.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                         nsIWBP.PERSIST_FLAGS_BYPASS_CACHE |
                         nsIWBP.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  // it is part of the active downloads the moment addDownload is called
  gDownloadCount++;

  var dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI(aParams.sourceURI),
                          createURI(aParams.targetFile), aParams.downloadName, null,
                          Math.round(Date.now() * 1000), null, persist);

  // This will throw if it isn't found, and that would mean test failure, so no
  // try catch block
  var test = dm.getDownload(dl.id);

  aParams.runBeforeStart.call(undefined, dl);

  persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
  persist.saveURI(dl.source, null, null, null, null, dl.targetFile);

  return dl;
}

function getDownloadListener()
{
  return {
    onDownloadStateChange: function(aState, aDownload)
    {
      if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_QUEUED)
        do_test_pending();

      if (aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FINISHED ||
          aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_CANCELED ||
          aDownload.state == Ci.nsIDownloadManager.DOWNLOAD_FAILED) {
          gDownloadCount--;
        do_test_finished();
      }

      if (gDownloadCount == 0 && typeof httpserv != "undefined" && httpserv)
      {
        do_test_pending();
        httpserv.stop(do_test_finished);
      }
    },
    onStateChange: function(a, b, c, d, e) { },
    onProgressChange: function(a, b, c, d, e, f, g) { },
    onSecurityChange: function(a, b, c, d) { }
  };
}

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Cu.import("resource://gre/modules/Services.jsm");
  return Services;
});

// Disable alert service notifications
Services.prefs.setBoolPref("browser.download.manager.showAlertOnComplete", false);

do_register_cleanup(function() {
  Services.obs.notifyObservers(null, "quit-application", null);
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the download manager backend

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

do_get_profile();

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

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

var gDownloadCount = 0;
/**
 * Adds a download to the DM, and starts it.
 * @param server: a HttpServer used to serve the sourceURI
 * @param aParams (optional): an optional object which contains the function
 *                            parameters:
 *                              resultFileName: leaf node for the target file
 *                              targetFile: nsIFile for the target (overrides resultFileName)
 *                              sourceURI: the download source URI
 *                              downloadName: the display name of the download
 *                              runBeforeStart: a function to run before starting the download
 *                              isPrivate: whether the download is private or not
 */
function addDownload(server, aParams)
{
  if (!server)
    do_throw("Must provide a valid server.");
  const PORT = server.identity.primaryPort;
  if (!aParams)
    aParams = {};
  if (!("resultFileName" in aParams))
    aParams.resultFileName = "download.result";
  if (!("targetFile" in aParams)) {
    aParams.targetFile = dirSvc.get("ProfD", Ci.nsIFile);
    aParams.targetFile.append(aParams.resultFileName);
  }
  if (!("sourceURI" in aParams))
    aParams.sourceURI = "http://localhost:" + PORT + "/head_download_manager.js";
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

  let dm = downloadUtils.downloadManager;
  var dl = dm.addDownload(Ci.nsIDownloadManager.DOWNLOAD_TYPE_DOWNLOAD,
                          createURI(aParams.sourceURI),
                          createURI(aParams.targetFile), aParams.downloadName, null,
                          Math.round(Date.now() * 1000), null, persist, aParams.isPrivate);

  // This will throw if it isn't found, and that would mean test failure, so no
  // try catch block
  if (!aParams.isPrivate)
    var test = dm.getDownload(dl.id);

  aParams.runBeforeStart.call(undefined, dl);

  persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
  persist.savePrivacyAwareURI(dl.source, null, null, 0, null, null, dl.targetFile,
                              aParams.isPrivate);

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

/**
 * Asynchronously adds visits to a page.
 *
 * @param aPlaceInfo
 *        Can be an nsIURI, in such a case a single LINK visit will be added.
 *        Otherwise can be an object describing the visit to add, or an array
 *        of these objects:
 *          { uri: nsIURI of the page,
 *            transition: one of the TRANSITION_* from nsINavHistoryService,
 *            [optional] title: title of the page,
 *            [optional] visitDate: visit date in microseconds from the epoch
 *            [optional] referrer: nsIURI of the referrer for this visit
 *          }
 *
 * @return {Promise}
 * @resolves When all visits have been added successfully.
 * @rejects JavaScript exception.
 */
function promiseAddVisits(aPlaceInfo)
{
  let deferred = Promise.defer();
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  }
  else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo)
  }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function handleError(aResultCode, aPlaceInfo) {
        let ex = new Components.Exception("Unexpected error in adding visits.",
                                          aResultCode);
        deferred.reject(ex);
      },
      handleResult: function () {},
      handleCompletion: function handleCompletion() {
        deferred.resolve();
      }
    }
  );

  return deferred.promise;
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

function oldDownloadManagerDisabled() {
  try {
    // This method throws an exception if the old Download Manager is disabled.
    Services.downloads.activeDownloadCount;
  } catch (ex) {
    return true;
  }
  return false;
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides utility functions for the download manager chrome tests.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

/**
 * Returns the toolkit implementation of the download manager UI service.
 * If the toolkit implementation of the service can't be found (e.g. because
 * SeaMonkey doesn't package that version but an own implementation that calls
 * different UI), then returns false (see bug 483781).
 *
 * @returns toolkit's nsIDownloadManagerUI implementation or false if not found
 */
function getDMUI()
{
  if (Components.classesByID["{7dfdf0d1-aff6-4a34-bad1-d0fe74601642}"])
    return Components.classesByID["{7dfdf0d1-aff6-4a34-bad1-d0fe74601642}"].
           getService(Ci.nsIDownloadManagerUI);
  return false;
}

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
                            Math.round(Date.now() * 1000), null, persist, false);

    let privacyContext = window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation)
                               .QueryInterface(Ci.nsILoadContext);

    persist.progressListener = dl.QueryInterface(Ci.nsIWebProgressListener);
    persist.saveURI(dl.source, null, null, null, null, dl.targetFile, privacyContext);

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

  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume) " +
    "VALUES (:name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume)");
  for each (let dl in DownloadData) {
    for (let prop in dl)
      stmt.params[prop] = dl[prop];

    stmt.execute();
  }
  stmt.finalize();
}

/**
 * Returns an instance to the download manager window
 *
 * @return an instance of nsIDOMWindow
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

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  Components.utils.import("resource://gre/modules/PlacesUtils.jsm");
  Components.utils.import("resource://gre/modules/Services.jsm");
  Services.obs.addObserver(function observeClearHistory(aSubject, aTopic) {
    Services.obs.removeObserver(observeClearHistory, aTopic);
    aCallback();
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  PlacesUtils.bhistory.removeAllPages();
}

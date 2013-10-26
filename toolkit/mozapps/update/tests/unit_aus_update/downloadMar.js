/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General MAR File Download Tests */

const INC_CONTRACT_ID = "@mozilla.org/network/incremental-download;1";

var gNextRunFunc;
var gStatusResult;
var gExpectedStatusResult;
var gIncrementalDownloadClassID, gIncOldFactory;

// gIncrementalDownloadErrorType is used to loop through each of the connection
// error types in the Mock incremental downloader.
var gIncrementalDownloadErrorType = 0;

function run_test() {
  setupTestCommon(true);

  logTestInfo("testing mar downloads, mar hash verification, and " +
              "mar download interrupted recovery");

  Services.prefs.setBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false);
  // The HTTP server is only used for the mar file downloads since it is slow
  start_httpserver();
  setUpdateURLOverride(gURLData + "update.xml");
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  do_execute_soon(run_test_pt1);
}

// The HttpServer must be stopped before calling do_test_finished
function finish_test() {
  stop_httpserver(do_test_finished);
}

function end_test() {
  cleanupMockIncrementalDownload();
  cleanupTestCommon();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = 400;
  gXHR.responseText = gResponseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  }
  catch(e) {
  }
  var e = { target: gXHR };
  gXHR.onload(e);
}

// Helper function for testing mar downloads that have the correct size
// specified in the update xml.
function run_test_helper_pt1(aMsg, aExpectedStatusResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusResult = aExpectedStatusResult;
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED)
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt1_2() {
  do_check_eq(gStatusResult, gExpectedStatusResult);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

// The following 3 functions are a workaround for GONK due to Bug 828858 and
// can be removed after it is fixed and the callers are changed to use the
// regular helper functions.
function run_test_helper_bug828858_pt1(aMsg, aExpectedStatusResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_bug828858_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusResult = aExpectedStatusResult;
  logTestInfo(aMsg, Components.stack.caller);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_bug828858_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_bug828858_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED)
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_bug828858_pt1_2() {
  if (gStatusResult == AUS_Cr.NS_ERROR_CONTENT_CORRUPTED) {
    do_check_eq(gStatusResult, AUS_Cr.NS_ERROR_CONTENT_CORRUPTED);
  } else {
    do_check_eq(gStatusResult, gExpectedStatusResult);
  }
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

function setResponseBody(aHashFunction, aHashValue, aSize) {
  var patches = getRemotePatchString(null, null,
                                     aHashFunction, aHashValue, aSize);
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
}

// mar download with a valid MD5 hash
function run_test_pt1() {
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid MD5 hash",
                      AUS_Cr.NS_OK, run_test_pt2);
}

// mar download with an invalid MD5 hash
function run_test_pt2() {
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid MD5 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt3);
}

// mar download with a valid SHA1 hash
function run_test_pt3() {
  setResponseBody("SHA1", SHA1_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA1 hash",
                      AUS_Cr.NS_OK, run_test_pt4);
}

// mar download with an invalid SHA1 hash
function run_test_pt4() {
  setResponseBody("SHA1", SHA1_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA1 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt5);
}

// mar download with a valid SHA256 hash
function run_test_pt5() {
  setResponseBody("SHA256", SHA256_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA256 hash",
                      AUS_Cr.NS_OK, run_test_pt6);
}

// mar download with an invalid SHA256 hash
function run_test_pt6() {
  setResponseBody("SHA256", SHA256_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA256 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt7);
}

// mar download with a valid SHA384 hash
function run_test_pt7() {
  setResponseBody("SHA384", SHA384_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA384 hash",
                      AUS_Cr.NS_OK, run_test_pt8);
}

// mar download with an invalid SHA384 hash
function run_test_pt8() {
  setResponseBody("SHA384", SHA384_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA384 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt9);
}

// mar download with a valid SHA512 hash
function run_test_pt9() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with a valid SHA512 hash",
                      AUS_Cr.NS_OK, run_test_pt10);
}

// mar download with an invalid SHA384 hash
function run_test_pt10() {
  setResponseBody("SHA512", SHA512_HASH_SIMPLE_MAR + "0");
  run_test_helper_pt1("mar download with an invalid SHA512 hash",
                      AUS_Cr.NS_ERROR_CORRUPTED_CONTENT, run_test_pt11);
}

// mar download with the mar not found
function run_test_pt11() {
  var patches = getRemotePatchString(null, gURLData + "missing.mar");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("mar download with the mar not found",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt12);
}

// mar download with a valid MD5 hash but invalid file size
function run_test_pt12() {
  const arbitraryFileSize = 1024000;
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR, arbitraryFileSize);
  if (IS_TOOLKIT_GONK) {
    // There seems to be a race on the web server side when the patchFile is
    // stored on the SDCard. Sometimes, the webserver will serve up an error
    // 416 and the contents of the file, and sometimes it will serve up an error
    // 200 and no contents. This can cause either NS_ERROR_UNEXPECTED or
    // NS_ERROR_CONTENT_CORRUPTED.
    // Bug 828858 was filed to follow up on this issue.
    run_test_helper_bug828858_pt1("mar download with a valid MD5 hash but invalid file size",
                                  AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt13);
  } else {
    run_test_helper_pt1("mar download with a valid MD5 hash but invalid file size",
                        AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt13);
  }
}

var newFactory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new IncrementalDownload().QueryInterface(aIID);
  },
  lockFactory: function(aLock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsIFactory])
};

function initMockIncrementalDownload() {
  var registrar = AUS_Cm.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  gIncrementalDownloadClassID = registrar.contractIDToCID(INC_CONTRACT_ID);
  gIncOldFactory = AUS_Cm.getClassObject(AUS_Cc[INC_CONTRACT_ID],
                                         AUS_Ci.nsIFactory);
  registrar.unregisterFactory(gIncrementalDownloadClassID, gIncOldFactory);
  var components = [IncrementalDownload];
  registrar.registerFactory(gIncrementalDownloadClassID, "",
                            INC_CONTRACT_ID, newFactory);
}

function cleanupMockIncrementalDownload() {
  if (gIncOldFactory) {
    var registrar = AUS_Cm.QueryInterface(AUS_Ci.nsIComponentRegistrar);
    registrar.unregisterFactory(gIncrementalDownloadClassID, newFactory);
    registrar.registerFactory(gIncrementalDownloadClassID, "",
                              INC_CONTRACT_ID, gIncOldFactory);
  }
  gIncOldFactory = null;
}

/* This Mock incremental downloader is used to verify that connection
 * interrupts work correctly in updater code.  The implementation of
 * the mock incremental downloader is very simple, it simply copies
 * the file to the destination location.
*/

function IncrementalDownload() {
  this.wrappedJSObject = this;
}

IncrementalDownload.prototype = {
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsIIncrementalDownload]),

  /* nsIIncrementalDownload */
  init: function(uri, file, chunkSize, intervalInSeconds) {
    this._destination = file;
    this._URI = uri;
    this._finalURI = uri;
  },

  start: function(observer, ctxt) {
    var tm = Components.classes["@mozilla.org/thread-manager;1"].
                        getService(AUS_Ci.nsIThreadManager);
    // Do the actual operation async to give a chance for observers
    // to add themselves.
    tm.mainThread.dispatch(function() {
        this._observer = observer.QueryInterface(AUS_Ci.nsIRequestObserver);
        this._ctxt = ctxt;
        this._observer.onStartRequest(this, this.ctxt);
        let mar = getTestDirFile(FILE_SIMPLE_MAR);
        mar.copyTo(this._destination.parent, this._destination.leafName);
        var status = AUS_Cr.NS_OK
        switch (gIncrementalDownloadErrorType++) {
          case 0:
            status = AUS_Cr.NS_ERROR_NET_RESET;
          break;
          case 1:
            status = AUS_Cr.NS_ERROR_CONNECTION_REFUSED;
          break;
          case 2:
            status = AUS_Cr.NS_ERROR_NET_RESET;
          break;
          case 3:
            status = AUS_Cr.NS_OK;
            break;
          case 4:
            status = AUS_Cr.NS_ERROR_OFFLINE;
            // After we report offline, we want to eventually show offline
            // status being changed to online.
            var tm = Components.classes["@mozilla.org/thread-manager;1"].
                                getService(AUS_Ci.nsIThreadManager);
            tm.mainThread.dispatch(function() {
              Services.obs.notifyObservers(gAUS,
                                           "network:offline-status-changed",
                                           "online");
            }, AUS_Ci.nsIThread.DISPATCH_NORMAL);
          break;
        }
        this._observer.onStopRequest(this, this._ctxt, status);
      }.bind(this), AUS_Ci.nsIThread.DISPATCH_NORMAL);
  },

  get URI() {
    return this._URI;
  },

  get currentSize() {
    throw AUS_Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  get destination() {
    return this._destination;
  },

  get finalURI() {
    return this._finalURI;
  },

  get totalSize() {
    throw AUS_Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /* nsIRequest */
  cancel: function(aStatus) {
    throw AUS_Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  suspend: function() {
    throw AUS_Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  isPending: function() {
    throw AUS_Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  _loadFlags: 0,
  get loadFlags() {
    return this._loadFlags;
  },
  set loadFlags(val) {
    this._loadFlags = val;
  },

  _loadGroup: null,
  get loadGroup() {
    return this._loadGroup;
  },
  set loadGroup(val) {
    this._loadGroup = val;
  },

  _name: "",
  get name() {
    return this._name;
  },

  _status: 0,
  get status() {
    return this._status;
  }
}

// Test disconnecting during an update
function run_test_pt13() {
  initMockIncrementalDownload();
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with connection interruption",
                      AUS_Cr.NS_OK, run_test_pt14);
}

// Test disconnecting during an update
function run_test_pt14() {
  gIncrementalDownloadErrorType = 0;
  Services.prefs.setIntPref(PREF_APP_UPDATE_SOCKET_ERRORS, 2);
  Services.prefs.setIntPref(PREF_APP_UPDATE_RETRY_TIMEOUT, 0);
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR);

  var expectedResult;
  if (IS_TOOLKIT_GONK) {
    // Gonk treats interrupted downloads differently. For gonk, if the state
    // is pending, this means that the download has completed and only the
    // staging needs to occur. So gonk will skip the download portion which
    // results in an NS_OK return.
    expectedResult = AUS_Cr.NS_OK;
  } else {
    expectedResult = AUS_Cr.NS_ERROR_NET_RESET;
  }
  run_test_helper_pt1("mar download with connection interruption without recovery",
                      expectedResult, run_test_pt15);
}

// Test entering offline mode while downloading
function run_test_pt15() {
  gIncrementalDownloadErrorType = 4;
  setResponseBody("MD5", MD5_HASH_SIMPLE_MAR);
  run_test_helper_pt1("mar download with offline mode",
                      AUS_Cr.NS_OK, finish_test);
}

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function DL_onStartRequest(request, context) {
  },

  onProgress: function DL_onProgress(request, context, progress, maxProgress) {
  },

  onStatus: function DL_onStatus(request, context, status, statusText) {
  },

  onStopRequest: function DL_onStopRequest(request, context, status) {
    gStatusResult = status;
    // Use a timeout to allow the request to complete
    do_execute_soon(gCheckFunc);
  },

  QueryInterface: function DL_QueryInterface(iid) {
    if (!iid.equals(AUS_Ci.nsIRequestObserver) &&
        !iid.equals(AUS_Ci.nsIProgressEventSink) &&
        !iid.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

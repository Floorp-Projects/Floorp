/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadIntegration",
                                  "resource://gre/modules/DownloadIntegration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "HttpServer",
                                  "resource://testing-common/httpd.js");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
                                  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MockRegistrar",
                                  "resource://testing-common/MockRegistrar.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gExternalHelperAppService",
           "@mozilla.org/uriloader/external-helper-app-service;1",
           Ci.nsIExternalHelperAppService);

const ServerSocket = Components.Constructor(
                                "@mozilla.org/network/server-socket;1",
                                "nsIServerSocket",
                                "init");
const BinaryOutputStream = Components.Constructor(
                                      "@mozilla.org/binaryoutputstream;1",
                                      "nsIBinaryOutputStream",
                                      "setOutputStream")

XPCOMUtils.defineLazyServiceGetter(this, "gMIMEService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");

const TEST_TARGET_FILE_NAME = "test-download.txt";
const TEST_STORE_FILE_NAME = "test-downloads.json";

const TEST_REFERRER_URL = "http://www.example.com/referrer.html";

const TEST_DATA_SHORT = "This test string is downloaded.";
// Generate using gzipCompressString in TelemetryController.jsm.
const TEST_DATA_SHORT_GZIP_ENCODED_FIRST = [
 31,139,8,0,0,0,0,0,0,3,11,201,200,44,86,40,73,45,46,81,40,46,41,202,204
];
const TEST_DATA_SHORT_GZIP_ENCODED_SECOND = [
  75,87,0,114,83,242,203,243,114,242,19,83,82,83,244,0,151,222,109,43,31,0,0,0
];
const TEST_DATA_SHORT_GZIP_ENCODED =
  TEST_DATA_SHORT_GZIP_ENCODED_FIRST.concat(TEST_DATA_SHORT_GZIP_ENCODED_SECOND);

/**
 * All the tests are implemented with add_task, this starts them automatically.
 */
function run_test()
{
  do_get_profile();
  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Support functions

/**
 * HttpServer object initialized before tests start.
 */
let gHttpServer;

/**
 * Given a file name, returns a string containing an URI that points to the file
 * on the currently running instance of the test HTTP server.
 */
function httpUrl(aFileName) {
  return "http://localhost:" + gHttpServer.identity.primaryPort + "/" +
         aFileName;
}

// While the previous test file should have deleted all the temporary files it
// used, on Windows these might still be pending deletion on the physical file
// system.  Thus, start from a new base number every time, to make a collision
// with a file that is still pending deletion highly unlikely.
let gFileCounter = Math.floor(Math.random() * 1000000);

/**
 * Returns a reference to a temporary file, that is guaranteed not to exist, and
 * to have never been created before.
 *
 * @param aLeafName
 *        Suggested leaf name for the file to be created.
 *
 * @return nsIFile pointing to a non-existent file in a temporary directory.
 *
 * @note It is not enough to delete the file if it exists, or to delete the file
 *       after calling nsIFile.createUnique, because on Windows the delete
 *       operation in the file system may still be pending, preventing a new
 *       file with the same name to be created.
 */
function getTempFile(aLeafName)
{
  // Prepend a serial number to the extension in the suggested leaf name.
  let [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
  let leafName = base + "-" + gFileCounter + ext;
  gFileCounter++;

  // Get a file reference under the temporary directory for this test file.
  let file = FileUtils.getFile("TmpD", [leafName]);
  do_check_false(file.exists());

  do_register_cleanup(function () {
    if (file.exists()) {
      file.remove(false);
    }
  });

  return file;
}

/**
 * Waits for pending events to be processed.
 *
 * @return {Promise}
 * @resolves When pending events have been processed.
 * @rejects Never.
 */
function promiseExecuteSoon()
{
  let deferred = Promise.defer();
  do_execute_soon(deferred.resolve);
  return deferred.promise;
}

/**
 * Waits for a pending events to be processed after a timeout.
 *
 * @return {Promise}
 * @resolves When pending events have been processed.
 * @rejects Never.
 */
function promiseTimeout(aTime)
{
  let deferred = Promise.defer();
  do_timeout(aTime, deferred.resolve);
  return deferred.promise;
}

/**
 * Waits for a new history visit to be notified for the specified URI.
 *
 * @param aUrl
 *        String containing the URI that will be visited.
 *
 * @return {Promise}
 * @resolves Array [aTime, aTransitionType] from nsINavHistoryObserver.onVisit.
 * @rejects Never.
 */
function promiseWaitForVisit(aUrl)
{
  let deferred = Promise.defer();

  let uri = NetUtil.newURI(aUrl);

  PlacesUtils.history.addObserver({
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver]),
    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onVisit: function (aURI, aVisitID, aTime, aSessionID, aReferringID,
                       aTransitionType, aGUID, aHidden) {
      if (aURI.equals(uri)) {
        PlacesUtils.history.removeObserver(this);
        deferred.resolve([aTime, aTransitionType]);
      }
    },
    onTitleChanged: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function () {},
  }, false);

  return deferred.promise;
}

/**
 * Check browsing history to see whether the given URI has been visited.
 *
 * @param aUrl
 *        String containing the URI that will be visited.
 *
 * @return {Promise}
 * @resolves Boolean indicating whether the URI has been visited.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aUrl) {
  let deferred = Promise.defer();

  PlacesUtils.asyncHistory.isURIVisited(NetUtil.newURI(aUrl),
    function (aURI, aIsVisited) {
      deferred.resolve(aIsVisited);
    });

  return deferred.promise;
}

/**
 * Creates a new Download object, setting a temporary file as the target.
 *
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("source.txt").
 *
 * @return {Promise}
 * @resolves The newly created Download object.
 * @rejects JavaScript exception.
 */
function promiseNewDownload(aSourceUrl) {
  return Downloads.createDownload({
    source: aSourceUrl || httpUrl("source.txt"),
    target: getTempFile(TEST_TARGET_FILE_NAME),
  });
}

/**
 * Starts a new download using the nsIWebBrowserPersist interface, and controls
 * it using the legacy nsITransfer interface.
 *
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("source.txt").
 * @param aOptions
 *        An optional object used to control the behavior of this function.
 *        You may pass an object with a subset of the following fields:
 *        {
 *          isPrivate: Boolean indicating whether the download originated from a
 *                     private window.
 *          targetFile: nsIFile for the target, or null to use a temporary file.
 *          outPersist: Receives a reference to the created nsIWebBrowserPersist
 *                      instance.
 *          launchWhenSucceeded: Boolean indicating whether the target should
 *                               be launched when it has completed successfully.
 *          launcherPath: String containing the path of the custom executable to
 *                        use to launch the target of the download.
 *        }
 *
 * @return {Promise}
 * @resolves The Download object created as a consequence of controlling the
 *           download through the legacy nsITransfer interface.
 * @rejects Never.  The current test fails in case of exceptions.
 */
function promiseStartLegacyDownload(aSourceUrl, aOptions) {
  let sourceURI = NetUtil.newURI(aSourceUrl || httpUrl("source.txt"));
  let targetFile = (aOptions && aOptions.targetFile)
                   || getTempFile(TEST_TARGET_FILE_NAME);

  let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                  .createInstance(Ci.nsIWebBrowserPersist);
  if (aOptions) {
    aOptions.outPersist = persist;
  }

  let fileExtension = null, mimeInfo = null;
  let match = sourceURI.path.match(/\.([^.\/]+)$/);
  if (match) {
    fileExtension = match[1];
  }

  if (fileExtension) {
    try {
      mimeInfo = gMIMEService.getFromTypeAndExtension(null, fileExtension);
      mimeInfo.preferredAction = Ci.nsIMIMEInfo.saveToDisk;
    } catch (ex) { }
  }

  if (aOptions && aOptions.launcherPath) {
    do_check_true(mimeInfo != null);

    let localHandlerApp = Cc["@mozilla.org/uriloader/local-handler-app;1"]
                            .createInstance(Ci.nsILocalHandlerApp);
    localHandlerApp.executable = new FileUtils.File(aOptions.launcherPath);

    mimeInfo.preferredApplicationHandler = localHandlerApp;
    mimeInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;
  }

  if (aOptions && aOptions.launchWhenSucceeded) {
    do_check_true(mimeInfo != null);

    mimeInfo.preferredAction = Ci.nsIMIMEInfo.useHelperApp;
  }

  // Apply decoding if required by the "Content-Encoding" header.
  persist.persistFlags &= ~Ci.nsIWebBrowserPersist.PERSIST_FLAGS_NO_CONVERSION;
  persist.persistFlags |=
    Ci.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  let transfer = Cc["@mozilla.org/transfer;1"].createInstance(Ci.nsITransfer);

  let deferred = Promise.defer();

  Downloads.getList(Downloads.ALL).then(function (aList) {
    // Temporarily register a view that will get notified when the download we
    // are controlling becomes visible in the list of downloads.
    aList.addView({
      onDownloadAdded: function (aDownload) {
        aList.removeView(this).then(null, do_report_unexpected_exception);

        // Remove the download to keep the list empty for the next test.  This
        // also allows the caller to register the "onchange" event directly.
        let promise = aList.remove(aDownload);

        // When the download object is ready, make it available to the caller.
        promise.then(() => deferred.resolve(aDownload),
                     do_report_unexpected_exception);
      },
    }).then(null, do_report_unexpected_exception);

    let isPrivate = aOptions && aOptions.isPrivate;

    // Initialize the components so they reference each other.  This will cause
    // the Download object to be created and added to the public downloads.
    transfer.init(sourceURI, NetUtil.newURI(targetFile), null, mimeInfo, null,
                  null, persist, isPrivate);
    persist.progressListener = transfer;

    // Start the actual download process.
    persist.savePrivacyAwareURI(sourceURI, null, null, 0, null, null, targetFile,
                                isPrivate);
  }.bind(this)).then(null, do_report_unexpected_exception);

  return deferred.promise;
}

/**
 * Starts a new download using the nsIHelperAppService interface, and controls
 * it using the legacy nsITransfer interface.  The source of the download will
 * be "interruptible_resumable.txt" and partially downloaded data will be kept.
 *
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("interruptible_resumable.txt").
 *
 * @return {Promise}
 * @resolves The Download object created as a consequence of controlling the
 *           download through the legacy nsITransfer interface.
 * @rejects Never.  The current test fails in case of exceptions.
 */
function promiseStartExternalHelperAppServiceDownload(aSourceUrl) {
  let sourceURI = NetUtil.newURI(aSourceUrl ||
                                 httpUrl("interruptible_resumable.txt"));

  let deferred = Promise.defer();

  Downloads.getList(Downloads.PUBLIC).then(function (aList) {
    // Temporarily register a view that will get notified when the download we
    // are controlling becomes visible in the list of downloads.
    aList.addView({
      onDownloadAdded: function (aDownload) {
        aList.removeView(this).then(null, do_report_unexpected_exception);

        // Remove the download to keep the list empty for the next test.  This
        // also allows the caller to register the "onchange" event directly.
        let promise = aList.remove(aDownload);

        // When the download object is ready, make it available to the caller.
        promise.then(() => deferred.resolve(aDownload),
                     do_report_unexpected_exception);
      },
    }).then(null, do_report_unexpected_exception);

    let channel = NetUtil.newChannel({
      uri: sourceURI,
      loadUsingSystemPrincipal: true,
    });

    // Start the actual download process.
    channel.asyncOpen({
      contentListener: null,

      onStartRequest: function (aRequest, aContext)
      {
        let channel = aRequest.QueryInterface(Ci.nsIChannel);
        this.contentListener = gExternalHelperAppService.doContent(
                                     channel.contentType, aRequest, null, true);
        this.contentListener.onStartRequest(aRequest, aContext);
      },

      onStopRequest: function (aRequest, aContext, aStatusCode)
      {
        this.contentListener.onStopRequest(aRequest, aContext, aStatusCode);
      },

      onDataAvailable: function (aRequest, aContext, aInputStream, aOffset,
                                 aCount)
      {
        this.contentListener.onDataAvailable(aRequest, aContext, aInputStream,
                                             aOffset, aCount);
      },
    }, null);
  }.bind(this)).then(null, do_report_unexpected_exception);

  return deferred.promise;
}

/**
 * Waits for a download to reach half of its progress, in case it has not
 * reached the expected progress already.
 *
 * @param aDownload
 *        The Download object to wait upon.
 *
 * @return {Promise}
 * @resolves When the download has reached half of its progress.
 * @rejects Never.
 */
function promiseDownloadMidway(aDownload) {
  let deferred = Promise.defer();

  // Wait for the download to reach half of its progress.
  let onchange = function () {
    if (!aDownload.stopped && !aDownload.canceled && aDownload.progress == 50) {
      aDownload.onchange = null;
      deferred.resolve();
    }
  };

  // Register for the notification, but also call the function directly in
  // case the download already reached the expected progress.
  aDownload.onchange = onchange;
  onchange();

  return deferred.promise;
}

/**
 * Waits for a download to finish, in case it has not finished already.
 *
 * @param aDownload
 *        The Download object to wait upon.
 *
 * @return {Promise}
 * @resolves When the download has finished successfully.
 * @rejects JavaScript exception if the download failed.
 */
function promiseDownloadStopped(aDownload) {
  if (!aDownload.stopped) {
    // The download is in progress, wait for the current attempt to finish and
    // report any errors that may occur.
    return aDownload.start();
  }

  if (aDownload.succeeded) {
    return Promise.resolve();
  }

  // The download failed or was canceled.
  return Promise.reject(aDownload.error || new Error("Download canceled."));
}

/**
 * Returns a new public or private DownloadList object.
 *
 * @param aIsPrivate
 *        True for the private list, false or undefined for the public list.
 *
 * @return {Promise}
 * @resolves The newly created DownloadList object.
 * @rejects JavaScript exception.
 */
function promiseNewList(aIsPrivate)
{
  // We need to clear all the internal state for the list and summary objects,
  // since all the objects are interdependent internally.
  Downloads._promiseListsInitialized = null;
  Downloads._lists = {};
  Downloads._summaries = {};

  return Downloads.getList(aIsPrivate ? Downloads.PRIVATE : Downloads.PUBLIC);
}

/**
 * Ensures that the given file contents are equal to the given string.
 *
 * @param aPath
 *        String containing the path of the file whose contents should be
 *        verified.
 * @param aExpectedContents
 *        String containing the octets that are expected in the file.
 *
 * @return {Promise}
 * @resolves When the operation completes.
 * @rejects Never.
 */
function promiseVerifyContents(aPath, aExpectedContents)
{
  return Task.spawn(function() {
    let file = new FileUtils.File(aPath);

    if (!(yield OS.File.exists(aPath))) {
      do_throw("File does not exist: " + aPath);
    }

    if ((yield OS.File.stat(aPath)).size == 0) {
      do_throw("File is empty: " + aPath);
    }

    let deferred = Promise.defer();
    NetUtil.asyncFetch(
      file,
      function(aInputStream, aStatus) {
        do_check_true(Components.isSuccessCode(aStatus));
        let contents = NetUtil.readInputStreamToString(aInputStream,
                                                       aInputStream.available());
        if (contents.length > TEST_DATA_SHORT.length * 2 ||
            /[^\x20-\x7E]/.test(contents)) {
          // Do not print the entire content string to the test log.
          do_check_eq(contents.length, aExpectedContents.length);
          do_check_true(contents == aExpectedContents);
        } else {
          // Print the string if it is short and made of printable characters.
          do_check_eq(contents, aExpectedContents);
        }
        deferred.resolve();
      });

    yield deferred.promise;
  });
}

/**
 * Starts a socket listener that closes each incoming connection.
 *
 * @returns nsIServerSocket that listens for connections.  Call its "close"
 *          method to stop listening and free the server port.
 */
function startFakeServer()
{
  let serverSocket = new ServerSocket(-1, true, -1);
  serverSocket.asyncListen({
    onSocketAccepted: function (aServ, aTransport) {
      aTransport.close(Cr.NS_BINDING_ABORTED);
    },
    onStopListening: function () { },
  });
  return serverSocket;
}

/**
 * This is an internal reference that should not be used directly by tests.
 */
let _gDeferResponses = Promise.defer();

/**
 * Ensures that all the interruptible requests started after this function is
 * called won't complete until the continueResponses function is called.
 *
 * Normally, the internal HTTP server returns all the available data as soon as
 * a request is received.  In order for some requests to be served one part at a
 * time, special interruptible handlers are registered on the HTTP server.  This
 * allows testing events or actions that need to happen in the middle of a
 * download.
 *
 * For example, the handler accessible at the httpUri("interruptible.txt")
 * address returns the TEST_DATA_SHORT text, then it may block until the
 * continueResponses method is called.  At this point, the handler sends the
 * TEST_DATA_SHORT text again to complete the response.
 *
 * If an interruptible request is started before the function is called, it may
 * or may not be blocked depending on the actual sequence of events.
 */
function mustInterruptResponses()
{
  // If there are pending blocked requests, allow them to complete.  This is
  // done to prevent requests from being blocked forever, but should not affect
  // the test logic, since previously started requests should not be monitored
  // on the client side anymore.
  _gDeferResponses.resolve();

  do_print("Interruptible responses will be blocked midway.");
  _gDeferResponses = Promise.defer();
}

/**
 * Allows all the current and future interruptible requests to complete.
 */
function continueResponses()
{
  do_print("Interruptible responses are now allowed to continue.");
  _gDeferResponses.resolve();
}

/**
 * Registers an interruptible response handler.
 *
 * @param aPath
 *        Path passed to nsIHttpServer.registerPathHandler.
 * @param aFirstPartFn
 *        This function is called when the response is received, with the
 *        aRequest and aResponse arguments of the server.
 * @param aSecondPartFn
 *        This function is called with the aRequest and aResponse arguments of
 *        the server, when the continueResponses function is called.
 */
function registerInterruptibleHandler(aPath, aFirstPartFn, aSecondPartFn)
{
  gHttpServer.registerPathHandler(aPath, function (aRequest, aResponse) {
    do_print("Interruptible request started.");

    // Process the first part of the response.
    aResponse.processAsync();
    aFirstPartFn(aRequest, aResponse);

    // Wait on the current deferred object, then finish the request.
    _gDeferResponses.promise.then(function RIH_onSuccess() {
      aSecondPartFn(aRequest, aResponse);
      aResponse.finish();
      do_print("Interruptible request finished.");
    }).then(null, Cu.reportError);
  });
}

/**
 * Ensure the given date object is valid.
 *
 * @param aDate
 *        The date object to be checked. This value can be null.
 */
function isValidDate(aDate) {
  return aDate && aDate.getTime && !isNaN(aDate.getTime());
}

/**
 * Position of the first byte served by the "interruptible_resumable.txt"
 * handler during the most recent response.
 */
let gMostRecentFirstBytePos;

////////////////////////////////////////////////////////////////////////////////
//// Initialization functions common to all tests

add_task(function test_common_initialize()
{
  // Start the HTTP server.
  gHttpServer = new HttpServer();
  gHttpServer.registerDirectory("/", do_get_file("../data"));
  gHttpServer.start(-1);

  // Cache locks might prevent concurrent requests to the same resource, and
  // this may block tests that use the interruptible handlers.
  Services.prefs.setBoolPref("browser.cache.disk.enable", false);
  Services.prefs.setBoolPref("browser.cache.memory.enable", false);
  do_register_cleanup(function () {
    Services.prefs.clearUserPref("browser.cache.disk.enable");
    Services.prefs.clearUserPref("browser.cache.memory.enable");
  });

  registerInterruptibleHandler("/interruptible.txt",
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
      aResponse.setHeader("Content-Length", "" + (TEST_DATA_SHORT.length * 2),
                          false);
      aResponse.write(TEST_DATA_SHORT);
    }, function secondPart(aRequest, aResponse) {
      aResponse.write(TEST_DATA_SHORT);
    });

  registerInterruptibleHandler("/interruptible_resumable.txt",
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);

      // Determine if only part of the data should be sent.
      let data = TEST_DATA_SHORT + TEST_DATA_SHORT;
      if (aRequest.hasHeader("Range")) {
        var matches = aRequest.getHeader("Range")
                              .match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
        var firstBytePos = (matches[1] === undefined) ? 0 : matches[1];
        var lastBytePos = (matches[2] === undefined) ? data.length - 1
                                            : matches[2];
        if (firstBytePos >= data.length) {
          aResponse.setStatusLine(aRequest.httpVersion, 416,
                             "Requested Range Not Satisfiable");
          aResponse.setHeader("Content-Range", "*/" + data.length, false);
          aResponse.finish();
          return;
        }

        aResponse.setStatusLine(aRequest.httpVersion, 206, "Partial Content");
        aResponse.setHeader("Content-Range", firstBytePos + "-" +
                                             lastBytePos + "/" +
                                             data.length, false);

        data = data.substring(firstBytePos, lastBytePos + 1);

        gMostRecentFirstBytePos = firstBytePos;
      } else {
        gMostRecentFirstBytePos = 0;
      }

      aResponse.setHeader("Content-Length", "" + data.length, false);

      aResponse.write(data.substring(0, data.length / 2));

      // Store the second part of the data on the response object, so that it
      // can be used by the secondPart function.
      aResponse.secondPartData = data.substring(data.length / 2);
    }, function secondPart(aRequest, aResponse) {
      aResponse.write(aResponse.secondPartData);
    });

  registerInterruptibleHandler("/interruptible_gzip.txt",
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
      aResponse.setHeader("Content-Encoding", "gzip", false);
      aResponse.setHeader("Content-Length", "" + TEST_DATA_SHORT_GZIP_ENCODED.length);

      let bos =  new BinaryOutputStream(aResponse.bodyOutputStream);
      bos.writeByteArray(TEST_DATA_SHORT_GZIP_ENCODED_FIRST,
                         TEST_DATA_SHORT_GZIP_ENCODED_FIRST.length);
    }, function secondPart(aRequest, aResponse) {
      let bos =  new BinaryOutputStream(aResponse.bodyOutputStream);
      bos.writeByteArray(TEST_DATA_SHORT_GZIP_ENCODED_SECOND,
                         TEST_DATA_SHORT_GZIP_ENCODED_SECOND.length);
    });

  gHttpServer.registerPathHandler("/shorter-than-content-length-http-1-1.txt",
    function (aRequest, aResponse) {
      aResponse.processAsync();
      aResponse.setStatusLine("1.1", 200, "OK");
      aResponse.setHeader("Content-Type", "text/plain", false);
      aResponse.setHeader("Content-Length", "" + (TEST_DATA_SHORT.length * 2),
                          false);
      aResponse.write(TEST_DATA_SHORT);
      aResponse.finish();
    });

  // This URL will emulate being blocked by Windows Parental controls
  gHttpServer.registerPathHandler("/parentalblocked.zip",
    function (aRequest, aResponse) {
      aResponse.setStatusLine(aRequest.httpVersion, 450,
                              "Blocked by Windows Parental Controls");
    });

  // Disable integration with the host application requiring profile access.
  DownloadIntegration.dontLoadList = true;
  DownloadIntegration.dontLoadObservers = true;
  // Disable the parental controls checking.
  DownloadIntegration.dontCheckParentalControls = true;
  // Disable application reputation checks.
  DownloadIntegration.dontCheckApplicationReputation = true;
  // Disable the calls to the OS to launch files and open containing folders
  DownloadIntegration.dontOpenFileAndFolder = true;
  DownloadIntegration._deferTestOpenFile = Promise.defer();
  DownloadIntegration._deferTestShowDir = Promise.defer();

  // Avoid leaking uncaught promise errors
  DownloadIntegration._deferTestOpenFile.promise.then(null, () => undefined);
  DownloadIntegration._deferTestShowDir.promise.then(null, () => undefined);

  // Get a reference to nsIComponentRegistrar, and ensure that is is freed
  // before the XPCOM shutdown.
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  do_register_cleanup(() => registrar = null);

  // Make sure that downloads started using nsIExternalHelperAppService are
  // saved to disk without asking for a destination interactively.
  let mock = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIHelperAppLauncherDialog]),
    promptForSaveToFileAsync(aLauncher,
                             aWindowContext,
                             aDefaultFileName,
                             aSuggestedFileExtension,
                             aForcePrompt) {
      // The dialog should create the empty placeholder file.
      let file = getTempFile(TEST_TARGET_FILE_NAME);
      file.create(Ci.nsIFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
      aLauncher.saveDestinationAvailable(file);
    },
  };

  let cid = MockRegistrar.register("@mozilla.org/helperapplauncherdialog;1", mock);
  do_register_cleanup(() => {
    MockRegistrar.unregister(cid);
  });
});

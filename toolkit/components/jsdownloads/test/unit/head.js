/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

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
// Generate using gzipCompressString in TelemetryPing.js.
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
      mimeInfo = gMIMEService.getFromTypeAndExtension(null,
                                                      fileExtension);
    } catch (ex) { }
  }

  // Apply decoding if required by the "Content-Encoding" header.
  persist.persistFlags &= ~Ci.nsIWebBrowserPersist.PERSIST_FLAGS_NO_CONVERSION;

  // We must create the nsITransfer implementation using its class ID because
  // the "@mozilla.org/transfer;1" contract is currently implemented in
  // "toolkit/components/downloads".  When the other folder is not included in
  // builds anymore (bug 851471), we'll be able to use the contract ID.
  let transfer =
      Components.classesByID["{1b4c85df-cbdd-4bb6-b04e-613caece083c}"]
                .createInstance(Ci.nsITransfer);

  let deferred = Promise.defer();

  let isPrivate = aOptions && aOptions.isPrivate;
  let promise = isPrivate ? Downloads.getPrivateDownloadList()
                          : Downloads.getPublicDownloadList();
  promise.then(function (aList) {
    // Temporarily register a view that will get notified when the download we
    // are controlling becomes visible in the list of downloads.
    aList.addView({
      onDownloadAdded: function (aDownload) {
        aList.removeView(this);

        // Remove the download to keep the list empty for the next test.  This
        // also allows the caller to register the "onchange" event directly.
        aList.remove(aDownload);

        // When the download object is ready, make it available to the caller.
        deferred.resolve(aDownload);
      },
    });

    // Initialize the components so they reference each other.  This will cause
    // the Download object to be created and added to the public downloads.
    transfer.init(sourceURI, NetUtil.newURI(targetFile), null, mimeInfo, null, null,
                  persist, isPrivate);
    persist.progressListener = transfer;

    // Start the actual download process.
    persist.savePrivacyAwareURI(sourceURI, null, null, null, null, targetFile,
                                isPrivate);
  }.bind(this)).then(null, do_report_unexpected_exception);

  return deferred.promise;
}

/**
 * Returns a new public DownloadList object.
 *
 * @return {Promise}
 * @resolves The newly created DownloadList object.
 * @rejects JavaScript exception.
 */
function promiseNewDownloadList() {
  // Force the creation of a new public download list.
  Downloads._promisePublicDownloadList = null;
  return Downloads.getPublicDownloadList();
}

/**
 * Returns a new private DownloadList object.
 *
 * @return {Promise}
 * @resolves The newly created DownloadList object.
 * @rejects JavaScript exception.
 */
function promiseNewPrivateDownloadList() {
  // Force the creation of a new public download list.
  Downloads._privateDownloadList = null;
  return Downloads.getPrivateDownloadList();
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
  let deferred = Promise.defer();
  let file = new FileUtils.File(aPath);
  NetUtil.asyncFetch(file, function(aInputStream, aStatus) {
    do_check_true(Components.isSuccessCode(aStatus));
    let contents = NetUtil.readInputStreamToString(aInputStream,
                                                   aInputStream.available());
    if (contents.length <= TEST_DATA_SHORT.length * 2) {
      do_check_eq(contents, aExpectedContents);
    } else {
      // Do not print the entire content string to the test log.
      do_check_eq(contents.length, aExpectedContents.length);
      do_check_true(contents == aExpectedContents);
    }
    deferred.resolve();
  });
  return deferred.promise;
}

/**
 * Adds entry for download.
 *
 * @param aSourceUrl
 *        String containing the URI for the download source, or null to use
 *        httpUrl("source.txt").
 *
 * @return {Promise}
 * @rejects JavaScript exception.
 */
function promiseAddDownloadToHistory(aSourceUrl) {
  let deferred = Promise.defer();
  PlacesUtils.asyncHistory.updatePlaces(
    {
      uri: NetUtil.newURI(aSourceUrl || httpUrl("source.txt")),
      visits: [{
        transitionType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
        visitDate:  Date.now()
      }]
    },
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
    });
  return deferred.promise;
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
 * This function allows testing events or actions that need to happen in the
 * middle of a download.
 *
 * Normally, the internal HTTP server returns all the available data as soon as
 * a request is received.  In order for some requests to be served one part at a
 * time, special interruptible handlers are registered on the HTTP server.
 *
 * Before making a request to one of the addresses served by the interruptible
 * handlers, you may call "deferNextResponse" to get a reference to an object
 * that allows you to control the next request.
 *
 * For example, the handler accessible at the httpUri("interruptible.txt")
 * address returns the TEST_DATA_SHORT text, then waits until the "resolve"
 * method is called on the object returned by the function.  At this point, the
 * handler sends the TEST_DATA_SHORT text again to complete the response.
 *
 * You can also call the "reject" method on the returned object to interrupt the
 * response midway.  Because of how the network layer is implemented, this does
 * not cause the socket to return an error.
 *
 * @returns Deferred object used to control the response.
 */
function deferNextResponse()
{
  do_print("Interruptible request will be controlled.");

  // Store an internal reference that should not be used directly by tests.
  if (!deferNextResponse._deferred) {
    deferNextResponse._deferred = Promise.defer();
  }
  return deferNextResponse._deferred;
}

/**
 * Returns a promise that is resolved when the next interruptible response
 * handler has received the request, and has started sending the first part of
 * the response.  The response might not have been received by the client yet.
 *
 * @return {Promise}
 * @resolves When the next request has been received.
 * @rejects Never.
 */
function promiseNextRequestReceived()
{
  do_print("Requested notification when interruptible request is received.");

  // Store an internal reference that should not be used directly by tests.
  promiseNextRequestReceived._deferred = Promise.defer();
  return promiseNextRequestReceived._deferred.promise;
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
 *        This function is called after the "resolve" method of the object
 *        returned by deferNextResponse is called.  This function is called with
 *        the aRequest and aResponse arguments of the server.
 */
function registerInterruptibleHandler(aPath, aFirstPartFn, aSecondPartFn)
{
  gHttpServer.registerPathHandler(aPath, function (aRequest, aResponse) {
    // Get a reference to the controlling object for this request.  If the
    // deferNextResponse function was not called, interrupt the test.
    let deferResponse = deferNextResponse._deferred;
    deferNextResponse._deferred = null;
    if (deferResponse) {
      do_print("Interruptible request started under control.");
    } else {
      do_print("Interruptible request started without being controlled.");
      deferResponse = Promise.defer();
      deferResponse.resolve();
    }

    // Process the first part of the response.
    aResponse.processAsync();
    aFirstPartFn(aRequest, aResponse);

    if (promiseNextRequestReceived._deferred) {
      do_print("Notifying that interruptible request has been received.");
      promiseNextRequestReceived._deferred.resolve();
      promiseNextRequestReceived._deferred = null;
    }

    // Wait on the deferred object, then finish or abort the request.
    deferResponse.promise.then(function RIH_onSuccess() {
      aSecondPartFn(aRequest, aResponse);
      aResponse.finish();
      do_print("Interruptible request finished.");
    }, function RIH_onFailure() {
      aResponse.abort();
      do_print("Interruptible request aborted.");
    });
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

////////////////////////////////////////////////////////////////////////////////
//// Initialization functions common to all tests

add_task(function test_common_initialize()
{
  // Start the HTTP server.
  gHttpServer = new HttpServer();
  gHttpServer.registerDirectory("/", do_get_file("../data"));
  gHttpServer.start(-1);

  registerInterruptibleHandler("/interruptible.txt",
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
      aResponse.setHeader("Content-Length", "" + (TEST_DATA_SHORT.length * 2),
                          false);
      aResponse.write(TEST_DATA_SHORT);
    }, function secondPart(aRequest, aResponse) {
      aResponse.write(TEST_DATA_SHORT);
    });

  registerInterruptibleHandler("/empty-noprogress.txt",
    function firstPart(aRequest, aResponse) {
      aResponse.setHeader("Content-Type", "text/plain", false);
    }, function secondPart(aRequest, aResponse) { });


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

  // Disable integration with the host application requiring profile access.
  DownloadIntegration.dontLoad = true;
  // Disable the parental controls checking.
  DownloadIntegration.dontCheckParentalControls = true;
  // Disable the calls to the OS to launch files and open containing folders
  DownloadIntegration.dontOpenFileAndFolder = true;
  DownloadIntegration._deferTestOpenFile = Promise.defer();
  DownloadIntegration._deferTestShowDir = Promise.defer();

});

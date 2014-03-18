/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests signature extraction using Windows Authenticode APIs of
 * downloaded files.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

const BackgroundFileSaverOutputStream = Components.Constructor(
      "@mozilla.org/network/background-file-saver;1?mode=outputstream",
      "nsIBackgroundFileSaver");

const StringInputStream = Components.Constructor(
      "@mozilla.org/io/string-input-stream;1",
      "nsIStringInputStream",
      "setData");

const TEST_FILE_NAME_1 = "test-backgroundfilesaver-1.txt";

/**
 * Returns a reference to a temporary file.  If the file is then created, it
 * will be removed when tests in this file finish.
 */
function getTempFile(aLeafName) {
  let file = FileUtils.getFile("TmpD", [aLeafName]);
  do_register_cleanup(function GTF_cleanup() {
    if (file.exists()) {
      file.remove(false);
    }
  });
  return file;
}

/**
 * Waits for the given saver object to complete.
 *
 * @param aSaver
 *        The saver, with the output stream or a stream listener implementation.
 * @param aOnTargetChangeFn
 *        Optional callback invoked with the target file name when it changes.
 *
 * @return {Promise}
 * @resolves When onSaveComplete is called with a success code.
 * @rejects With an exception, if onSaveComplete is called with a failure code.
 */
function promiseSaverComplete(aSaver, aOnTargetChangeFn) {
  let deferred = Promise.defer();
  aSaver.observer = {
    onTargetChange: function BFSO_onSaveComplete(aSaver, aTarget)
    {
      if (aOnTargetChangeFn) {
        aOnTargetChangeFn(aTarget);
      }
    },
    onSaveComplete: function BFSO_onSaveComplete(aSaver, aStatus)
    {
      if (Components.isSuccessCode(aStatus)) {
        deferred.resolve();
      } else {
        deferred.reject(new Components.Exception("Saver failed.", aStatus));
      }
    },
  };
  return deferred.promise;
}

/**
 * Feeds a string to a BackgroundFileSaverOutputStream.
 *
 * @param aSourceString
 *        The source data to copy.
 * @param aSaverOutputStream
 *        The BackgroundFileSaverOutputStream to feed.
 * @param aCloseWhenDone
 *        If true, the output stream will be closed when the copy finishes.
 *
 * @return {Promise}
 * @resolves When the copy completes with a success code.
 * @rejects With an exception, if the copy fails.
 */
function promiseCopyToSaver(aSourceString, aSaverOutputStream, aCloseWhenDone) {
  let deferred = Promise.defer();
  let inputStream = new StringInputStream(aSourceString, aSourceString.length);
  let copier = Cc["@mozilla.org/network/async-stream-copier;1"]
               .createInstance(Ci.nsIAsyncStreamCopier);
  copier.init(inputStream, aSaverOutputStream, null, false, true, 0x8000, true,
              aCloseWhenDone);
  copier.asyncCopy({
    onStartRequest: function () { },
    onStopRequest: function (aRequest, aContext, aStatusCode)
    {
      if (Components.isSuccessCode(aStatusCode)) {
        deferred.resolve();
      } else {
        deferred.reject(new Components.Exception(aResult));
      }
    },
  }, null);
  return deferred.promise;
}

let gStillRunning = true;

////////////////////////////////////////////////////////////////////////////////
//// Tests

function run_test()
{
  run_next_test();
}

add_task(function test_setup()
{
  // Wait 10 minutes, that is half of the external xpcshell timeout.
  do_timeout(10 * 60 * 1000, function() {
    if (gStillRunning) {
      do_throw("Test timed out.");
    }
  })
});

function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(Ci.nsIFileInputStream);
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

add_task(function test_signature()
{
  // Check that we get a signature if the saver is finished on Windows.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  let data = readFileToString("data/signed_win.exe");
  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  try {
    let signatureInfo = saver.signatureInfo;
    do_throw("Can't get signature before saver is complete.");
  } catch (ex if ex.result == Cr.NS_ERROR_NOT_AVAILABLE) { }

  saver.enableSignatureInfo();
  saver.setTarget(destFile, false);
  yield promiseCopyToSaver(data, saver, true);

  saver.finish(Cr.NS_OK);
  yield completionPromise;

  // There's only one nsIX509CertList in the signature array.
  do_check_eq(1, saver.signatureInfo.length);
  let certLists = saver.signatureInfo.enumerate();
  do_check_true(certLists.hasMoreElements());
  let certList = certLists.getNext().QueryInterface(Ci.nsIX509CertList);
  do_check_false(certLists.hasMoreElements());

  // Check that it has 3 certs.
  let certs = certList.getEnumerator();
  do_check_true(certs.hasMoreElements());
  let signer = certs.getNext().QueryInterface(Ci.nsIX509Cert);
  do_check_true(certs.hasMoreElements());
  let issuer = certs.getNext().QueryInterface(Ci.nsIX509Cert);
  do_check_true(certs.hasMoreElements());
  let root = certs.getNext().QueryInterface(Ci.nsIX509Cert);
  do_check_false(certs.hasMoreElements());

  // Check that the certs have expected strings attached.
  let organization = "Microsoft Corporation";
  do_check_eq("Microsoft Corporation", signer.commonName);
  do_check_eq(organization, signer.organization);
  do_check_eq("Copyright (c) 2002 Microsoft Corp.", signer.organizationalUnit);

  do_check_eq("Microsoft Code Signing PCA", issuer.commonName);
  do_check_eq(organization, issuer.organization);
  do_check_eq("Copyright (c) 2000 Microsoft Corp.", issuer.organizationalUnit);

  do_check_eq("Microsoft Root Authority", root.commonName);
  do_check_false(root.organization);
  do_check_eq("Copyright (c) 1997 Microsoft Corp.", root.organizationalUnit);

  // Clean up.
  destFile.remove(false);
});

add_task(function test_teardown()
{
  gStillRunning = false;
});

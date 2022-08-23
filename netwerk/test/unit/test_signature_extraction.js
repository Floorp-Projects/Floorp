/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests signature extraction using Windows Authenticode APIs of
 * downloaded files.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "NetUtil",
  "resource://gre/modules/NetUtil.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileTestUtils",
  "resource://testing-common/FileTestUtils.jsm"
);

const BackgroundFileSaverOutputStream = Components.Constructor(
  "@mozilla.org/network/background-file-saver;1?mode=outputstream",
  "nsIBackgroundFileSaver"
);

const StringInputStream = Components.Constructor(
  "@mozilla.org/io/string-input-stream;1",
  "nsIStringInputStream",
  "setData"
);

const TEST_FILE_NAME_1 = "test-backgroundfilesaver-1.txt";

/**
 * Returns a reference to a temporary file that is guaranteed not to exist and
 * is cleaned up later. See FileTestUtils.getTempFile for details.
 */
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
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
  return new Promise((resolve, reject) => {
    aSaver.observer = {
      onTargetChange: function BFSO_onSaveComplete(aSaver, aTarget) {
        if (aOnTargetChangeFn) {
          aOnTargetChangeFn(aTarget);
        }
      },
      onSaveComplete: function BFSO_onSaveComplete(aSaver, aStatus) {
        if (Components.isSuccessCode(aStatus)) {
          resolve();
        } else {
          reject(new Components.Exception("Saver failed.", aStatus));
        }
      },
    };
  });
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
  return new Promise((resolve, reject) => {
    let inputStream = new StringInputStream(
      aSourceString,
      aSourceString.length
    );
    let copier = Cc[
      "@mozilla.org/network/async-stream-copier;1"
    ].createInstance(Ci.nsIAsyncStreamCopier);
    copier.init(
      inputStream,
      aSaverOutputStream,
      null,
      false,
      true,
      0x8000,
      true,
      aCloseWhenDone
    );
    copier.asyncCopy(
      {
        onStartRequest() {},
        onStopRequest(aRequest, aContext, aStatusCode) {
          if (Components.isSuccessCode(aStatusCode)) {
            resolve();
          } else {
            reject(new Components.Exception(aStatusCode));
          }
        },
      },
      null
    );
  });
}

var gStillRunning = true;

////////////////////////////////////////////////////////////////////////////////
//// Tests

add_task(function test_setup() {
  // Wait 10 minutes, that is half of the external xpcshell timeout.
  do_timeout(10 * 60 * 1000, function() {
    if (gStillRunning) {
      do_throw("Test timed out.");
    }
  });
});

function readFileToString(aFilename) {
  let f = do_get_file(aFilename);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(f, -1, 0, 0);
  let buf = NetUtil.readInputStreamToString(stream, stream.available());
  return buf;
}

add_task(async function test_signature() {
  // Check that we get a signature if the saver is finished on Windows.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  let data = readFileToString("data/signed_win.exe");
  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  try {
    saver.signatureInfo;
    do_throw("Can't get signature before saver is complete.");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw ex;
    }
  }

  saver.enableSignatureInfo();
  saver.setTarget(destFile, false);
  await promiseCopyToSaver(data, saver, true);

  saver.finish(Cr.NS_OK);
  await completionPromise;

  // There's only one Array of certs(raw bytes) in the signature array.
  Assert.equal(1, saver.signatureInfo.length);
  let certLists = saver.signatureInfo;
  Assert.ok(certLists.length === 1);

  // Check that it has 3 certs(raw bytes).
  let certs = certLists[0];
  Assert.ok(certs.length === 3);

  const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let signer = certDB.constructX509(certs[0]);
  let issuer = certDB.constructX509(certs[1]);
  let root = certDB.constructX509(certs[2]);

  let organization = "Microsoft Corporation";
  Assert.equal("Microsoft Corporation", signer.commonName);
  Assert.equal(organization, signer.organization);
  Assert.equal("Copyright (c) 2002 Microsoft Corp.", signer.organizationalUnit);

  Assert.equal("Microsoft Code Signing PCA", issuer.commonName);
  Assert.equal(organization, issuer.organization);
  Assert.equal("Copyright (c) 2000 Microsoft Corp.", issuer.organizationalUnit);

  Assert.equal("Microsoft Root Authority", root.commonName);
  Assert.ok(!root.organization);
  Assert.equal("Copyright (c) 1997 Microsoft Corp.", root.organizationalUnit);

  // Clean up.
  destFile.remove(false);
});

add_task(function test_teardown() {
  gStillRunning = false;
});

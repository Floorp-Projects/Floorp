/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests components that implement nsIBackgroundFileSaver.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
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

const BackgroundFileSaverStreamListener = Components.Constructor(
  "@mozilla.org/network/background-file-saver;1?mode=streamlistener",
  "nsIBackgroundFileSaver"
);

const StringInputStream = Components.Constructor(
  "@mozilla.org/io/string-input-stream;1",
  "nsIStringInputStream",
  "setData"
);

const REQUEST_SUSPEND_AT = 1024 * 1024 * 4;
const TEST_DATA_SHORT = "This test string is written to the file.";
const TEST_FILE_NAME_1 = "test-backgroundfilesaver-1.txt";
const TEST_FILE_NAME_2 = "test-backgroundfilesaver-2.txt";
const TEST_FILE_NAME_3 = "test-backgroundfilesaver-3.txt";

// A map of test data length to the expected SHA-256 hashes
const EXPECTED_HASHES = {
  // No data
  0: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
  // TEST_DATA_SHORT
  40: "f37176b690e8744ee990a206c086cba54d1502aa2456c3b0c84ef6345d72a192",
  // TEST_DATA_SHORT + TEST_DATA_SHORT
  80: "780c0e91f50bb7ec922cc11e16859e6d5df283c0d9470f61772e3d79f41eeb58",
  // TEST_DATA_LONG
  4718592: "372cb9e5ce7b76d3e2a5042e78aa72dcf973e659a262c61b7ff51df74b36767b",
  // TEST_DATA_LONG + TEST_DATA_LONG
  9437184: "693e4f8c6855a6fed4f5f9370d12cc53105672f3ff69783581e7d925984c41d3",
};

const gTextDecoder = new TextDecoder();

// Generate a long string of data in a moderately fast way.
const TEST_256_CHARS = new Array(257).join("-");
const DESIRED_LENGTH = REQUEST_SUSPEND_AT * 1.125;
const TEST_DATA_LONG = new Array(1 + DESIRED_LENGTH / 256).join(TEST_256_CHARS);
Assert.equal(TEST_DATA_LONG.length, DESIRED_LENGTH);

/**
 * Returns a reference to a temporary file that is guaranteed not to exist and
 * is cleaned up later. See FileTestUtils.getTempFile for details.
 */
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
}

/**
 * Helper function for converting a binary blob to its hex equivalent.
 *
 * @param str
 *        String possibly containing non-printable chars.
 * @return A hex-encoded string.
 */
function toHex(str) {
  var hex = "";
  for (var i = 0; i < str.length; i++) {
    hex += ("0" + str.charCodeAt(i).toString(16)).slice(-2);
  }
  return hex;
}

/**
 * Ensures that the given file contents are equal to the given string.
 *
 * @param aFile
 *        nsIFile whose contents should be verified.
 * @param aExpectedContents
 *        String containing the octets that are expected in the file.
 *
 * @return {Promise}
 * @resolves When the operation completes.
 * @rejects Never.
 */
function promiseVerifyContents(aFile, aExpectedContents) {
  return new Promise(resolve => {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(aFile),
        loadUsingSystemPrincipal: true,
      },
      function(aInputStream, aStatus) {
        Assert.ok(Components.isSuccessCode(aStatus));
        let contents = NetUtil.readInputStreamToString(
          aInputStream,
          aInputStream.available()
        );
        if (contents.length <= TEST_DATA_SHORT.length * 2) {
          Assert.equal(contents, aExpectedContents);
        } else {
          // Do not print the entire content string to the test log.
          Assert.equal(contents.length, aExpectedContents.length);
          Assert.ok(contents == aExpectedContents);
        }
        resolve();
      }
    );
  });
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
        onStopRequest(aRequest, aStatusCode) {
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

/**
 * Feeds a string to a BackgroundFileSaverStreamListener.
 *
 * @param aSourceString
 *        The source data to copy.
 * @param aSaverStreamListener
 *        The BackgroundFileSaverStreamListener to feed.
 * @param aCloseWhenDone
 *        If true, the output stream will be closed when the copy finishes.
 *
 * @return {Promise}
 * @resolves When the operation completes with a success code.
 * @rejects With an exception, if the operation fails.
 */
function promisePumpToSaver(
  aSourceString,
  aSaverStreamListener,
  aCloseWhenDone
) {
  return new Promise((resolve, reject) => {
    aSaverStreamListener.QueryInterface(Ci.nsIStreamListener);
    let inputStream = new StringInputStream(
      aSourceString,
      aSourceString.length
    );
    let pump = Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(
      Ci.nsIInputStreamPump
    );
    pump.init(inputStream, 0, 0, true);
    pump.asyncRead({
      onStartRequest: function PPTS_onStartRequest(aRequest) {
        aSaverStreamListener.onStartRequest(aRequest);
      },
      onStopRequest: function PPTS_onStopRequest(aRequest, aStatusCode) {
        aSaverStreamListener.onStopRequest(aRequest, aStatusCode);
        if (Components.isSuccessCode(aStatusCode)) {
          resolve();
        } else {
          reject(new Components.Exception(aStatusCode));
        }
      },
      onDataAvailable: function PPTS_onDataAvailable(
        aRequest,
        aInputStream,
        aOffset,
        aCount
      ) {
        aSaverStreamListener.onDataAvailable(
          aRequest,
          aInputStream,
          aOffset,
          aCount
        );
      },
    });
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

add_task(async function test_normal() {
  // This test demonstrates the most basic use case.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  // Create the object implementing the output stream.
  let saver = new BackgroundFileSaverOutputStream();

  // Set up callbacks for completion and target file name change.
  let receivedOnTargetChange = false;
  function onTargetChange(aTarget) {
    Assert.ok(destFile.equals(aTarget));
    receivedOnTargetChange = true;
  }
  let completionPromise = promiseSaverComplete(saver, onTargetChange);

  // Set the target file.
  saver.setTarget(destFile, false);

  // Write some data and close the output stream.
  await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);

  // Indicate that we are ready to finish, and wait for a successful callback.
  saver.finish(Cr.NS_OK);
  await completionPromise;

  // Only after we receive the completion notification, we can also be sure that
  // we've received the target file name change notification before it.
  Assert.ok(receivedOnTargetChange);

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_combinations() {
  let initialFile = getTempFile(TEST_FILE_NAME_1);
  let renamedFile = getTempFile(TEST_FILE_NAME_2);

  // Keep track of the current file.
  let currentFile = null;
  function onTargetChange(aTarget) {
    currentFile = null;
    info("Target file changed to: " + aTarget.leafName);
    currentFile = aTarget;
  }

  // Tests various combinations of events and behaviors for both the stream
  // listener and the output stream implementations.
  for (let testFlags = 0; testFlags < 32; testFlags++) {
    let keepPartialOnFailure = !!(testFlags & 1);
    let renameAtSomePoint = !!(testFlags & 2);
    let cancelAtSomePoint = !!(testFlags & 4);
    let useStreamListener = !!(testFlags & 8);
    let useLongData = !!(testFlags & 16);

    let startTime = Date.now();
    info(
      "Starting keepPartialOnFailure = " +
        keepPartialOnFailure +
        ", renameAtSomePoint = " +
        renameAtSomePoint +
        ", cancelAtSomePoint = " +
        cancelAtSomePoint +
        ", useStreamListener = " +
        useStreamListener +
        ", useLongData = " +
        useLongData
    );

    // Create the object and register the observers.
    currentFile = null;
    let saver = useStreamListener
      ? new BackgroundFileSaverStreamListener()
      : new BackgroundFileSaverOutputStream();
    saver.enableSha256();
    let completionPromise = promiseSaverComplete(saver, onTargetChange);

    // Start feeding the first chunk of data to the saver.  In case we are using
    // the stream listener, we only write one chunk.
    let testData = useLongData ? TEST_DATA_LONG : TEST_DATA_SHORT;
    let feedPromise = useStreamListener
      ? promisePumpToSaver(testData + testData, saver)
      : promiseCopyToSaver(testData, saver, false);

    // Set a target output file.
    saver.setTarget(initialFile, keepPartialOnFailure);

    // Wait for the first chunk of data to be copied.
    await feedPromise;

    if (renameAtSomePoint) {
      saver.setTarget(renamedFile, keepPartialOnFailure);
    }

    if (cancelAtSomePoint) {
      saver.finish(Cr.NS_ERROR_FAILURE);
    }

    // Feed the second chunk of data to the saver.
    if (!useStreamListener) {
      await promiseCopyToSaver(testData, saver, true);
    }

    // Wait for completion, and ensure we succeeded or failed as expected.
    if (!cancelAtSomePoint) {
      saver.finish(Cr.NS_OK);
    }
    try {
      await completionPromise;
      if (cancelAtSomePoint) {
        do_throw("Failure expected.");
      }
    } catch (ex) {
      if (!cancelAtSomePoint || ex.result != Cr.NS_ERROR_FAILURE) {
        throw ex;
      }
    }

    if (!cancelAtSomePoint) {
      // In this case, the file must exist.
      Assert.ok(currentFile.exists());
      let expectedContents = testData + testData;
      await promiseVerifyContents(currentFile, expectedContents);
      Assert.equal(
        EXPECTED_HASHES[expectedContents.length],
        toHex(saver.sha256Hash)
      );
      currentFile.remove(false);

      // If the target was really renamed, the old file should not exist.
      if (renamedFile.equals(currentFile)) {
        Assert.ok(!initialFile.exists());
      }
    } else if (!keepPartialOnFailure) {
      // In this case, the file must not exist.
      Assert.ok(!initialFile.exists());
      Assert.ok(!renamedFile.exists());
    } else {
      // In this case, the file may or may not exist, because canceling can
      // interrupt the asynchronous operation at any point, even before the file
      // has been created for the first time.
      if (initialFile.exists()) {
        initialFile.remove(false);
      }
      if (renamedFile.exists()) {
        renamedFile.remove(false);
      }
    }

    info("Test case completed in " + (Date.now() - startTime) + " ms.");
  }
});

add_task(async function test_setTarget_after_close_stream() {
  // This test checks the case where we close the output stream before we call
  // the setTarget method.  All the data should be buffered and written anyway.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  // Test the case where the file does not already exists first, then the case
  // where the file already exists.
  for (let i = 0; i < 2; i++) {
    let saver = new BackgroundFileSaverOutputStream();
    saver.enableSha256();
    let completionPromise = promiseSaverComplete(saver);

    // Copy some data to the output stream of the file saver.  This data must
    // be shorter than the internal component's pipe buffer for the test to
    // succeed, because otherwise the test would block waiting for the write to
    // complete.
    await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);

    // Set the target file and wait for the output to finish.
    saver.setTarget(destFile, false);
    saver.finish(Cr.NS_OK);
    await completionPromise;

    // Verify results.
    await promiseVerifyContents(destFile, TEST_DATA_SHORT);
    Assert.equal(
      EXPECTED_HASHES[TEST_DATA_SHORT.length],
      toHex(saver.sha256Hash)
    );
  }

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_setTarget_fast() {
  // This test checks a fast rename of the target file.
  let destFile1 = getTempFile(TEST_FILE_NAME_1);
  let destFile2 = getTempFile(TEST_FILE_NAME_2);
  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  // Set the initial name after the stream is closed, then rename immediately.
  await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);
  saver.setTarget(destFile1, false);
  saver.setTarget(destFile2, false);

  // Wait for all the operations to complete.
  saver.finish(Cr.NS_OK);
  await completionPromise;

  // Verify results and clean up.
  Assert.ok(!destFile1.exists());
  await promiseVerifyContents(destFile2, TEST_DATA_SHORT);
  destFile2.remove(false);
});

add_task(async function test_setTarget_multiple() {
  // This test checks multiple renames of the target file.
  let destFile = getTempFile(TEST_FILE_NAME_1);
  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  // Rename both before and after the stream is closed.
  saver.setTarget(getTempFile(TEST_FILE_NAME_2), false);
  saver.setTarget(getTempFile(TEST_FILE_NAME_3), false);
  await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);
  saver.setTarget(getTempFile(TEST_FILE_NAME_2), false);
  saver.setTarget(destFile, false);

  // Wait for all the operations to complete.
  saver.finish(Cr.NS_OK);
  await completionPromise;

  // Verify results and clean up.
  Assert.ok(!getTempFile(TEST_FILE_NAME_2).exists());
  Assert.ok(!getTempFile(TEST_FILE_NAME_3).exists());
  await promiseVerifyContents(destFile, TEST_DATA_SHORT);
  destFile.remove(false);
});

add_task(async function test_enableAppend() {
  // This test checks append mode with hashing disabled.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  // Test the case where the file does not already exists first, then the case
  // where the file already exists.
  for (let i = 0; i < 2; i++) {
    let saver = new BackgroundFileSaverOutputStream();
    saver.enableAppend();
    let completionPromise = promiseSaverComplete(saver);

    saver.setTarget(destFile, false);
    await promiseCopyToSaver(TEST_DATA_LONG, saver, true);

    saver.finish(Cr.NS_OK);
    await completionPromise;

    // Verify results.
    let expectedContents =
      i == 0 ? TEST_DATA_LONG : TEST_DATA_LONG + TEST_DATA_LONG;
    await promiseVerifyContents(destFile, expectedContents);
  }

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_enableAppend_setTarget_fast() {
  // This test checks a fast rename of the target file in append mode.
  let destFile1 = getTempFile(TEST_FILE_NAME_1);
  let destFile2 = getTempFile(TEST_FILE_NAME_2);

  // Test the case where the file does not already exists first, then the case
  // where the file already exists.
  for (let i = 0; i < 2; i++) {
    let saver = new BackgroundFileSaverOutputStream();
    saver.enableAppend();
    let completionPromise = promiseSaverComplete(saver);

    await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);

    // The first time, we start appending to the first file and rename to the
    // second file.  The second time, we start appending to the second file,
    // that was created the first time, and rename back to the first file.
    let firstFile = i == 0 ? destFile1 : destFile2;
    let secondFile = i == 0 ? destFile2 : destFile1;
    saver.setTarget(firstFile, false);
    saver.setTarget(secondFile, false);

    saver.finish(Cr.NS_OK);
    await completionPromise;

    // Verify results.
    Assert.ok(!firstFile.exists());
    let expectedContents =
      i == 0 ? TEST_DATA_SHORT : TEST_DATA_SHORT + TEST_DATA_SHORT;
    await promiseVerifyContents(secondFile, expectedContents);
  }

  // Clean up.
  destFile1.remove(false);
});

add_task(async function test_enableAppend_hash() {
  // This test checks append mode, also verifying that the computed hash
  // includes the contents of the existing data.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  // Test the case where the file does not already exists first, then the case
  // where the file already exists.
  for (let i = 0; i < 2; i++) {
    let saver = new BackgroundFileSaverOutputStream();
    saver.enableAppend();
    saver.enableSha256();
    let completionPromise = promiseSaverComplete(saver);

    saver.setTarget(destFile, false);
    await promiseCopyToSaver(TEST_DATA_LONG, saver, true);

    saver.finish(Cr.NS_OK);
    await completionPromise;

    // Verify results.
    let expectedContents =
      i == 0 ? TEST_DATA_LONG : TEST_DATA_LONG + TEST_DATA_LONG;
    await promiseVerifyContents(destFile, expectedContents);
    Assert.equal(
      EXPECTED_HASHES[expectedContents.length],
      toHex(saver.sha256Hash)
    );
  }

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_finish_only() {
  // This test checks creating the object and doing nothing.
  let saver = new BackgroundFileSaverOutputStream();
  function onTargetChange(aTarget) {
    do_throw("Should not receive the onTargetChange notification.");
  }
  let completionPromise = promiseSaverComplete(saver, onTargetChange);
  saver.finish(Cr.NS_OK);
  await completionPromise;
});

add_task(async function test_empty() {
  // This test checks we still create an empty file when no data is fed.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  saver.setTarget(destFile, false);
  await promiseCopyToSaver("", saver, true);

  saver.finish(Cr.NS_OK);
  await completionPromise;

  // Verify results.
  Assert.ok(destFile.exists());
  Assert.equal(destFile.fileSize, 0);

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_empty_hash() {
  // This test checks the hash of an empty file, both in normal and append mode.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  // Test normal mode first, then append mode.
  for (let i = 0; i < 2; i++) {
    let saver = new BackgroundFileSaverOutputStream();
    if (i == 1) {
      saver.enableAppend();
    }
    saver.enableSha256();
    let completionPromise = promiseSaverComplete(saver);

    saver.setTarget(destFile, false);
    await promiseCopyToSaver("", saver, true);

    saver.finish(Cr.NS_OK);
    await completionPromise;

    // Verify results.
    Assert.equal(destFile.fileSize, 0);
    Assert.equal(EXPECTED_HASHES[0], toHex(saver.sha256Hash));
  }

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_invalid_hash() {
  let saver = new BackgroundFileSaverStreamListener();
  let completionPromise = promiseSaverComplete(saver);
  // We shouldn't be able to get the hash if hashing hasn't been enabled
  try {
    saver.sha256Hash;
    do_throw("Shouldn't be able to get hash if hashing not enabled");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw ex;
    }
  }
  // Enable hashing, but don't feed any data to saver
  saver.enableSha256();
  let destFile = getTempFile(TEST_FILE_NAME_1);
  saver.setTarget(destFile, false);
  // We don't wait on promiseSaverComplete, so the hash getter can run before
  // or after onSaveComplete is called. However, the expected behavior is the
  // same in both cases since the hash is only valid when the save completes
  // successfully.
  saver.finish(Cr.NS_ERROR_FAILURE);
  try {
    saver.sha256Hash;
    do_throw("Shouldn't be able to get hash if save did not succeed");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw ex;
    }
  }
  // Wait for completion so that the worker thread finishes dealing with the
  // target file. We expect it to fail.
  try {
    await completionPromise;
    do_throw("completionPromise should throw");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_FAILURE) {
      throw ex;
    }
  }
});

add_task(async function test_signature() {
  // Check that we get a signature if the saver is finished.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);

  try {
    saver.signatureInfo;
    do_throw("Can't get signature if saver is not complete");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw ex;
    }
  }

  saver.enableSignatureInfo();
  saver.setTarget(destFile, false);
  await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);

  saver.finish(Cr.NS_OK);
  await completionPromise;
  await promiseVerifyContents(destFile, TEST_DATA_SHORT);

  // signatureInfo is an empty nsIArray
  Assert.equal(0, saver.signatureInfo.length);

  // Clean up.
  destFile.remove(false);
});

add_task(async function test_signature_not_enabled() {
  // Check that we get a signature if the saver is finished on Windows.
  let destFile = getTempFile(TEST_FILE_NAME_1);

  let saver = new BackgroundFileSaverOutputStream();
  let completionPromise = promiseSaverComplete(saver);
  saver.setTarget(destFile, false);
  await promiseCopyToSaver(TEST_DATA_SHORT, saver, true);

  saver.finish(Cr.NS_OK);
  await completionPromise;
  try {
    saver.signatureInfo;
    do_throw("Can't get signature if not enabled");
  } catch (ex) {
    if (ex.result != Cr.NS_ERROR_NOT_AVAILABLE) {
      throw ex;
    }
  }

  // Clean up.
  destFile.remove(false);
});

add_task(function test_teardown() {
  gStillRunning = false;
});

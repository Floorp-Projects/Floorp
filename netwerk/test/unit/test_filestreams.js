/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We need the profile directory so the test harness will clean up our test
// files.
do_get_profile();

const OUTPUT_STREAM_CONTRACT_ID = "@mozilla.org/network/file-output-stream;1";
const SAFE_OUTPUT_STREAM_CONTRACT_ID =
  "@mozilla.org/network/safe-file-output-stream;1";

////////////////////////////////////////////////////////////////////////////////
//// Helper Methods

/**
 * Generates a leafName for a file that does not exist, but does *not*
 * create the file. Similar to createUnique except for the fact that createUnique
 * does create the file.
 *
 * @param aFile
 *        The file to modify in order for it to have a unique leafname.
 */
function ensure_unique(aFile) {
  ensure_unique.fileIndex = ensure_unique.fileIndex || 0;

  var leafName = aFile.leafName;
  while (aFile.clone().exists()) {
    aFile.leafName = leafName + "_" + ensure_unique.fileIndex++;
  }
}

/**
 * Tests for files being accessed at the right time. Streams that use
 * DEFER_OPEN should only open or create the file when an operation is
 * done, and not during Init().
 *
 * Note that for writing, we check for actual writing in test_NetUtil (async)
 * and in sync_operations in this file (sync), whereas in this function we
 * just check that the file is *not* created during init.
 *
 * @param aContractId
 *        The contract ID to use for the output stream
 * @param aDeferOpen
 *        Whether to check with DEFER_OPEN or not
 * @param aTrickDeferredOpen
 *        Whether we try to 'trick' deferred opens by changing the file object before
 *        the actual open. The stream should have a clone, so changes to the file
 *        object after Init and before Open should not affect it.
 */
function check_access(aContractId, aDeferOpen, aTrickDeferredOpen) {
  const LEAF_NAME = "filestreams-test-file.tmp";
  const TRICKY_LEAF_NAME = "BetYouDidNotExpectThat.tmp";
  let file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  file.append(LEAF_NAME);

  // Writing

  ensure_unique(file);
  let ostream = Cc[aContractId].createInstance(Ci.nsIFileOutputStream);
  ostream.init(
    file,
    -1,
    -1,
    aDeferOpen ? Ci.nsIFileOutputStream.DEFER_OPEN : 0
  );
  Assert.equal(aDeferOpen, !file.clone().exists()); // If defer, should not exist and vice versa
  if (aDeferOpen) {
    // File should appear when we do write to it.
    if (aTrickDeferredOpen) {
      // See |@param aDeferOpen| in the JavaDoc comment for this function
      file.leafName = TRICKY_LEAF_NAME;
    }
    ostream.write("data", 4);
    if (aTrickDeferredOpen) {
      file.leafName = LEAF_NAME;
    }
    // We did a write, so the file should now exist
    Assert.ok(file.clone().exists());
  }
  ostream.close();

  // Reading

  ensure_unique(file);
  let istream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  var initOk, getOk;
  try {
    istream.init(
      file,
      -1,
      0,
      aDeferOpen ? Ci.nsIFileInputStream.DEFER_OPEN : 0
    );
    initOk = true;
  } catch (e) {
    initOk = false;
  }
  try {
    let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
      Ci.nsIFileInputStream
    );
    fstream.init(file, -1, 0, 0);
    getOk = true;
  } catch (e) {
    getOk = false;
  }

  // If the open is deferred, then Init should succeed even though the file we
  // intend to read does not exist, and then trying to read from it should
  // fail. The other case is where the open is not deferred, and there we should
  // get an error when we Init (and also when we try to read).
  Assert.ok(
    (aDeferOpen && initOk && !getOk) || (!aDeferOpen && !initOk && !getOk)
  );
  istream.close();
}

/**
 * We test async operations in test_NetUtil.js, and here test for simple sync
 * operations on input streams.
 *
 * @param aDeferOpen
 *        Whether to use DEFER_OPEN in the streams.
 */
function sync_operations(aDeferOpen) {
  const TEST_DATA = "this is a test string";
  const LEAF_NAME = "filestreams-test-file.tmp";

  let file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  file.append(LEAF_NAME);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let ostream = Cc[OUTPUT_STREAM_CONTRACT_ID].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(
    file,
    -1,
    -1,
    aDeferOpen ? Ci.nsIFileOutputStream.DEFER_OPEN : 0
  );

  ostream.write(TEST_DATA, TEST_DATA.length);
  ostream.close();

  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, aDeferOpen ? Ci.nsIFileInputStream.DEFER_OPEN : 0);

  let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(
    Ci.nsIConverterInputStream
  );
  cstream.init(fstream, "UTF-8", 0, 0);

  let string = {};
  cstream.readString(-1, string);
  cstream.close();
  fstream.close();

  Assert.equal(string.value, TEST_DATA);
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

function test_access() {
  check_access(OUTPUT_STREAM_CONTRACT_ID, false, false);
}

function test_access_trick() {
  check_access(OUTPUT_STREAM_CONTRACT_ID, false, true);
}

function test_access_defer() {
  check_access(OUTPUT_STREAM_CONTRACT_ID, true, false);
}

function test_access_defer_trick() {
  check_access(OUTPUT_STREAM_CONTRACT_ID, true, true);
}

function test_access_safe() {
  check_access(SAFE_OUTPUT_STREAM_CONTRACT_ID, false, false);
}

function test_access_safe_trick() {
  check_access(SAFE_OUTPUT_STREAM_CONTRACT_ID, false, true);
}

function test_access_safe_defer() {
  check_access(SAFE_OUTPUT_STREAM_CONTRACT_ID, true, false);
}

function test_access_safe_defer_trick() {
  check_access(SAFE_OUTPUT_STREAM_CONTRACT_ID, true, true);
}

function test_sync_operations() {
  sync_operations();
}

function test_sync_operations_deferred() {
  sync_operations(true);
}

function do_test_zero_size_buffered(disableBuffering) {
  const LEAF_NAME = "filestreams-test-file.tmp";
  const BUFFERSIZE = 4096;

  let file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("ProfD", Ci.nsIFile);
  file.append(LEAF_NAME);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);

  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(
    file,
    -1,
    0,
    Ci.nsIFileInputStream.CLOSE_ON_EOF | Ci.nsIFileInputStream.REOPEN_ON_REWIND
  );

  var buffered = Cc[
    "@mozilla.org/network/buffered-input-stream;1"
  ].createInstance(Ci.nsIBufferedInputStream);
  buffered.init(fstream, BUFFERSIZE);

  if (disableBuffering) {
    buffered.QueryInterface(Ci.nsIStreamBufferAccess).disableBuffering();
  }

  // Scriptable input streams clamp read sizes to the return value of
  // available(), so don't quite do what we want here.
  let cstream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(
    Ci.nsIConverterInputStream
  );
  cstream.init(buffered, "UTF-8", 0, 0);

  Assert.equal(buffered.available(), 0);

  // Now try reading from this stream
  let string = {};
  Assert.equal(cstream.readString(BUFFERSIZE, string), 0);
  Assert.equal(string.value, "");

  // Now check that available() throws
  var exceptionThrown = false;
  try {
    Assert.equal(buffered.available(), 0);
  } catch (e) {
    exceptionThrown = true;
  }
  Assert.ok(exceptionThrown);

  // OK, now seek back to start
  buffered.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);

  // Now check that available() does not throw
  exceptionThrown = false;
  try {
    Assert.equal(buffered.available(), 0);
  } catch (e) {
    exceptionThrown = true;
  }
  Assert.ok(!exceptionThrown);
}

function test_zero_size_buffered() {
  do_test_zero_size_buffered(false);
  do_test_zero_size_buffered(true);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

var tests = [
  test_access,
  test_access_trick,
  test_access_defer,
  test_access_defer_trick,
  test_access_safe,
  test_access_safe_trick,
  test_access_safe_defer,
  test_access_safe_defer_trick,
  test_sync_operations,
  test_sync_operations_deferred,
  test_zero_size_buffered,
];

function run_test() {
  tests.forEach(function(test) {
    test();
  });
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BIN_SUFFIX = mozinfo.bin_suffix;
const tempDir = do_get_tempdir();

/**
 * Compares binary data of 2 arrays and throws if they aren't the same.
 * Throws on mismatch, does nothing on match.
 *
 * @param arr1 The first array to compare
 * @param arr2 The second array to compare
 */
function compareBinaryData(arr1, arr2) {
  Assert.equal(arr1.length, arr2.length);
  for (let i = 0; i < arr1.length; i++) {
    if (arr1[i] != arr2[i]) {
      throw new Error(
        `Data differs at index ${i}, arr1: ${arr1[i]}, arr2: ${arr2[i]}`
      );
    }
  }
}

/**
 * Reads a file's data and returns it
 *
 * @param file The file to read the data from
 * @return a byte array for the data in the file.
 */
function getBinaryFileData(file) {
  let fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  // Open as RD_ONLY with default permissions.
  fileStream.init(file, -1, -1, null);

  // Check the returned size versus the expected size.
  let stream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  stream.setInputStream(fileStream);
  let bytes = stream.readByteArray(stream.available());
  fileStream.close();
  return bytes;
}

/**
 * Runs each method in the passed in object
 * Every method of the passed in object that starts with test_ will be ran
 * The cleanup_per_test method of the object will be run right away, it will be
 * registered to be the cleanup function, and it will be run between each test.
 *
 * @return The number of tests ran
 */
function run_tests(obj) {
  let cleanup_per_test = obj.cleanup_per_test;
  if (cleanup_per_test === undefined) {
    cleanup_per_test = function __cleanup_per_test() {};
  }

  registerCleanupFunction(cleanup_per_test);

  // Make sure there's nothing left over from a preious failed test
  cleanup_per_test();

  let ranCount = 0;
  // hasOwnProperty ensures we only see direct properties and not all
  for (let f in obj) {
    if (
      typeof obj[f] === "function" &&
      obj.hasOwnProperty(f) &&
      f.toString().indexOf("test_") === 0
    ) {
      obj[f]();
      cleanup_per_test();
      ranCount++;
    }
  }
  return ranCount;
}

/**
 * Creates a MAR file with the content of files.
 *
 * @param outMAR  The file where the MAR should be created to
 * @param dataDir The directory where the relative file paths exist
 * @param files   The relative file paths of the files to include in the MAR
 */
function createMAR(outMAR, dataDir, files) {
  // You cannot create an empy MAR.
  Assert.ok(!!files.length);

  // Get an nsIProcess to the signmar binary.
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

  // Make sure the signmar binary exists and is an executable.
  Assert.ok(signmarBin.exists());
  Assert.ok(signmarBin.isExecutable());

  // Ensure on non Windows platforms we encode the same permissions
  // as the refernence MARs contain.  On Windows this is also safe.
  // The reference MAR files have permissions of 0o664, so in case
  // someone is running these tests locally with another permission
  // (perhaps 0o777), make sure that we encode them as 0o664.
  for (let filePath of files) {
    let f = dataDir.clone();
    f.append(filePath);
    f.permissions = 0o664;
  }

  // Setup the command line arguments to create the MAR.
  let args = [
    "-C",
    dataDir.path,
    "-H",
    "@MAR_CHANNEL_ID@",
    "-V",
    "13.0a1",
    "-c",
    outMAR.path,
  ];
  args = args.concat(files);

  info("Running: " + signmarBin.path + " " + args.join(" "));
  process.init(signmarBin);
  process.run(true, args, args.length);

  // Verify signmar returned 0 for success.
  Assert.equal(process.exitValue, 0);

  // Verify the out MAR file actually exists.
  Assert.ok(outMAR.exists());
}

/**
 * Extracts a MAR file to the specified output directory.
 *
 * @param mar     The MAR file that should be matched
 * @param dataDir The directory to extract to
 */
function extractMAR(mar, dataDir) {
  // Get an nsIProcess to the signmar binary.
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

  // Make sure the signmar binary exists and is an executable.
  Assert.ok(signmarBin.exists());
  Assert.ok(signmarBin.isExecutable());

  // Setup the command line arguments to extract the MAR.
  let args = ["-C", dataDir.path, "-x", mar.path];

  info("Running: " + signmarBin.path + " " + args.join(" "));
  process.init(signmarBin);
  process.run(true, args, args.length);

  return process.exitValue;
}

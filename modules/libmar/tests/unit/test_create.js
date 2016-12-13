/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {

  /**
   * Creates MAR from the passed files, compares it to the reference MAR.
   *
   * @param refMARFileName The name of the MAR file that should match
   * @param files          The files that should go in the created MAR
   * @param checkNoMAR     If true return an error if a file already exists
   */
  function run_one_test(refMARFileName, files, checkNoMAR) {
    if (checkNoMAR === undefined) {
      checkNoMAR = true;
    }

    // Ensure the MAR we will create doesn't already exist.
    let outMAR = tempDir.clone();
    outMAR.append("out.mar");
    if (checkNoMAR) {
      do_check_false(outMAR.exists());
    }

    // Create the actual MAR file.
    createMAR(outMAR, do_get_file("data"), files);

    // Get the reference MAR data.
    let refMAR = do_get_file("data/" + refMARFileName);
    let refMARData = getBinaryFileData(refMAR);

    // Verify the data of the MAR is what it should be.
    let outMARData = getBinaryFileData(outMAR);
    if (mozinfo.os != "win") {
      // Modify the array index that contains the file permission in this mar so
      // the comparison succeeds. This value is only changed when the value is
      // the expected value on non-Windows platforms since the MAR files are
      // created on Windows. This makes it possible to use the same MAR files for
      // all platforms.
      switch (refMARFileName) {
        case "0_sized.mar":
          if (outMARData[143] == 180) {
            outMARData[143] = 182;
          }
          break;
        case "1_byte.mar":
          if (outMARData[144] == 180) {
            outMARData[144] = 182;
          }
          break;
        case "binary_data.mar":
          if (outMARData[655] == 180) {
            outMARData[655] = 182;
          }
          break;
        case "multiple_file.mar":
          if (outMARData[656] == 180) {
            outMARData[656] = 182;
          }
          if (outMARData[681] == 180) {
            outMARData[681] = 182;
          }
          if (outMARData[705] == 180) {
            outMARData[705] = 182;
          }
      }
    }
    compareBinaryData(outMARData, refMARData);
  }

  // Define the unit tests to run.
  let tests = {
    // Test creating a MAR file with a 0 byte file.
    test_zero_sized: function _test_zero_sized() {
      return run_one_test("0_sized.mar", ["0_sized_file"]);
    },
    // Test creating a MAR file with a 1 byte file.
    test_one_byte: function _test_one_byte() {
      return run_one_test("1_byte.mar", ["1_byte_file"]);
    },
    // Test creating a MAR file with binary data.
    test_binary_data: function _test_binary_data() {
      return run_one_test("binary_data.mar", ["binary_data_file"]);
    },
    // Test creating a MAR file with multiple files inside of it.
    test_multiple_file: function _test_multiple_file() {
      return run_one_test("multiple_file.mar",
                          ["0_sized_file", "1_byte_file", "binary_data_file"]);
    },
    // Test creating a MAR file on top of a different one that already exists
    // at the location the new one will be created at.
    test_overwrite_already_exists: function _test_overwrite_already_exists() {
      let differentFile = do_get_file("data/1_byte.mar");
      let outMARDir = tempDir.clone();
      differentFile.copyTo(outMARDir, "out.mar");
      return run_one_test("binary_data.mar", ["binary_data_file"], false);
    },
    // Between each test make sure the out MAR does not exist.
    cleanup_per_test: function _cleanup_per_test() {
      let outMAR = tempDir.clone();
      outMAR.append("out.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }
    }
  };

  // Run all the tests
  do_check_eq(run_tests(tests), Object.keys(tests).length - 1);
}

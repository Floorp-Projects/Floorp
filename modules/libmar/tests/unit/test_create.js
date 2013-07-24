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
    compareBinaryData(outMARData, refMARData);
  }

  // Define the unit tests to run.
  let tests = {
    // Test creating a MAR file with a 0 byte file.
    test_zero_sized: function() {
      return run_one_test(refMARPrefix + "0_sized_mar.mar", ["0_sized_file"]);
    },
    // Test creating a MAR file with a 1 byte file.
    test_one_byte: function() {
      return run_one_test(refMARPrefix + "1_byte_mar.mar", ["1_byte_file"]);
    },
    // Test creating a MAR file with binary data.
    test_binary_data: function() {
      return run_one_test(refMARPrefix + "binary_data_mar.mar", 
                          ["binary_data_file"]);
    },
    // Test creating a MAR file with multiple files inside of it.
    test_multiple_file: function() {
      return run_one_test(refMARPrefix + "multiple_file_mar.mar", 
                          ["0_sized_file", "1_byte_file", "binary_data_file"]);
    },
    // Test creating a MAR file on top of a different one that already exists
    // at the location the new one will be created at.
    test_overwrite_already_exists: function() {
      let differentFile = do_get_file("data/1_byte_mar.mar");
      let outMARDir = tempDir.clone();
      differentFile.copyTo(outMARDir, "out.mar");
      return run_one_test(refMARPrefix + "binary_data_mar.mar", 
                          ["binary_data_file"], false);
    },
    // Between each test make sure the out MAR does not exist.
    cleanup_per_test: function() {
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

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {

  /**
   * Extracts a MAR and makes sure each file matches the reference files.
   *
   * @param marFileName The name of the MAR file to extract
   * @param files       The files that the extracted MAR should contain
  */
  function run_one_test(marFileName, files) { 
    // Get the MAR file that we will be extracting
    let mar = do_get_file("data/" + marFileName);

    // Get the path that we will extract to
    let outDir = do_get_file("out", true);
    do_check_false(outDir.exists());
    outDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);

    // Get the ref files and the files that will be extracted.
    let outFiles = [];
    let refFiles = [];
    for (let i = 0; i < files.length; i++) {
      let outFile = do_get_file("out/" + files[i], true);
      do_check_false(outFile.exists());

      outFiles.push(outFile);
      refFiles.push(do_get_file("data/" + files[i]));
    }

    // Extract the MAR contents into the ./out dir.
    extractMAR(mar, outDir);

    // Compare to make sure the extracted files are the same.
    for (let i = 0; i < files.length; i++) {
      do_check_true(outFiles[i].exists());
      let refFileData = getBinaryFileData(refFiles[i]);
      let outFileData = getBinaryFileData(outFiles[i]);
      compareBinaryData(refFileData, outFileData);
    }
  }

  // Define the unit tests to run.
  let tests = {
    // Test extracting a MAR file with a 0 byte file.
    test_zero_sized: function() {
      return run_one_test("0_sized_mar.mar", ["0_sized_file"]);
    }, 
    // Test extracting a MAR file with a 1 byte file.
    test_one_byte: function() {
      return run_one_test("1_byte_mar.mar", ["1_byte_file"]);
    },
    // Test extracting a MAR file with binary data.
    test_binary_data: function() {
      return run_one_test("binary_data_mar.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR without a product information block (PIB) which
    // contains binary data.
    test_no_pib: function() {
      return run_one_test("no_pib_mar.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR without a product information block (PIB) that is
    // signed and which contains binary data.
    test_no_pib_signed: function() {
      return run_one_test("signed_no_pib_mar.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR with a product information block (PIB) that is
    // signed and which contains binary data.
    test_pib_signed: function() {
      return run_one_test("signed_pib_mar.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR file with multiple files inside of it.
    test_multiple_file: function() {
      return run_one_test("multiple_file_mar.mar", 
                          ["0_sized_file", "1_byte_file", "binary_data_file"]);
    }, 
    // Between each test make sure the out directory and its subfiles do 
    // not exist.
    cleanup_per_test: function() {
      let outDir = do_get_file("out", true);
      if (outDir.exists()) {
        outDir.remove(true);
      }
    }
  };

  // Run all the tests
  do_check_eq(run_tests(tests), Object.keys(tests).length - 1);
}

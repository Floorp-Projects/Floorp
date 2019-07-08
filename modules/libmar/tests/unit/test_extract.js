/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  /**
   * Extracts a MAR and makes sure each file matches the reference files.
   *
   * @param marFileName The name of the MAR file to extract
   * @param files       The files that the extracted MAR should contain
   */
  function extract_and_compare(marFileName, files) {
    // Get the MAR file that we will be extracting
    let mar = do_get_file("data/" + marFileName);

    // Get the path that we will extract to
    let outDir = tempDir.clone();
    outDir.append("out");
    Assert.ok(!outDir.exists());
    outDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o777);

    // Get the ref files and the files that will be extracted.
    let outFiles = [];
    let refFiles = [];
    for (let i = 0; i < files.length; i++) {
      let outFile = outDir.clone();
      outFile.append(files[i]);
      Assert.ok(!outFile.exists());

      outFiles.push(outFile);
      refFiles.push(do_get_file("data/" + files[i]));
    }

    // Extract the MAR contents to ./out dir and verify 0 for success.
    Assert.equal(extractMAR(mar, outDir), 0);

    // Compare to make sure the extracted files are the same.
    for (let i = 0; i < files.length; i++) {
      Assert.ok(outFiles[i].exists());
      let refFileData = getBinaryFileData(refFiles[i]);
      let outFileData = getBinaryFileData(outFiles[i]);
      compareBinaryData(refFileData, outFileData);
    }
  }

  /**
   * Attempts to extract a MAR and expects a failure
   *
   * @param marFileName The name of the MAR file to extract
   */
  function extract_and_fail(marFileName) {
    // Get the MAR file that we will be extracting
    let mar = do_get_file("data/" + marFileName);

    // Get the path that we will extract to
    let outDir = tempDir.clone();
    outDir.append("out");
    Assert.ok(!outDir.exists());
    outDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o777);

    // Extract the MAR contents to ./out dir and verify -1 (255 from the
    // nsIprocess) for failure
    Assert.equal(extractMAR(mar, outDir), 1);
  }

  // Define the unit tests to run.
  let tests = {
    // Test extracting a MAR file with a 0 byte file.
    test_zero_sized: function _test_zero_sized() {
      return extract_and_compare("0_sized.mar", ["0_sized_file"]);
    },
    // Test extracting a MAR file with a 1 byte file.
    test_one_byte: function _test_one_byte() {
      return extract_and_compare("1_byte.mar", ["1_byte_file"]);
    },
    // Test extracting a MAR file with binary data.
    test_binary_data: function _test_binary_data() {
      return extract_and_compare("binary_data.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR without a product information block (PIB) which
    // contains binary data.
    test_no_pib: function _test_no_pib() {
      return extract_and_compare("no_pib.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR without a product information block (PIB) that is
    // signed and which contains binary data.
    test_no_pib_signed: function _test_no_pib_signed() {
      return extract_and_compare("signed_no_pib.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR with a product information block (PIB) that is
    // signed and which contains binary data.
    test_pib_signed: function _test_pib_signed() {
      return extract_and_compare("signed_pib.mar", ["binary_data_file"]);
    },
    // Test extracting a MAR file with multiple files inside of it.
    test_multiple_file: function _test_multiple_file() {
      return extract_and_compare("multiple_file.mar", [
        "0_sized_file",
        "1_byte_file",
        "binary_data_file",
      ]);
    },
    // Test collision detection where file A + B are the same offset
    test_collision_same_offset: function test_collision_same_offset() {
      return extract_and_fail("manipulated_same_offset.mar");
    },
    // Test collision detection where file A's indexes are a subset of file B's
    test_collision_is_contained: function test_collision_is_contained() {
      return extract_and_fail("manipulated_is_container.mar");
    },
    // Test collision detection where file B's indexes are a subset of file A's
    test_collision_contained_by: function test_collision_contained_by() {
      return extract_and_fail("manipulated_is_contained.mar");
    },
    // Test collision detection where file A ends in file B's indexes
    test_collision_a_onto_b: function test_collision_a_onto_b() {
      return extract_and_fail("manipulated_frontend_collision.mar");
    },
    // Test collision detection where file B ends in file A's indexes
    test_collsion_b_onto_a: function test_collsion_b_onto_a() {
      return extract_and_fail("manipulated_backend_collision.mar");
    },
    // Test collision detection where file C shares indexes with both file A & B
    test_collision_multiple: function test_collision_multiple() {
      return extract_and_fail("manipulated_multiple_collision.mar");
    },
    // Test collision detection where A is the last file in the list
    test_collision_last: function test_collision_multiple_last() {
      return extract_and_fail("manipulated_multiple_collision_last.mar");
    },
    // Test collision detection where A is the first file in the list
    test_collision_first: function test_collision_multiple_first() {
      return extract_and_fail("manipulated_multiple_collision_first.mar");
    },
    // Between each test make sure the out directory and its subfiles do
    // not exist.
    cleanup_per_test: function _cleanup_per_test() {
      let outDir = tempDir.clone();
      outDir.append("out");
      if (outDir.exists()) {
        outDir.remove(true);
      }
    },
  };

  // Run all the tests
  Assert.equal(run_tests(tests), Object.keys(tests).length - 1);
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {

  /**
   * Signs a MAR file.
   *
   * @param inMAR The MAR file that should be signed
   * @param outMAR The MAR file to create
  */
  function signMAR(inMAR, outMAR) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to create the MAR.
    let NSSConfigDir = do_get_file("data");
    let args = ["-d", NSSConfigDir.path, "-n", "mycert", "-s", 
                inMAR.path, outMAR.path];

    do_print('Running sign operation: ' + signmarBin.path);
    process.init(signmarBin);
    process.run(true, args, args.length);

    // Verify signmar returned 0 for success.
    do_check_eq(process.exitValue, 0);
  }

  /**
   * Verifies a MAR file.
   *
   * @param signedMAR Verifies a MAR file
  */
  function verifyMAR(signedMAR, wantSuccess) {
    if (wantSuccess === undefined) {
      wantSuccess = true;
    }
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    let DERFile = do_get_file("data/mycert.der");

    // Will reference the arguments to use for verification in signmar
    let args;

    // The XPCShell test wiki indicates this is the preferred way for 
    // Windows detection.
    var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

    // Setup the command line arguments to create the MAR.
    // Windows vs. Linux/Mac/... have different command line for verification 
    // since  on Windows we verify with CryptoAPI and on all other platforms 
    // we verify with NSS. So on Windows we use an exported DER file and on 
    // other platforms we use the NSS config db.
    if (isWindows) {
      args = ["-D", DERFile.path, "-v", signedMAR.path];
    } else {
      let NSSConfigDir = do_get_file("data");
      args = ["-d", NSSConfigDir.path, "-n", "mycert", "-v", signedMAR.path];
    }

    do_print('Running verify operation: ' + signmarBin.path);
    process.init(signmarBin);
    try {
      // We put this in a try block because nsIProcess doesn't like -1 returns
      process.run(true, args, args.length);
    } catch (e) {
      process.exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      do_check_eq(process.exitValue, 0);
    } else {
      do_check_neq(process.exitValue, 0);
    }
  }

  /**
   * Strips a MAR signature.
   *
   * @param signedMAR The MAR file that should be signed
   * @param outMAR The MAR file to write to with signature stripped
  */
  function stripMARSignature(signedMAR, outMAR) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to create the MAR.
    let args = ["-r", signedMAR.path, outMAR.path];

    do_print('Running sign operation: ' + signmarBin.path);
    process.init(signmarBin);
    process.run(true, args, args.length);

    // Verify signmar returned 0 for success.
    do_check_eq(process.exitValue, 0);
  }


  function cleanup() {
    let outMAR = do_get_file("signed_out.mar", true);
    if (outMAR.exists()) {
      outMAR.remove(false);
    }

    outMAR = do_get_file("out.mar", true);
    if (outMAR.exists()) {
      outMAR.remove(false);
    }

    let outDir = do_get_file("out", true);
    if (outDir.exists()) {
      outDir.remove(true);
    }
  }

  // Define the unit tests to run.
  let tests = {
    // Test signing a MAR file
    test_sign: function() {
      let inMAR = do_get_file("data/binary_data_mar.mar");
      let outMAR = do_get_file("signed_out.mar", true);
      do_check_false(outMAR.exists());
      signMAR(inMAR, outMAR);
      do_check_true(outMAR.exists());
      let outMARData = getBinaryFileData(outMAR);
      let refMAR = do_get_file("data/signed_pib_mar.mar");
      let refMARData = getBinaryFileData(refMAR);
      compareBinaryData(outMARData, refMARData);
    }, 
    // Test verifying a signed MAR file
    test_verify: function() {
      let signedMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(signedMAR);
    }, 
    // Test verifying a signed MAR file without a PIB
    test_verify_no_pib: function() {
      let signedMAR = do_get_file("data/signed_no_pib_mar.mar");
      verifyMAR(signedMAR);
    }, 
    // Test verifying a crafted MAR file where the attacker tried to adjust
    // the version number manually.
    test_crafted_mar: function() {
      let signedBadMAR = do_get_file("data/manipulated_signed_mar.mar");
      verifyMAR(signedBadMAR, false);
    }, 
    // Test to make sure a stripped MAR is the same as the original MAR
    test_strip_signature: function() {
      let originalMAR = do_get_file("data/binary_data_mar.mar");
      let signedMAR = do_get_file("signed_out.mar");
      let outMAR = do_get_file("out.mar", true);
      stripMARSignature(signedMAR, outMAR);

      // Verify that the stripped MAR matches the original data MAR exactly
      let outMARData = getBinaryFileData(outMAR);
      let originalMARData = getBinaryFileData(originalMAR);
      compareBinaryData(outMARData, originalMARData);
    },
  };

  cleanup();

  // Run all the tests, there should be 5.
  do_check_eq(run_tests(tests), 5);

  do_register_cleanup(cleanup);
}

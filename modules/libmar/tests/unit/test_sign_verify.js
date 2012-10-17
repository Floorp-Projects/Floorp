/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {

  /**
   * Signs a MAR file.
   *
   * @param inMAR The MAR file that should be signed
   * @param outMAR The MAR file to create
  */
  function signMAR(inMAR, outMAR, certs, wantSuccess, useShortHandCmdLine) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to sign the MAR.
    let NSSConfigDir = do_get_file("data");
    let args = ["-d", NSSConfigDir.path];
    if (certs.length == 1 && useShortHandCmdLine) {
      args.push("-n", certs[0]);
    } else {
      for (i = 0; i < certs.length; i++) {
        args.push("-n" + i, certs[i]);
      }
    }
    args.push("-s", inMAR.path, outMAR.path);

    do_print('Running sign operation: ' + signmarBin.path);
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
    } catch(e) {
      // On Windows negative return value throws an exception
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
   * Verifies a MAR file.
   *
   * @param signedMAR Verifies a MAR file
  */
  function verifyMAR(signedMAR, wantSuccess, certs, useShortHandCmdLine) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Will reference the arguments to use for verification in signmar
    let args = [];

    // The XPCShell test wiki indicates this is the preferred way for 
    // Windows detection.
    var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

    // Setup the command line arguments to create the MAR.
    // Windows vs. Linux/Mac/... have different command line for verification 
    // since  on Windows we verify with CryptoAPI and on all other platforms 
    // we verify with NSS. So on Windows we use an exported DER file and on 
    // other platforms we use the NSS config db.
    if (isWindows) {
      if (certs.length == 1 && useShortHandCmdLine) {
        args.push("-D", "data/" + certs[0] + ".der");
      } else {
        for (i = 0; i < certs.length; i++) {
          args.push("-D" + i, "data/" + certs[i] + ".der");
        }
      }
      args.push("-v", signedMAR.path);
    } else {
      let NSSConfigDir = do_get_file("data");
      args = ["-d", NSSConfigDir.path];
      if (certs.length == 1 && useShortHandCmdLine) {
        args.push("-n", certs[0]);
      } else {
        for (i = 0; i < certs.length; i++) {
          args.push("-n" + i, certs[i]);
        }
      }
      args.push("-v", signedMAR.path);
    }

    do_print('Running verify operation: ' + signmarBin.path);
    process.init(signmarBin);
    try {
      // We put this in a try block because nsIProcess doesn't like -1 returns
      process.run(true, args, args.length);
    } catch (e) {
      // On Windows negative return value throws an exception
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
  function stripMARSignature(signedMAR, outMAR, wantSuccess) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to create the MAR.
    let args = ["-r", signedMAR.path, outMAR.path];
    do_print('=----- -r ' + signedMAR.path + ' ' + outMAR.path + '\n\n\n');

    do_print('Running sign operation: ' + signmarBin.path);
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
    } catch (e) {
      // On Windows negative return value throws an exception
      process.exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      do_check_eq(process.exitValue, 0);
    } else {
      do_check_neq(process.exitValue, 0);
    }
  }


  function cleanup() {
    let outMAR = do_get_file("signed_out.mar", true);
    if (outMAR.exists()) {
      outMAR.remove(false);
    }
    outMAR = do_get_file("multiple_signed_out.mar", true);
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

  const wantFailure = false;
  const wantSuccess = true;
  // Define the unit tests to run.
  let tests = {
    // Test signing a MAR file with a single signature
    test_sign_single: function() {
      let inMAR = do_get_file("data/" + refMARPrefix + "binary_data_mar.mar");
      let outMAR = do_get_file("signed_out.mar", true);
      if (outMAR.exists()) {
        outMAR.remove(false);
      }
      signMAR(inMAR, outMAR, ["mycert"], wantSuccess, true);
      do_check_true(outMAR.exists());
      let outMARData = getBinaryFileData(outMAR);
      let refMAR = do_get_file("data/" + refMARPrefix + "signed_pib_mar.mar");
      let refMARData = getBinaryFileData(refMAR);
      compareBinaryData(outMARData, refMARData);
    }, 
    // Test signing a MAR file with multiple signatures
    test_sign_multiple: function() {
      let inMAR = do_get_file("data/" + refMARPrefix + "binary_data_mar.mar");
      let outMAR = do_get_file("multiple_signed_out.mar", true);
      if (outMAR.exists()) {
        outMAR.remove(false);
      }
      do_check_false(outMAR.exists());
      signMAR(inMAR, outMAR, ["mycert", "mycert2", "mycert3"],
              wantSuccess, true);
      do_check_true(outMAR.exists());
      let outMARData = getBinaryFileData(outMAR);
      let refMAR = do_get_file("data/" + refMARPrefix + "multiple_signed_pib_mar.mar");
      let refMARData = getBinaryFileData(refMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test verifying a signed MAR file
    test_verify_single: function() {
      let signedMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert"], true);
      verifyMAR(signedMAR, wantSuccess, ["mycert"], false);
    }, 
    // Test verifying a signed MAR file with too many certs fails.
    // Or if you want to look at it another way, One mycert signature
    // is missing.
    test_verify_single_too_many_certs: function() {
      let signedMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert"], true);
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert"], false);
    },
    // Test verifying a signed MAR file fails when using a wrong cert
    test_verify_single_wrong_cert: function() {
      let signedMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert2"], true);
      verifyMAR(signedMAR, wantFailure, ["mycert2"], false);
    },
    // Test verifying a signed MAR file with multiple signatures
    test_verify_multiple: function() {
      let signedMAR = do_get_file("data/multiple_signed_pib_mar.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying an unsigned MAR file fails
    test_verify_unsigned_mar_file_fails: function() {
      let unsignedMAR = do_get_file("data/binary_data_mar.mar");
      verifyMAR(unsignedMAR, wantFailure, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying a signed MAR file with the same signature multiple
    // times fails.  The input MAR has: mycert, mycert2, mycert3.
    // we're checking to make sure the number of verified signatures
    // is only 1 and not 3.  Each signature should be verified once.
    test_verify_multiple_same_cert: function() {
      let signedMAR = do_get_file("data/multiple_signed_pib_mar.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert", "mycert"]);
    },
    // Test verifying a signed MAR file with the correct signatures but in
    // a different order fails
    test_verify_multiple_wrong_order: function() {
      let signedMAR = do_get_file("data/multiple_signed_pib_mar.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert3", "mycert2"]);
      verifyMAR(signedMAR, wantFailure, ["mycert2", "mycert", "mycert3"]);
      verifyMAR(signedMAR, wantFailure, ["mycert2", "mycert3", "mycert"]);
      verifyMAR(signedMAR, wantFailure, ["mycert3", "mycert", "mycert2"]);
      verifyMAR(signedMAR, wantFailure, ["mycert3", "mycert2", "mycert"]);
    },
    // Test verifying a signed MAR file without a PIB
    test_verify_no_pib: function() {
      let signedMAR = do_get_file("data/signed_no_pib_mar.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert"], true);
      verifyMAR(signedMAR, wantSuccess, ["mycert"], false);
    }, 
    // Test verifying a signed MAR file with multiple signatures without a PIB
    test_verify_no_pib_multiple: function() {
      let signedMAR = do_get_file("data/multiple_signed_no_pib_mar.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying a crafted MAR file where the attacker tried to adjust
    // the version number manually.
    test_crafted_mar: function() {
      let signedBadMAR = do_get_file("data/manipulated_signed_mar.mar");
      verifyMAR(signedBadMAR, wantFailure, ["mycert"], true);
      verifyMAR(signedBadMAR, wantFailure, ["mycert"], false);
    }, 
    // Test verifying a file that doesn't exist fails
    test_bad_path_verify_fails: function() {
      let noMAR = do_get_file("data/does_not_exist_.mar", true);
      do_check_false(noMAR.exists());
      verifyMAR(noMAR, wantFailure, ["mycert"], true);
    },
    // Test to make sure a stripped MAR is the same as the original MAR
    test_strip_signature: function() {
      let originalMAR = do_get_file("data/" + 
                                    refMARPrefix + 
                                    "binary_data_mar.mar");
      let signedMAR = do_get_file("signed_out.mar");
      let outMAR = do_get_file("out.mar", true);
      stripMARSignature(signedMAR, outMAR, wantSuccess);

      // Verify that the stripped MAR matches the original data MAR exactly
      let outMARData = getBinaryFileData(outMAR);
      let originalMARData = getBinaryFileData(originalMAR);
      compareBinaryData(outMARData, originalMARData);
    },
    // Test to make sure a stripped multi-signature-MAR is the same as the original MAR
    test_strip_multiple_signatures: function() {
      let originalMAR = do_get_file("data/" +
                                    refMARPrefix +
                                    "binary_data_mar.mar");
      let signedMAR = do_get_file("multiple_signed_out.mar");
      let outMAR = do_get_file("out.mar", true);
      stripMARSignature(signedMAR, outMAR, wantSuccess);

      // Verify that the stripped MAR matches the original data MAR exactly
      let outMARData = getBinaryFileData(outMAR);
      let originalMARData = getBinaryFileData(originalMAR);
      compareBinaryData(outMARData, originalMARData);
    },
    // Test signing a file that doesn't exist fails
    test_bad_path_sign_fails: function() {
      let inMAR = do_get_file("data/does_not_exist_.mar", true);
      let outMAR = do_get_file("signed_out.mar", true);
      do_check_false(inMAR.exists());
      signMAR(inMAR, outMAR, ["mycert"], wantFailure, true);
      do_check_false(outMAR.exists());
    },
    // Test verifying only a subset of the signatures fails.
    // The input MAR has: mycert, mycert2, mycert3.
    // We're only verifying 2 of the 3 signatures and that should fail.
    test_verify_multiple_subset: function() {
      let signedMAR = do_get_file("data/multiple_signed_pib_mar.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert2"]);
    },
    // Test stripping a MAR that doesn't exist fails 
    test_bad_path_strip_fails: function() {
      let noMAR = do_get_file("data/does_not_exist_mar", true);
      do_check_false(noMAR.exists());
      let outMAR = do_get_file("out.mar", true);
      stripMARSignature(noMAR, outMAR, wantFailure);
    },
    // Between each test make sure the out MAR does not exist.
    cleanup_per_test: function() {
    }
  };

  cleanup();

  // Run all the tests
  do_check_eq(run_tests(tests), Object.keys(tests).length - 1);

  do_register_cleanup(cleanup);
}

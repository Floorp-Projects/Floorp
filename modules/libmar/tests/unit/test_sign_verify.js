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
      for (var i = 0; i < certs.length; i++) {
        args.push("-n" + i, certs[i]);
      }
    }
    args.push("-s", inMAR.path, outMAR.path);

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
   * Extract a MAR signature.
   *
   * @param inMAR        The MAR file who's signature should be extracted
   * @param sigIndex     The index of the signature to extract
   * @param extractedSig The file where the extracted signature will be stored
   * @param wantSuccess  True if a successful signmar return code is desired
  */
  function extractMARSignature(inMAR, sigIndex, extractedSig, wantSuccess) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to extract the signature in the MAR.
    let args = ["-n" + sigIndex, "-X", inMAR.path, extractedSig.path];

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
   * Import a MAR signature.
   *
   * @param inMAR        The MAR file who's signature should be imported to
   * @param sigIndex     The index of the signature to import to
   * @param sigFile      The file where the base64 signature exists
   * @param outMAR       The same as inMAR but with the specified signature
   *                     swapped at the specified index.
   * @param wantSuccess  True if a successful signmar return code is desired
  */
  function importMARSignature(inMAR, sigIndex, sigFile, outMAR, wantSuccess) {
    // Get a process to the signmar binary from the dist/bin directory.
    let process = Cc["@mozilla.org/process/util;1"].
                  createInstance(Ci.nsIProcess);
    let signmarBin = do_get_file("signmar" + BIN_SUFFIX);

    // Make sure the signmar binary exists and is an executable.
    do_check_true(signmarBin.exists());
    do_check_true(signmarBin.isExecutable());

    // Setup the command line arguments to import the signature in the MAR.
    let args = ["-n" + sigIndex, "-I", inMAR.path, sigFile.path, outMAR.path];

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

    // Setup the command line arguments to create the MAR.
    // Windows & Mac vs. Linux/... have different command line for verification
    // since on Windows we verify with CryptoAPI, on Mac with Security
    // Transforms or CDSA/CSSM and on all other platforms we verify with NSS. So
    // on Windows and Mac we use an exported DER file and on other platforms we
    // use the NSS config db.
    if (mozinfo.os == "win" || mozinfo.os == "mac") {
      if (certs.length == 1 && useShortHandCmdLine) {
        args.push("-D", "data/" + certs[0] + ".der");
      } else {
        for (var i = 0; i < certs.length; i++) {
          args.push("-D" + i, "data/" + certs[i] + ".der");
        }
      }
    } else {
      let NSSConfigDir = do_get_file("data");
      args = ["-d", NSSConfigDir.path];
      if (certs.length == 1 && useShortHandCmdLine) {
        args.push("-n", certs[0]);
      } else {
        for (var i = 0; i < certs.length; i++) {
          args.push("-n" + i, certs[i]);
        }
      }
    }
    args.push("-v", signedMAR.path);

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
    let outMAR = tempDir.clone();
    outMAR.append("signed_out.mar");
    if (outMAR.exists()) {
      outMAR.remove(false);
    }
    outMAR = tempDir.clone();
    outMAR.append("multiple_signed_out.mar");
    if (outMAR.exists()) {
      outMAR.remove(false);
    }
    outMAR = tempDir.clone();
    outMAR.append("out.mar");
    if (outMAR.exists()) {
      outMAR.remove(false);
    }

    let outDir = tempDir.clone();
    outDir.append("out");
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
      let outMAR = tempDir.clone();
      outMAR.append("signed_out.mar");
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
      let outMAR = tempDir.clone();
      outMAR.append("multiple_signed_out.mar");
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
      let signedMAR = tempDir.clone();
      signedMAR.append("signed_out.mar");
      let outMAR = tempDir.clone();
      outMAR.append("out.mar", true);
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
      let signedMAR = tempDir.clone();
      signedMAR.append("multiple_signed_out.mar");
      let outMAR = tempDir.clone();
      outMAR.append("out.mar");
      stripMARSignature(signedMAR, outMAR, wantSuccess);

      // Verify that the stripped MAR matches the original data MAR exactly
      let outMARData = getBinaryFileData(outMAR);
      let originalMARData = getBinaryFileData(originalMAR);
      compareBinaryData(outMARData, originalMARData);
    },
    // Test extracting the first signature in a MAR that has only a single signature
    test_extract_sig_single: function() {
      let inMAR = do_get_file("data/signed_pib_mar.mar");
      let extractedSig = do_get_file("extracted_signature", true);
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      extractMARSignature(inMAR, 0, extractedSig, wantSuccess);
      do_check_true(extractedSig.exists());

      let referenceSig = do_get_file("data/signed_pib_mar.signature.0"); +
      compareBinaryData(extractedSig, referenceSig);
    },
    // Test extracting the all signatures in a multi signature MAR
    // The input MAR has 3 signatures.
    test_extract_sig_multi: function() {
      for (let i = 0; i < 3; i++) {
        let inMAR = do_get_file("data/multiple_signed_pib_mar.mar");
        let extractedSig = do_get_file("extracted_signature", true);
        if (extractedSig.exists()) {
          extractedSig.remove(false);
        }
        extractMARSignature(inMAR, i, extractedSig, wantSuccess);
        do_check_true(extractedSig.exists());

        let referenceSig = do_get_file("data/multiple_signed_pib_mar.sig." + i); +
        compareBinaryData(extractedSig, referenceSig);
      }
    },
    // Test extracting a signature that is out of range fails
    test_extract_sig_out_of_range: function() {
      let inMAR = do_get_file("data/signed_pib_mar.mar");
      let extractedSig = do_get_file("extracted_signature", true);
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      const outOfBoundsIndex = 5;
      extractMARSignature(inMAR, outOfBoundsIndex, extractedSig, wantFailure);
      do_check_false(extractedSig.exists());
    },
    // Test signing a file that doesn't exist fails
    test_bad_path_sign_fails: function() {
      let inMAR = do_get_file("data/does_not_exist_.mar", true);
      let outMAR = tempDir.clone();
      outMAR.append("signed_out.mar");
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
    // Test importing the first signature in a MAR that has only
    // a single signature
    test_import_sig_single: function() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert"], false);
      verifyMAR(inMAR, wantFailure, ["mycert2"], false);
      verifyMAR(inMAR, wantFailure, ["mycert3"], false);

      // Get the signature file for this MAR signed with the key from mycert2
      let sigFile = do_get_file("data/signed_pib_mar.signature.mycert2");
      do_check_true(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib_mar.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      //Run the import operation
      importMARSignature(inMAR, 0, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file and that mycert no longer verifies
      // and that mycert2 does verify
      do_check_true(outMAR.exists());
      verifyMAR(outMAR, wantFailure, ["mycert"], false);
      verifyMAR(outMAR, wantSuccess, ["mycert2"], false);
      verifyMAR(outMAR, wantFailure, ["mycert3"], false);

      // Compare the binary data to something that was signed originally
      // with the private key from mycert2
      let refMAR = do_get_file("data/signed_pib_mar_with_mycert2.mar");
      do_check_true(refMAR.exists());
      let refMARData = getBinaryFileData(refMAR);
      let outMARData = getBinaryFileData(outMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test importing a signature that doesn't belong to the file
    // fails to verify.
    test_import_wrong_sig: function() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/signed_pib_mar.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert"], false);
      verifyMAR(inMAR, wantFailure, ["mycert2"], false);
      verifyMAR(inMAR, wantFailure, ["mycert3"], false);

      // Get the signature file for this MAR signed with the key from mycert2
      let sigFile = do_get_file("data/multiple_signed_pib_mar.sig.0");
      do_check_true(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib_mar.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      //Run the import operation
      importMARSignature(inMAR, 0, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file and that mycert no longer verifies
      // and that mycert2 does verify
      do_check_true(outMAR.exists());
      verifyMAR(outMAR, wantFailure, ["mycert"], false);
      verifyMAR(outMAR, wantFailure, ["mycert2"], false);
      verifyMAR(outMAR, wantFailure, ["mycert3"], false);
    },
    // Test importing to the second signature in a MAR that has multiple
    // signature
    test_import_sig_multiple: function() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/multiple_signed_pib_mar.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert", "mycert2", "mycert3"], false);
      verifyMAR(inMAR, wantFailure, ["mycert", "mycert", "mycert3"], false);

      // Get the signature file for this MAR signed with the key from mycert
      let sigFile = do_get_file("data/multiple_signed_pib_mar.sig.0");
      do_check_true(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib_mar.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      //Run the import operation
      const secondSigPos = 1;
      importMARSignature(inMAR, secondSigPos, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file and that mycert no longer verifies
      // and that mycert2 does verify
      do_check_true(outMAR.exists());
      verifyMAR(outMAR, wantSuccess, ["mycert", "mycert", "mycert3"], false);
      verifyMAR(outMAR, wantFailure, ["mycert", "mycert2", "mycert3"], false);

      // Compare the binary data to something that was signed originally
      // with the private keys from mycert, mycert, mycert3
      let refMAR = do_get_file("data/multiple_signed_pib_mar_2.mar");
      do_check_true(refMAR.exists());
      let refMARData = getBinaryFileData(refMAR);
      let outMARData = getBinaryFileData(outMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test stripping a MAR that doesn't exist fails 
    test_bad_path_strip_fails: function() {
      let noMAR = do_get_file("data/does_not_exist_mar", true);
      do_check_false(noMAR.exists());
      let outMAR = tempDir.clone();
      outMAR.append("out.mar");
      stripMARSignature(noMAR, outMAR, wantFailure);
    },
    // Test extracting from a bad path fails
    test_extract_bad_path: function() {
      let noMAR = do_get_file("data/does_not_exist.mar", true);
      let extractedSig = do_get_file("extracted_signature", true);
      do_check_false(noMAR.exists());
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      extractMARSignature(noMAR, 0, extractedSig, wantFailure);
      do_check_false(extractedSig.exists());
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

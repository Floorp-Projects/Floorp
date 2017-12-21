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
    Assert.ok(signmarBin.exists());
    Assert.ok(signmarBin.isExecutable());

    // Setup the command line arguments to sign the MAR.
    let NSSConfigDir = do_get_file("data");
    let args = ["-d", NSSConfigDir.path];
    if (certs.length == 1 && useShortHandCmdLine) {
      args.push("-n", certs[0]);
    } else {
      for (let i = 0; i < certs.length; i++) {
        args.push("-n" + i, certs[i]);
      }
    }
    args.push("-s", inMAR.path, outMAR.path);

    let exitValue;
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
      exitValue = process.exitValue;
    } catch (e) {
      // On Windows negative return value throws an exception
      exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      Assert.equal(exitValue, 0);
    } else {
      Assert.notEqual(exitValue, 0);
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
    Assert.ok(signmarBin.exists());
    Assert.ok(signmarBin.isExecutable());

    // Setup the command line arguments to extract the signature in the MAR.
    let args = ["-n" + sigIndex, "-X", inMAR.path, extractedSig.path];

    let exitValue;
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
      exitValue = process.exitValue;
    } catch (e) {
      // On Windows negative return value throws an exception
      exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      Assert.equal(exitValue, 0);
    } else {
      Assert.notEqual(exitValue, 0);
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
    Assert.ok(signmarBin.exists());
    Assert.ok(signmarBin.isExecutable());

    // Setup the command line arguments to import the signature in the MAR.
    let args = ["-n" + sigIndex, "-I", inMAR.path, sigFile.path, outMAR.path];

    let exitValue;
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
      exitValue = process.exitValue;
    } catch (e) {
      // On Windows negative return value throws an exception
      exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      Assert.equal(exitValue, 0);
    } else {
      Assert.notEqual(exitValue, 0);
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
    Assert.ok(signmarBin.exists());
    Assert.ok(signmarBin.isExecutable());

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
        for (let i = 0; i < certs.length; i++) {
          args.push("-D" + i, "data/" + certs[i] + ".der");
        }
      }
    } else {
      let NSSConfigDir = do_get_file("data");
      args = ["-d", NSSConfigDir.path];
      if (certs.length == 1 && useShortHandCmdLine) {
        args.push("-n", certs[0]);
      } else {
        for (let i = 0; i < certs.length; i++) {
          args.push("-n" + i, certs[i]);
        }
      }
    }
    args.push("-v", signedMAR.path);

    let exitValue;
    process.init(signmarBin);
    try {
      // We put this in a try block because nsIProcess doesn't like -1 returns
      process.run(true, args, args.length);
      exitValue = process.exitValue;
    } catch (e) {
      // On Windows negative return value throws an exception
      exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      Assert.equal(exitValue, 0);
    } else {
      Assert.notEqual(exitValue, 0);
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
    Assert.ok(signmarBin.exists());
    Assert.ok(signmarBin.isExecutable());

    // Setup the command line arguments to create the MAR.
    let args = ["-r", signedMAR.path, outMAR.path];

    let exitValue;
    process.init(signmarBin);
    try {
      process.run(true, args, args.length);
      exitValue = process.exitValue;
    } catch (e) {
      // On Windows negative return value throws an exception
      exitValue = -1;
    }

    // Verify signmar returned 0 for success.
    if (wantSuccess) {
      Assert.equal(exitValue, 0);
    } else {
      Assert.notEqual(exitValue, 0);
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
    test_sign_single: function _test_sign_single() {
      let inMAR = do_get_file("data/binary_data.mar");
      let outMAR = tempDir.clone();
      outMAR.append("signed_out.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }
      signMAR(inMAR, outMAR, ["mycert"], wantSuccess, true);
      Assert.ok(outMAR.exists());
      let outMARData = getBinaryFileData(outMAR);
      let refMAR = do_get_file("data/signed_pib.mar");
      let refMARData = getBinaryFileData(refMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test signing a MAR file with multiple signatures
    test_sign_multiple: function _test_sign_multiple() {
      let inMAR = do_get_file("data/binary_data.mar");
      let outMAR = tempDir.clone();
      outMAR.append("multiple_signed_out.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }
      Assert.ok(!outMAR.exists());
      signMAR(inMAR, outMAR, ["mycert", "mycert2", "mycert3"],
              wantSuccess, true);
      Assert.ok(outMAR.exists());
      let outMARData = getBinaryFileData(outMAR);
      let refMAR = do_get_file("data/multiple_signed_pib.mar");
      let refMARData = getBinaryFileData(refMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test verifying a signed MAR file
    test_verify_single: function _test_verify_single() {
      let signedMAR = do_get_file("data/signed_pib.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert"], true);
      verifyMAR(signedMAR, wantSuccess, ["mycert"], false);
    },
    // Test verifying a signed MAR file with too many certs fails.
    // Or if you want to look at it another way, One mycert signature
    // is missing.
    test_verify_single_too_many_certs: function _test_verify_single_too_many_certs() {
      let signedMAR = do_get_file("data/signed_pib.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert"], true);
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert"], false);
    },
    // Test verifying a signed MAR file fails when using a wrong cert
    test_verify_single_wrong_cert: function _test_verify_single_wrong_cert() {
      let signedMAR = do_get_file("data/signed_pib.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert2"], true);
      verifyMAR(signedMAR, wantFailure, ["mycert2"], false);
    },
    // Test verifying a signed MAR file with multiple signatures
    test_verify_multiple: function _test_verify_multiple() {
      let signedMAR = do_get_file("data/multiple_signed_pib.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying an unsigned MAR file fails
    test_verify_unsigned_mar_file_fails: function _test_verify_unsigned_mar_file_fails() {
      let unsignedMAR = do_get_file("data/binary_data.mar");
      verifyMAR(unsignedMAR, wantFailure, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying a signed MAR file with the same signature multiple
    // times fails.  The input MAR has: mycert, mycert2, mycert3.
    // we're checking to make sure the number of verified signatures
    // is only 1 and not 3.  Each signature should be verified once.
    test_verify_multiple_same_cert: function _test_verify_multiple_same_cert() {
      let signedMAR = do_get_file("data/multiple_signed_pib.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert", "mycert"]);
    },
    // Test verifying a signed MAR file with the correct signatures but in
    // a different order fails
    test_verify_multiple_wrong_order: function _test_verify_multiple_wrong_order() {
      let signedMAR = do_get_file("data/multiple_signed_pib.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert3", "mycert2"]);
      verifyMAR(signedMAR, wantFailure, ["mycert2", "mycert", "mycert3"]);
      verifyMAR(signedMAR, wantFailure, ["mycert2", "mycert3", "mycert"]);
      verifyMAR(signedMAR, wantFailure, ["mycert3", "mycert", "mycert2"]);
      verifyMAR(signedMAR, wantFailure, ["mycert3", "mycert2", "mycert"]);
    },
    // Test verifying a signed MAR file without a PIB
    test_verify_no_pib: function _test_verify_no_pib() {
      let signedMAR = do_get_file("data/signed_no_pib.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert"], true);
      verifyMAR(signedMAR, wantSuccess, ["mycert"], false);
    },
    // Test verifying a signed MAR file with multiple signatures without a PIB
    test_verify_no_pib_multiple: function _test_verify_no_pib_multiple() {
      let signedMAR = do_get_file("data/multiple_signed_no_pib.mar");
      verifyMAR(signedMAR, wantSuccess, ["mycert", "mycert2", "mycert3"]);
    },
    // Test verifying a crafted MAR file where the attacker tried to adjust
    // the version number manually.
    test_crafted_mar: function _test_crafted_mar() {
      let signedBadMAR = do_get_file("data/manipulated_signed.mar");
      verifyMAR(signedBadMAR, wantFailure, ["mycert"], true);
      verifyMAR(signedBadMAR, wantFailure, ["mycert"], false);
    },
    // Test verifying a file that doesn't exist fails
    test_bad_path_verify_fails: function _test_bad_path_verify_fails() {
      let noMAR = do_get_file("data/does_not_exist.mar", true);
      Assert.ok(!noMAR.exists());
      verifyMAR(noMAR, wantFailure, ["mycert"], true);
    },
    // Test to make sure a stripped MAR is the same as the original MAR
    test_strip_signature: function _test_strip_signature() {
      let originalMAR = do_get_file("data/binary_data.mar");
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
    test_strip_multiple_signatures: function _test_strip_multiple_signatures() {
      let originalMAR = do_get_file("data/binary_data.mar");
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
    test_extract_sig_single: function _test_extract_sig_single() {
      let inMAR = do_get_file("data/signed_pib.mar");
      let extractedSig = do_get_file("extracted_signature", true);
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      extractMARSignature(inMAR, 0, extractedSig, wantSuccess);
      Assert.ok(extractedSig.exists());

      let referenceSig = do_get_file("data/signed_pib_mar.signature.0");
      compareBinaryData(extractedSig, referenceSig);
    },
    // Test extracting the all signatures in a multi signature MAR
    // The input MAR has 3 signatures.
    test_extract_sig_multi: function _test_extract_sig_multi() {
      for (let i = 0; i < 3; i++) {
        let inMAR = do_get_file("data/multiple_signed_pib.mar");
        let extractedSig = do_get_file("extracted_signature", true);
        if (extractedSig.exists()) {
          extractedSig.remove(false);
        }
        extractMARSignature(inMAR, i, extractedSig, wantSuccess);
        Assert.ok(extractedSig.exists());

        let referenceSig = do_get_file("data/multiple_signed_pib_mar.sig." + i);
        compareBinaryData(extractedSig, referenceSig);
      }
    },
    // Test extracting a signature that is out of range fails
    test_extract_sig_out_of_range: function _test_extract_sig_out_of_range() {
      let inMAR = do_get_file("data/signed_pib.mar");
      let extractedSig = do_get_file("extracted_signature", true);
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      const outOfBoundsIndex = 5;
      extractMARSignature(inMAR, outOfBoundsIndex, extractedSig, wantFailure);
      Assert.ok(!extractedSig.exists());
    },
    // Test signing a file that doesn't exist fails
    test_bad_path_sign_fails: function _test_bad_path_sign_fails() {
      let inMAR = do_get_file("data/does_not_exist.mar", true);
      let outMAR = tempDir.clone();
      outMAR.append("signed_out.mar");
      Assert.ok(!inMAR.exists());
      signMAR(inMAR, outMAR, ["mycert"], wantFailure, true);
      Assert.ok(!outMAR.exists());
    },
    // Test verifying only a subset of the signatures fails.
    // The input MAR has: mycert, mycert2, mycert3.
    // We're only verifying 2 of the 3 signatures and that should fail.
    test_verify_multiple_subset: function _test_verify_multiple_subset() {
      let signedMAR = do_get_file("data/multiple_signed_pib.mar");
      verifyMAR(signedMAR, wantFailure, ["mycert", "mycert2"]);
    },
    // Test importing the first signature in a MAR that has only
    // a single signature
    test_import_sig_single: function _test_import_sig_single() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/signed_pib.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert"], false);
      verifyMAR(inMAR, wantFailure, ["mycert2"], false);
      verifyMAR(inMAR, wantFailure, ["mycert3"], false);

      // Get the signature file for this MAR signed with the key from mycert2
      let sigFile = do_get_file("data/signed_pib_mar.signature.mycert2");
      Assert.ok(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      // Run the import operation
      importMARSignature(inMAR, 0, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file, that mycert no longer verifies and that,
      // mycert2 does verify
      Assert.ok(outMAR.exists());
      verifyMAR(outMAR, wantFailure, ["mycert"], false);
      verifyMAR(outMAR, wantSuccess, ["mycert2"], false);
      verifyMAR(outMAR, wantFailure, ["mycert3"], false);

      // Compare the binary data to something that was signed originally
      // with the private key from mycert2
      let refMAR = do_get_file("data/signed_pib_with_mycert2.mar");
      Assert.ok(refMAR.exists());
      let refMARData = getBinaryFileData(refMAR);
      let outMARData = getBinaryFileData(outMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test importing a signature that doesn't belong to the file
    // fails to verify.
    test_import_wrong_sig: function _test_import_wrong_sig() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/signed_pib.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert"], false);
      verifyMAR(inMAR, wantFailure, ["mycert2"], false);
      verifyMAR(inMAR, wantFailure, ["mycert3"], false);

      // Get the signature file for multiple_signed_pib.mar signed with the
      // key from mycert
      let sigFile = do_get_file("data/multiple_signed_pib_mar.sig.0");
      Assert.ok(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      // Run the import operation
      importMARSignature(inMAR, 0, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file and that the mar file fails to verify
      // when using a signature for another mar file.
      Assert.ok(outMAR.exists());
      verifyMAR(outMAR, wantFailure, ["mycert"], false);
      verifyMAR(outMAR, wantFailure, ["mycert2"], false);
      verifyMAR(outMAR, wantFailure, ["mycert3"], false);
    },
    // Test importing to the second signature in a MAR that has multiple
    // signature
    test_import_sig_multiple: function _test_import_sig_multiple() {
      // Make sure the input MAR was signed with mycert only
      let inMAR = do_get_file("data/multiple_signed_pib.mar");
      verifyMAR(inMAR, wantSuccess, ["mycert", "mycert2", "mycert3"], false);
      verifyMAR(inMAR, wantFailure, ["mycert", "mycert", "mycert3"], false);

      // Get the signature file for this MAR signed with the key from mycert
      let sigFile = do_get_file("data/multiple_signed_pib_mar.sig.0");
      Assert.ok(sigFile.exists());
      let outMAR = tempDir.clone();
      outMAR.append("sigchanged_signed_pib.mar");
      if (outMAR.exists()) {
        outMAR.remove(false);
      }

      // Run the import operation
      const secondSigPos = 1;
      importMARSignature(inMAR, secondSigPos, sigFile, outMAR, wantSuccess);

      // Verify we have a new MAR file and that mycert no longer verifies
      // and that mycert2 does verify
      Assert.ok(outMAR.exists());
      verifyMAR(outMAR, wantSuccess, ["mycert", "mycert", "mycert3"], false);
      verifyMAR(outMAR, wantFailure, ["mycert", "mycert2", "mycert3"], false);

      // Compare the binary data to something that was signed originally
      // with the private keys from mycert, mycert, mycert3
      let refMAR = do_get_file("data/multiple_signed_pib_2.mar");
      Assert.ok(refMAR.exists());
      let refMARData = getBinaryFileData(refMAR);
      let outMARData = getBinaryFileData(outMAR);
      compareBinaryData(outMARData, refMARData);
    },
    // Test stripping a MAR that doesn't exist fails
    test_bad_path_strip_fails: function _test_bad_path_strip_fails() {
      let noMAR = do_get_file("data/does_not_exist.mar", true);
      Assert.ok(!noMAR.exists());
      let outMAR = tempDir.clone();
      outMAR.append("out.mar");
      stripMARSignature(noMAR, outMAR, wantFailure);
    },
    // Test extracting from a bad path fails
    test_extract_bad_path: function _test_extract_bad_path() {
      let noMAR = do_get_file("data/does_not_exist.mar", true);
      let extractedSig = do_get_file("extracted_signature", true);
      Assert.ok(!noMAR.exists());
      if (extractedSig.exists()) {
        extractedSig.remove(false);
      }
      extractMARSignature(noMAR, 0, extractedSig, wantFailure);
      Assert.ok(!extractedSig.exists());
    },
    // Between each test make sure the out MAR does not exist.
    cleanup_per_test: function _cleanup_per_test() {
    }
  };

  cleanup();

  // Run all the tests
  Assert.equal(run_tests(tests), Object.keys(tests).length - 1);

  do_register_cleanup(cleanup);
}

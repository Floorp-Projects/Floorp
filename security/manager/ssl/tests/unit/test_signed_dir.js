// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that signed extensions extracted/unpacked into a directory do not pass
// signature verification, because that's no longer supported.

const { ZipUtils } = ChromeUtils.import("resource://gre/modules/ZipUtils.jsm", {});

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

/**
 * Signed test extension. This is any arbitrary Mozilla signed XPI that
 * preferably has recently been signed (but note that it actually doesn't
 * matter, since we ignore expired certificates when checking signing).
 * @type nsIFile
 */
var gSignedXPI =
  do_get_file("test_signed_dir/lightbeam_for_firefox-1.3.1-fx.xpi", false);
/**
 * The directory that the test extension will be extracted to.
 * @type nsIFile
 */
var gTarget = FileUtils.getDir("TmpD", ["test_signed_dir"]);
gTarget.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

/**
 * Extracts the signed XPI into a directory, and tampers the files in that
 * directory if instructed.
 *
 * @returns {nsIFile}
 *          The directory where the XPI was extracted to.
 */
function prepare() {
  ZipUtils.extractFiles(gSignedXPI, gTarget);
  return gTarget;
}

function checkResult(expectedRv, dir, resolve) {
  return function verifySignedDirCallback(rv, aSignerCert) {
    equal(rv, expectedRv, "Actual and expected return value should match");
    equal(aSignerCert != null, Components.isSuccessCode(expectedRv),
          "expecting certificate:");
    dir.remove(true);
    resolve();
  };
}

function verifyDirAsync(expectedRv) {
  let targetDir = prepare();
  return new Promise((resolve, reject) => {
    certdb.verifySignedDirectoryAsync(
      Ci.nsIX509CertDB.AddonsPublicRoot, targetDir,
      checkResult(expectedRv, targetDir, resolve));
  });
}

add_task(async function testAPIFails() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED);
});

registerCleanupFunction(function() {
  if (gTarget.exists()) {
    gTarget.remove(true);
  }
});

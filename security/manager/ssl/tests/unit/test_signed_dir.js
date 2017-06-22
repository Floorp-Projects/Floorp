// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that signed extensions extracted/unpacked into a directory work pass
// verification when non-tampered, and fail verification when tampered via
// various means.

const { ZipUtils } = Cu.import("resource://gre/modules/ZipUtils.jsm", {});

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
 * Each property below is optional. Defining none of them means "don't tamper".
 *
 * @typedef {TamperInstructions}
 * @type Object
 * @property {String[][]} copy
 *           Format: [[path,newname], [path2,newname2], ...]
 *           Copy the file located at |path| and name it |newname|.
 * @property {String[]} delete
 *           List of paths to files to delete.
 * @property {String[]} corrupt
 *           List of paths to files to corrupt.
 */

/**
 * Extracts the signed XPI into a directory, and tampers the files in that
 * directory if instructed.
 *
 * @param {TamperInstructions} tamper
 *        Instructions on whether to tamper any files, and if so, how.
 * @returns {nsIFile}
 *          The directory where the XPI was extracted to.
 */
function prepare(tamper) {
  ZipUtils.extractFiles(gSignedXPI, gTarget);

  // copy files
  if (tamper.copy) {
    tamper.copy.forEach(i => {
      let f = gTarget.clone();
      i[0].split("/").forEach(seg => { f.append(seg); });
      f.copyTo(null, i[1]);
    });
  }

  // delete files
  if (tamper.delete) {
    tamper.delete.forEach(i => {
      let f = gTarget.clone();
      i.split("/").forEach(seg => { f.append(seg); });
      f.remove(true);
    });
  }

  // corrupt files
  if (tamper.corrupt) {
    tamper.corrupt.forEach(i => {
      let f = gTarget.clone();
      i.split("/").forEach(seg => { f.append(seg); });
      let s = FileUtils.openFileOutputStream(f, FileUtils.MODE_WRONLY);
      const str = "Kilroy was here";
      s.write(str, str.length);
      s.close();
    });
  }

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

function verifyDirAsync(expectedRv, tamper) {
  let targetDir = prepare(tamper);
  return new Promise((resolve, reject) => {
    certdb.verifySignedDirectoryAsync(
      Ci.nsIX509CertDB.AddonsPublicRoot, targetDir,
      checkResult(expectedRv, targetDir, resolve));
  });
}

//
// the tests
//
add_task(async function testValid() {
  await verifyDirAsync(Cr.NS_OK, {} /* no tampering */);
});

add_task(async function testNoMetaDir() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED,
                       {delete: ["META-INF"]});
});

add_task(async function testEmptyMetaDir() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED,
                       {delete: ["META-INF/mozilla.rsa",
                                 "META-INF/mozilla.sf",
                                 "META-INF/manifest.mf"]});
});

add_task(async function testTwoRSAFiles() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {copy: [["META-INF/mozilla.rsa", "extra.rsa"]]});
});

add_task(async function testCorruptRSAFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {corrupt: ["META-INF/mozilla.rsa"]});
});

add_task(async function testMissingSFFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {delete: ["META-INF/mozilla.sf"]});
});

add_task(async function testCorruptSFFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {corrupt: ["META-INF/mozilla.sf"]});
});

add_task(async function testExtraInvalidSFFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                       {copy: [["META-INF/mozilla.rsa", "extra.sf"]]});
});

add_task(async function testExtraValidSFFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                       {copy: [["META-INF/mozilla.sf", "extra.sf"]]});
});

add_task(async function testMissingManifest() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {delete: ["META-INF/manifest.mf"]});
});

add_task(async function testCorruptManifest() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                       {corrupt: ["META-INF/manifest.mf"]});
});

add_task(async function testMissingFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING,
                       {delete: ["bootstrap.js"]});
});

add_task(async function testCorruptFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY,
                       {corrupt: ["bootstrap.js"]});
});

add_task(async function testExtraFile() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                       {copy: [["bootstrap.js", "extra"]]});
});

add_task(async function testMissingFileInDir() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING,
                       {delete: ["lib/ui.js"]});
});

add_task(async function testCorruptFileInDir() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY,
                       {corrupt: ["lib/ui.js"]});
});

add_task(async function testExtraFileInDir() {
  await verifyDirAsync(Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                       {copy: [["lib/ui.js", "extra"]]});
});

do_register_cleanup(function() {
  if (gTarget.exists()) {
    gTarget.remove(true);
  }
});

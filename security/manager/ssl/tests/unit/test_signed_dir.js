"use strict";

Components.utils.import("resource://gre/modules/ZipUtils.jsm");

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

var gSignedXPI = do_get_file("test_signed_apps/sslcontrol.xpi", false);
var gTarget = FileUtils.getDir("TmpD", ["test_signed_dir"]);
gTarget.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);


// tamper data structure, each element is optional
// { copy:    [[path,newname], [path2,newname2], ...],
//   delete:  [path, path2, ...],
//   corrupt: [path, path2, ...]
// }

function prepare(tamper) {
  ZipUtils.extractFiles(gSignedXPI, gTarget);

  // copy files
  if (tamper.copy) {
    tamper.copy.forEach(i => {
      let f = gTarget.clone();
      i[0].split('/').forEach(seg => {f.append(seg);});
      f.copyTo(null, i[1]);
    });
  }

  // delete files
  if (tamper.delete) {
    tamper.delete.forEach(i => {
      let f = gTarget.clone();
      i.split('/').forEach(seg => {f.append(seg);});
      f.remove(true);
    });
  }

  // corrupt files
  if (tamper.corrupt) {
    tamper.corrupt.forEach(i => {
      let f = gTarget.clone();
      i.split('/').forEach(seg => {f.append(seg);});
      let s = FileUtils.openFileOutputStream(f, FileUtils.MODE_WRONLY);
      const str = "Kilroy was here";
      s.write(str, str.length);
      s.close();
    });
  }

  return gTarget;
}


function check_result(name, expectedRv, dir) {
  return function verifySignedDirCallback(rv, aSignerCert) {
    equal(rv, expectedRv, name + " rv:");
    equal(aSignerCert != null, Components.isSuccessCode(expectedRv),
          "expecting certificate:");
    // cleanup and kick off next test
    dir.remove(true);
    run_next_test();
  };
}

function verifyDirAsync(name, expectedRv, tamper) {
  let targetDir = prepare(tamper);
  certdb.verifySignedDirectoryAsync(
    Ci.nsIX509CertDB.AddonsPublicRoot, targetDir,
    check_result(name, expectedRv, targetDir));
}


//
// the tests
//

add_test(function() {
  verifyDirAsync("'valid'", Cr.NS_OK, {} /* no tampering */ );
});

add_test(function() {
  verifyDirAsync("'no meta dir'", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED,
                 {delete: ["META-INF"]});
});

add_test(function() {
  verifyDirAsync("'empty meta dir'", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED,
                 {delete: ["META-INF/mozilla.rsa",
                           "META-INF/mozilla.sf",
                           "META-INF/manifest.mf"]});
});

add_test(function() {
  verifyDirAsync("'two rsa files'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {copy: [["META-INF/mozilla.rsa","extra.rsa"]]});
});

add_test(function() {
  verifyDirAsync("'corrupt rsa file'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {corrupt: ["META-INF/mozilla.rsa"]});
});

add_test(function() {
  verifyDirAsync("'missing sf file'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {delete: ["META-INF/mozilla.sf"]});
});

add_test(function() {
  verifyDirAsync("'corrupt sf file'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {corrupt: ["META-INF/mozilla.sf"]});
});

add_test(function() {
  verifyDirAsync("'extra .sf file (invalid)'", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                 {copy: [["META-INF/mozilla.rsa","extra.sf"]]});
});

add_test(function() {
  verifyDirAsync("'extra .sf file (valid)'", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                 {copy: [["META-INF/mozilla.sf","extra.sf"]]});
});

add_test(function() {
  verifyDirAsync("'missing manifest'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {delete: ["META-INF/manifest.mf"]});
});

add_test(function() {
  verifyDirAsync("'corrupt manifest'", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,
                 {corrupt: ["META-INF/manifest.mf"]});
});

add_test(function() {
  verifyDirAsync("'missing file'", Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING,
                 {delete: ["bootstrap.js"]});
});

add_test(function() {
  verifyDirAsync("'corrupt file'", Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY,
                 {corrupt: ["bootstrap.js"]});
});

add_test(function() {
  verifyDirAsync("'extra file'", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                 {copy: [["bootstrap.js","extra"]]});
});

add_test(function() {
  verifyDirAsync("'missing file in dir'", Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING,
                 {delete: ["content/options.xul"]});
});

add_test(function() {
  verifyDirAsync("'corrupt file in dir'", Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY,
                 {corrupt: ["content/options.xul"]});
});

add_test(function() {
  verifyDirAsync("'extra file in dir'", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,
                 {copy: [["content/options.xul","extra"]]});
});

do_register_cleanup(function() {
  if (gTarget.exists()) {
    gTarget.remove(true);
  }
});

function run_test() {
  run_next_test();
}
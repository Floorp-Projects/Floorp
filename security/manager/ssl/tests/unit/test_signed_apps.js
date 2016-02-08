"use strict";
/* To regenerate the certificates and apps for this test:

        cd security/manager/ssl/tests/unit/test_signed_apps
        PATH=$NSS/bin:$NSS/lib:$PATH ./generate.sh
        cd ../../../../../..
        make -C $OBJDIR/security/manager/ssl/tests

   $NSS is the path to NSS binaries and libraries built for the host platform.
   If you get error messages about "CertUtil" on Windows, then it means that
   the Windows CertUtil.exe is ahead of the NSS certutil.exe in $PATH.

   Check in the generated files. These steps are not done as part of the build
   because we do not want to add a build-time dependency on the OpenSSL or NSS
   tools or libraries built for the host platform.
*/

// XXX from prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);

// Creates a new app package based in the inFilePath package, with a set of
// modifications (including possibly deletions) applied to the existing entries,
// and/or a set of new entries to be included.
function tamper(inFilePath, outFilePath, modifications, newEntries) {
  var writer = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  writer.open(outFilePath, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  try {
    var reader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(Ci.nsIZipReader);
    reader.open(inFilePath);
    try {
      var entries = reader.findEntries("");
      while (entries.hasMore()) {
        var entryName = entries.getNext();
        var inEntry = reader.getEntry(entryName);
        var entryInput = reader.getInputStream(entryName);
        try {
          var f = modifications[entryName];
          var outEntry, outEntryInput;
          if (f) {
            [outEntry, outEntryInput] = f(inEntry, entryInput);
            delete modifications[entryName];
          } else {
            [outEntry, outEntryInput] = [inEntry, entryInput];
          }
          // if f does not want the input entry to be copied to the output entry
          // at all (i.e. it wants it to be deleted), it will return null.
          if (outEntryInput) {
            try {
              writer.addEntryStream(entryName,
                                    outEntry.lastModifiedTime,
                                    outEntry.compression,
                                    outEntryInput,
                                    false);
            } finally {
              if (entryInput != outEntryInput)
                outEntryInput.close();
            }
          }
        } finally {
          entryInput.close();
        }
      }
    } finally {
      reader.close();
    }

    // Any leftover modification means that we were expecting to modify an entry
    // in the input file that wasn't there.
    for(var name in modifications) {
      if (modifications.hasOwnProperty(name)) {
        throw "input file was missing expected entries: " + name;
      }
    }

    // Now, append any new entries to the end
    newEntries.forEach(function(newEntry) {
      var sis = Cc["@mozilla.org/io/string-input-stream;1"]
                  .createInstance(Ci.nsIStringInputStream);
      try {
        sis.setData(newEntry.content, newEntry.content.length);
        writer.addEntryStream(newEntry.name,
                              new Date(),
                              Ci.nsIZipWriter.COMPRESSION_BEST,
                              sis,
                              false);
      } finally {
        sis.close();
      }
    });
  } finally {
    writer.close();
  }
}

function removeEntry(entry, entryInput) { return [null, null]; }

function truncateEntry(entry, entryInput) {
  if (entryInput.available() == 0)
    throw "Truncating already-zero length entry will result in identical entry.";

  var content = Cc["@mozilla.org/io/string-input-stream;1"]
                  .createInstance(Ci.nsIStringInputStream);
  content.data = "";

  return [entry, content]
}

function run_test() {
  run_next_test();
}

function check_open_result(name, expectedRv) {
  return function openSignedAppFileCallback(rv, aZipReader, aSignerCert) {
    do_print("openSignedAppFileCallback called for " + name);
    equal(rv, expectedRv, "Actual and expected return value should match");
    equal(aZipReader != null, Components.isSuccessCode(expectedRv),
          "ZIP reader should be null only if the return value denotes failure");
    equal(aSignerCert != null, Components.isSuccessCode(expectedRv),
          "Signer cert should be null only if the return value denotes failure");
    run_next_test();
  };
}

function original_app_path(test_name) {
  return do_get_file("test_signed_apps/" + test_name + ".zip", false);
}

function tampered_app_path(test_name) {
  return FileUtils.getFile("TmpD", ["test_signed_app-" + test_name + ".zip"]);
}

add_test(function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("valid_app_1"),
    check_open_result("valid", Cr.NS_OK));
});

add_test(function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("unsigned_app_1"),
    check_open_result("unsigned", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED));
});

add_test(function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("unknown_issuer_app_1"),
    check_open_result("unknown_issuer",
                      getXPCOMStatusFromNSS(SEC_ERROR_UNKNOWN_ISSUER)));
});

// Sanity check to ensure a no-op tampering gives a valid result
add_test(function () {
  var tampered = tampered_app_path("identity_tampering");
  tamper(original_app_path("valid_app_1"), tampered, { }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("valid_app_1"),
    check_open_result("identity_tampering", Cr.NS_OK));
});

add_test(function () {
  var tampered = tampered_app_path("missing_rsa");
  tamper(original_app_path("valid_app_1"), tampered, { "META-INF/A.RSA" : removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_rsa", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED));
});

add_test(function () {
  var tampered = tampered_app_path("missing_sf");
  tamper(original_app_path("valid_app_1"), tampered, { "META-INF/A.SF" : removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_sf", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID));
});

add_test(function () {
  var tampered = tampered_app_path("missing_manifest_mf");
  tamper(original_app_path("valid_app_1"), tampered, { "META-INF/MANIFEST.MF" : removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_manifest_mf",
                      Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID));
});

add_test(function () {
  var tampered = tampered_app_path("missing_entry");
  tamper(original_app_path("valid_app_1"), tampered, { "manifest.webapp" : removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_entry", Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING));
});

add_test(function () {
  var tampered = tampered_app_path("truncated_entry");
  tamper(original_app_path("valid_app_1"), tampered, { "manifest.webapp" : truncateEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("truncated_entry", Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY));
});

add_test(function () {
  var tampered = tampered_app_path("unsigned_entry");
  tamper(original_app_path("valid_app_1"), tampered, {},
    [ { "name": "unsigned.txt", "content": "unsigned content!" } ]);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("unsigned_entry", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY));
});

add_test(function () {
  var tampered = tampered_app_path("unsigned_metainf_entry");
  tamper(original_app_path("valid_app_1"), tampered, {},
    [ { name: "META-INF/unsigned.txt", content: "unsigned content!" } ]);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("unsigned_metainf_entry",
                      Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY));
});

// TODO: tampered MF, tampered SF
// TODO: too-large MF, too-large RSA, too-large SF
// TODO: MF and SF that end immediately after the last main header
//       (no CR nor LF)
// TODO: broken headers to exercise the parser

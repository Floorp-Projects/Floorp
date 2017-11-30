"use strict";

// Tests the API nsIX509CertDB.openSignedAppFileAsync, which backs add-on
// signature verification. Testcases include various ways of tampering with
// add-ons as well as different hash algorithms used in the various
// signature/metadata files.

// from prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

// Creates a new app package based in the inFilePath package, with a set of
// modifications (including possibly deletions) applied to the existing entries,
// and/or a set of new entries to be included.
function tamper(inFilePath, outFilePath, modifications, newEntries) {
  let writer = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  writer.open(outFilePath, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  try {
    let reader = Cc["@mozilla.org/libjar/zip-reader;1"]
                   .createInstance(Ci.nsIZipReader);
    reader.open(inFilePath);
    try {
      let entries = reader.findEntries("");
      while (entries.hasMore()) {
        let entryName = entries.getNext();
        let inEntry = reader.getEntry(entryName);
        let entryInput = reader.getInputStream(entryName);
        try {
          let f = modifications[entryName];
          let outEntry, outEntryInput;
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
              if (entryInput != outEntryInput) {
                outEntryInput.close();
              }
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
    for (let name in modifications) {
      if (modifications.hasOwnProperty(name)) {
        throw new Error("input file was missing expected entries: " + name);
      }
    }

    // Now, append any new entries to the end
    newEntries.forEach(function(newEntry) {
      let sis = Cc["@mozilla.org/io/string-input-stream;1"]
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
  if (entryInput.available() == 0) {
    throw new Error("Truncating already-zero length entry will result in " +
                    "identical entry.");
  }

  let content = Cc["@mozilla.org/io/string-input-stream;1"]
                  .createInstance(Ci.nsIStringInputStream);
  content.data = "";

  return [entry, content];
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

var hashTestcases = [
  // SHA-256 in PKCS#7 + SHA-256 present elsewhere => OK
  { name: "app_mf-1-256_sf-1-256_p7-1-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1-256_sf-1-256_p7-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1-256_sf-256_p7-1-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1-256_sf-256_p7-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-256_sf-1-256_p7-1-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-256_sf-1-256_p7-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-256_sf-256_p7-1-256",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-256_sf-256_p7-256",
    expectedResult: Cr.NS_OK },

  // SHA-1 in PKCS#7 + SHA-1 present elsewhere => OK
  { name: "app_mf-1-256_sf-1-256_p7-1",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1-256_sf-1_p7-1",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1_sf-1-256_p7-1",
    expectedResult: Cr.NS_OK },
  { name: "app_mf-1_sf-1_p7-1",
    expectedResult: Cr.NS_OK },

  // SHA-256 in PKCS#7 + SHA-256 not present elsewhere => INVALID
  { name: "app_mf-1-256_sf-1_p7-1-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1-256_sf-1_p7-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-1-256_p7-1-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-1-256_p7-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-1_p7-1-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-1_p7-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-256_p7-1-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-256_p7-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-256_sf-1_p7-1-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-256_sf-1_p7-256",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },

  // SHA-1 in PKCS#7 + SHA-1 not present elsewhere => INVALID
  { name: "app_mf-1-256_sf-256_p7-1",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-1_sf-256_p7-1",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-256_sf-1-256_p7-1",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-256_sf-1_p7-1",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
  { name: "app_mf-256_sf-256_p7-1",
    expectedResult: Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID },
];

// Policy values for the preference "security.signed_app_signatures.policy"
const PKCS7WithSHA1OrSHA256 = 0;
const PKCS7WithSHA256 = 1;

function add_signature_test(policy, test) {
  // First queue up a test to set the desired policy:
  add_test(function () {
    Services.prefs.setIntPref("security.signed_app_signatures.policy", policy);
    run_next_test();
  });
  // Then queue up the test itself:
  add_test(test);
}

for (let testcase of hashTestcases) {
  add_signature_test(PKCS7WithSHA1OrSHA256, function () {
    certdb.openSignedAppFileAsync(
      Ci.nsIX509CertDB.AppXPCShellRoot,
      original_app_path(testcase.name),
      check_open_result(testcase.name, testcase.expectedResult));
  });
}

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot,
    original_app_path("empty_signerInfos"),
    check_open_result("the signerInfos in the PKCS#7 signature is empty",
                      Cr.NS_ERROR_CMS_VERIFY_NOT_SIGNED));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("unsigned_app"),
    check_open_result("unsigned", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("unknown_issuer_app"),
    check_open_result("unknown_issuer",
                      getXPCOMStatusFromNSS(SEC_ERROR_UNKNOWN_ISSUER)));
});

// Sanity check to ensure a no-op tampering gives a valid result
add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("identity_tampering");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered, { }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, original_app_path("app_mf-1_sf-1_p7-1"),
    check_open_result("identity_tampering", Cr.NS_OK));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("missing_rsa");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/A.RSA": removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_rsa", Cr.NS_ERROR_SIGNED_JAR_NOT_SIGNED));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("missing_sf");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/A.SF": removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_sf", Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("missing_manifest_mf");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/MANIFEST.MF": removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_manifest_mf",
                      Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("missing_entry");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "manifest.json": removeEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("missing_entry", Cr.NS_ERROR_SIGNED_JAR_ENTRY_MISSING));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("truncated_entry");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "manifest.json": truncateEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("truncated_entry",
                      Cr.NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("truncated_manifestFile");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/MANIFEST.MF": truncateEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("truncated_manifestFile",
                      Cr.NS_ERROR_SIGNED_JAR_MANIFEST_INVALID));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("truncated_signatureFile");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/A.SF": truncateEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("truncated_signatureFile",
                      getXPCOMStatusFromNSS(SEC_ERROR_PKCS7_BAD_SIGNATURE)));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("truncated_pkcs7File");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered,
         { "META-INF/A.RSA": truncateEntry }, []);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("truncated_pkcs7File",
                      Cr.NS_ERROR_CMS_VERIFY_NOT_SIGNED));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("unsigned_entry");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered, {},
    [ { "name": "unsigned.txt", "content": "unsigned content!" } ]);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("unsigned_entry", Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY));
});

add_signature_test(PKCS7WithSHA1OrSHA256, function () {
  let tampered = tampered_app_path("unsigned_metainf_entry");
  tamper(original_app_path("app_mf-1_sf-1_p7-1"), tampered, {},
    [ { name: "META-INF/unsigned.txt", content: "unsigned content!" } ]);
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot, tampered,
    check_open_result("unsigned_metainf_entry",
                      Cr.NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY));
});

add_signature_test(PKCS7WithSHA256, function testSHA1Disabled() {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot,
    original_app_path("app_mf-1_sf-1_p7-1"),
    check_open_result("SHA-1 should not be accepted if disabled by policy",
                      Cr.NS_ERROR_SIGNED_JAR_WRONG_SIGNATURE));
});

add_signature_test(PKCS7WithSHA256, function testSHA256WorksWithSHA1Disabled() {
  certdb.openSignedAppFileAsync(
    Ci.nsIX509CertDB.AppXPCShellRoot,
    original_app_path("app_mf-256_sf-256_p7-256"),
    check_open_result("SHA-256 should work if SHA-1 is disabled by policy",
                      Cr.NS_OK));
});

add_signature_test(PKCS7WithSHA256,
  function testMultipleSignaturesWorkWithSHA1Disabled() {
    certdb.openSignedAppFileAsync(
      Ci.nsIX509CertDB.AppXPCShellRoot,
      original_app_path("app_mf-1-256_sf-1-256_p7-1-256"),
      check_open_result("Multiple signatures should work if SHA-1 is " +
                        "disabled by policy (if SHA-256 signature verifies)",
                        Cr.NS_OK));
});

// TODO: tampered MF, tampered SF
// TODO: too-large MF, too-large RSA, too-large SF
// TODO: MF and SF that end immediately after the last main header
//       (no CR nor LF)
// TODO: broken headers to exercise the parser

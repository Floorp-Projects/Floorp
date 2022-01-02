/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell genRootCAHashes.js \
//                                  [absolute path to]/RootHashes.inc'

const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const CertDb = Cc[nsX509CertDB].getService(Ci.nsIX509CertDB);

const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

const FILENAME_OUTPUT = "RootHashes.inc";
const FILENAME_TRUST_ANCHORS = "KnownRootHashes.json";
const ROOT_NOT_ASSIGNED = -1;

const JSON_HEADER = `// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/. */
//
//***************************************************************************
// This is an automatically generated file. It's used to maintain state for
// runs of genRootCAHashes.js; you should never need to manually edit it
//***************************************************************************

// Notes:
// binNumber 1 used to be for "GTE_CyberTrust_Global_Root", but that root was
// removed from the built-in roots module, so now it is used to indicate that
// the certificate is not a built-in and was found in the softoken (cert9.db).

// binNumber 2 used to be for "Thawte_Server_CA", but that root was removed from
// the built-in roots module, so now it is used to indicate that the certificate
// is not a built-in and was found on an external PKCS#11 token.

// binNumber 3 used to be for "Thawte_Premium_Server_CA", but that root was
// removed from the built-in roots module, so now it is used to indicate that
// the certificate is not a built-in and was temporarily imported from the OS as
// part of the "Enterprise Roots" feature.

`;

const FILE_HEADER =
  "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
  " * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
  " * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
  "\n" +
  "/*****************************************************************************/\n" +
  "/* This is an automatically generated file. If you're not                    */\n" +
  "/* RootCertificateTelemetryUtils.cpp, you shouldn't be #including it.        */\n" +
  "/*****************************************************************************/\n" +
  "\n" +
  "#define HASH_LEN 32\n";

const FP_PREAMBLE =
  "struct CertAuthorityHash {\n" +
  "  // See bug 1338873 about making these fields const.\n" +
  "  uint8_t hash[HASH_LEN];\n" +
  "  int32_t binNumber;\n" +
  "};\n\n" +
  "static const struct CertAuthorityHash ROOT_TABLE[] = {\n";

const FP_POSTAMBLE = "};\n";

// Helper
function writeString(fos, string) {
  fos.write(string, string.length);
}

// Remove all colons from a string
function stripColons(hexString) {
  return hexString.replace(/:/g, "");
}

// Expect an array of bytes and make it C-formatted
function hexSlice(bytes, start, end) {
  let ret = "";
  for (let i = start; i < end; i++) {
    let hex = (0 + bytes.charCodeAt(i).toString(16)).slice(-2).toUpperCase();
    ret += "0x" + hex;
    if (i < end - 1) {
      ret += ", ";
    }
  }
  return ret;
}

function stripComments(buf) {
  let lines = buf.split("\n");
  let entryRegex = /^\s*\/\//;
  let data = "";
  for (let i = 0; i < lines.length; i++) {
    let match = entryRegex.exec(lines[i]);
    if (!match) {
      data = data + lines[i];
    }
  }
  return data;
}

// Load the trust anchors JSON object from disk
function loadTrustAnchors(file) {
  if (file.exists()) {
    let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
      Ci.nsIFileInputStream
    );
    stream.init(file, -1, 0, 0);
    let buf = NetUtil.readInputStreamToString(stream, stream.available());
    return JSON.parse(stripComments(buf));
  }
  // If there's no input file, bootstrap.
  return { roots: [], maxBin: 0 };
}

// Saves our persistence file so that we don't lose track of the mapping
// between bin numbers and the CA-hashes, even as CAs come and go.
function writeTrustAnchors(file) {
  let fos = FileUtils.openSafeFileOutputStream(file);

  let serializedData = JSON.stringify(gTrustAnchors, null, "  ");
  fos.write(JSON_HEADER, JSON_HEADER.length);
  fos.write(serializedData, serializedData.length);

  FileUtils.closeSafeFileOutputStream(fos);
}

// Write the C++ header file
function writeRootHashes(fos) {
  try {
    writeString(fos, FILE_HEADER);

    // Output the sorted gTrustAnchors
    writeString(fos, FP_PREAMBLE);
    gTrustAnchors.roots.forEach(function(fp) {
      let fpBytes = atob(fp.sha256Fingerprint);

      writeString(fos, "  {\n");
      writeString(fos, "    /* " + fp.label + " */\n");
      writeString(fos, "    { " + hexSlice(fpBytes, 0, 16) + ",\n");
      writeString(fos, "      " + hexSlice(fpBytes, 16, 32) + " },\n");
      writeString(fos, "      " + fp.binNumber + " /* Bin Number */\n");

      writeString(fos, "  },\n");
    });
    writeString(fos, FP_POSTAMBLE);

    writeString(fos, "\n");
  } catch (e) {
    dump("ERROR: problem writing output: " + e + "\n");
  }
}

// Scan our list (linearly) for the given fingerprint string
function findTrustAnchorByFingerprint(sha256Fingerprint) {
  for (let i = 0; i < gTrustAnchors.roots.length; i++) {
    if (sha256Fingerprint == gTrustAnchors.roots[i].sha256Fingerprint) {
      return i;
    }
  }
  return ROOT_NOT_ASSIGNED;
}

// Get a clean label for a given certificate; usually the common name.
function getLabelForCert(cert) {
  let label = cert.commonName;

  if (label.length < 5) {
    label = cert.subjectName;
  }

  // replace non-ascii characters
  label = label.replace(/[^[:ascii:]]/g, "_");
  // replace non-word characters
  label = label.replace(/[^A-Za-z0-9]/g, "_");
  return label;
}

// Fill in the gTrustAnchors list with trust anchors from the database.
function insertTrustAnchorsFromDatabase() {
  // We only want CA certs for SSL
  const CERT_TYPE = Ci.nsIX509Cert.CA_CERT;
  const TRUST_TYPE = Ci.nsIX509CertDB.TRUSTED_SSL;

  // Iterate through the whole Cert DB
  for (let cert of CertDb.getCerts()) {
    // Find the certificate in our existing list. Do it here because we need to check if
    // it's untrusted too.

    // If this is a trusted cert
    if (CertDb.isCertTrusted(cert, CERT_TYPE, TRUST_TYPE)) {
      // Base64 encode the hex string
      let binaryFingerprint = CommonUtils.hexToBytes(
        stripColons(cert.sha256Fingerprint)
      );
      let encodedFingerprint = btoa(binaryFingerprint);

      // Scan to see if this is already in the database.
      if (
        findTrustAnchorByFingerprint(encodedFingerprint) == ROOT_NOT_ASSIGNED
      ) {
        // Let's get a usable name; some old certs do not have CN= filled out
        let label = getLabelForCert(cert);

        // Add to list
        gTrustAnchors.maxBin += 1;
        gTrustAnchors.roots.push({
          label,
          binNumber: gTrustAnchors.maxBin,
          sha256Fingerprint: encodedFingerprint,
        });
      }
    }
  }
}

//
//  PRIMARY LOGIC
//

if (arguments.length != 1) {
  throw new Error(
    "Usage: genRootCAHashes.js <absolute path to current RootHashes.inc>"
  );
}

var trustAnchorsFile = FileUtils.getFile("CurWorkD", [FILENAME_TRUST_ANCHORS]);
var rootHashesFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
rootHashesFile.initWithPath(arguments[0]);

// Open the known hashes file; this is to ensure stable bin numbers.
var gTrustAnchors = loadTrustAnchors(trustAnchorsFile);

// Collect all certificate entries
insertTrustAnchorsFromDatabase();

// Update known hashes before we sort
writeTrustAnchors(trustAnchorsFile);

// Sort all trust anchors before writing, as AccumulateRootCA.cpp
// will perform binary searches
gTrustAnchors.roots.sort(function(a, b) {
  // We need to work from the binary values, not the base64 values.
  let aBin = atob(a.sha256Fingerprint);
  let bBin = atob(b.sha256Fingerprint);

  if (aBin < bBin) {
    return -1;
  }
  if (aBin > bBin) {
    return 1;
  }
  return 0;
});

// Write the output file.
var rootHashesFileOutputStream = FileUtils.openSafeFileOutputStream(
  rootHashesFile
);
writeRootHashes(rootHashesFileOutputStream);
FileUtils.closeSafeFileOutputStream(rootHashesFileOutputStream);

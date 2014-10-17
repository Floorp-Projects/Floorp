/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// How to run this file:
// 1. [obtain firefox source code]
// 2. [build/obtain firefox binaries]
// 3. run `[path to]/run-mozilla.sh [path to]/xpcshell genRootCAHashes.js \
//                                  [absolute path to]/RootHashes.inc'

// <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
// <https://bugzilla.mozilla.org/show_bug.cgi?id=546628>
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const CertDb = Components.classes[nsX509CertDB].getService(Ci.nsIX509CertDB);

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://services-common/utils.js");

const FILENAME_OUTPUT = "RootHashes.inc";
const FILENAME_TRUST_ANCHORS = "KnownRootHashes.json";
const ROOT_NOT_ASSIGNED = -1;

const JSON_HEADER = "// This Source Code Form is subject to the terms of the Mozilla Public\n" +
"// License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
"// file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"//\n" +
"//***************************************************************************\n" +
"// This is an automatically generated file. It's used to maintain state for\n" +
"// runs of genRootCAHashes.js; you should never need to manually edit it\n" +
"//***************************************************************************\n" +
"\n";

const FILE_HEADER = "/* This Source Code Form is subject to the terms of the Mozilla Public\n" +
" * License, v. 2.0. If a copy of the MPL was not distributed with this\n" +
" * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n" +
"\n" +
"/*****************************************************************************/\n" +
"/* This is an automatically generated file. If you're not                    */\n" +
"/* RootCertificateTelemetryUtils.cpp, you shouldn't be #including it.        */\n" +
"/*****************************************************************************/\n" +
"\n" +
"#define HASH_LEN 32\n";

const FP_PREAMBLE = "struct CertAuthorityHash {\n" +
" const uint8_t hash[HASH_LEN];\n" +
" const int32_t binNumber;\n" +
"};\n\n" +
"static const struct CertAuthorityHash ROOT_TABLE[] = {\n";

const FP_POSTAMBLE = "};\n";

// Helper
function writeString(fos, string) {
  fos.write(string, string.length);
}

// Remove all colons from a string
function stripColons(hexString) {
  return hexString.replace(/:/g, '');
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
    let stream = Cc["@mozilla.org/network/file-input-stream;1"]
                   .createInstance(Ci.nsIFileInputStream);
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

  let serializedData = JSON.stringify(gTrustAnchors, null, '  ');
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
      writeString(fos, "    /* "+fp.label+" */\n");
      writeString(fos, "    { " + hexSlice(fpBytes, 0, 16) + ",\n");
      writeString(fos, "      " + hexSlice(fpBytes, 16, 32) + " },\n");
      writeString(fos, "      " + fp.binNumber + " /* Bin Number */\n");

      writeString(fos, "  },\n");
    });
    writeString(fos, FP_POSTAMBLE);

    writeString(fos, "\n");

  }
  catch (e) {
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
  label = label.replace( /[^[:ascii:]]/g, "_");
  // replace non-word characters
  label = label.replace(/[^A-Za-z0-9]/g ,"_");
  return label;
}

// Fill in the gTrustAnchors list with trust anchors from the database.
function insertTrustAnchorsFromDatabase(){
  // We only want CA certs for SSL
  const CERT_TYPE = Ci.nsIX509Cert.CA_CERT;
  const TRUST_TYPE = Ci.nsIX509CertDB.TRUSTED_SSL;

  // Iterate through the whole Cert DB
  let enumerator = CertDb.getCerts().getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);

    // Find the certificate in our existing list. Do it here because we need to check if
    // it's untrusted too.

    // If this is a trusted cert
    if (CertDb.isCertTrusted(cert, CERT_TYPE, TRUST_TYPE)) {
      // Base64 encode the hex string
      let binaryFingerprint = CommonUtils.hexToBytes(stripColons(cert.sha256Fingerprint));
      let encodedFingerprint = btoa(binaryFingerprint);

       // Scan to see if this is already in the database.
      if (findTrustAnchorByFingerprint(encodedFingerprint) == ROOT_NOT_ASSIGNED) {

        // Let's get a usable name; some old certs do not have CN= filled out
        let label = getLabelForCert(cert);

        // Add to list
        gTrustAnchors.maxBin += 1;
        gTrustAnchors.roots.push(
          {
            "label": label,
            "binNumber": gTrustAnchors.maxBin,
            "sha256Fingerprint": encodedFingerprint
          });
      }
    }
  }
}

//
//  PRIMARY LOGIC
//

if (arguments.length < 1) {
  throw "Usage: genRootCAHashes.js <absolute path to current RootHashes.inc>";
}

let trustAnchorsFile = FileUtils.getFile("CurWorkD", [FILENAME_TRUST_ANCHORS]);
// let rootHashesFile = FileUtils.getFile("CurWorkD", arguments[0]);
let rootHashesFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
rootHashesFile.initWithPath(arguments[0]);

// Open the known hashes file; this is to ensure stable bin numbers.
let gTrustAnchors = loadTrustAnchors(trustAnchorsFile);

// Collect all certificate entries
insertTrustAnchorsFromDatabase();

// Update known hashes before we sort
writeTrustAnchors(trustAnchorsFile);

// Sort all trust anchors before writing, as AccumulateRootCA.cpp
// will perform binary searches
gTrustAnchors.roots.sort(function(a, b) {
  // We need to work from the binary values, not the base64 values.
  let aBin = atob(a.sha256Fingerprint);
  let bBin = atob(b.sha256Fingerprint)

  if (aBin < bBin)
     return -1;
  else if (aBin > bBin)
     return 1;
   else
     return 0;
});

// Write the output file.
let rootHashesFileOutputStream = FileUtils.openSafeFileOutputStream(rootHashesFile);
writeRootHashes(rootHashesFileOutputStream);
FileUtils.closeSafeFileOutputStream(rootHashesFileOutputStream);

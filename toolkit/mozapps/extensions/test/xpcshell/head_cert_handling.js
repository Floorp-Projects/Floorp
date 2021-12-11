// Helpers for handling certs.
// These are taken from
// https://searchfox.org/mozilla-central/rev/36aa22c7ea92bd3cf7910774004fff7e63341cf5/security/manager/ssl/tests/unit/head_psm.js
// but we don't want to drag that file in here because
// - it conflicts with `head_addons.js`.
// - it has a lot of extra code we don't need.
// So dupe relevant code here.

// This file will be included along with head_addons.js, use its globals.
/* import-globals-from head_addons.js */

"use strict";

// Convert a string to an array of bytes consisting of the char code at each
// index.
function stringToArray(s) {
  let a = [];
  for (let i = 0; i < s.length; i++) {
    a.push(s.charCodeAt(i));
  }
  return a;
}

// Commonly certificates are represented as PEM. The format is roughly as
// follows:
//
// -----BEGIN CERTIFICATE-----
// [some lines of base64, each typically 64 characters long]
// -----END CERTIFICATE-----
//
// However, nsIX509CertDB.constructX509FromBase64 and related functions do not
// handle input of this form. Instead, they require a single string of base64
// with no newlines or BEGIN/END headers. This is a helper function to convert
// PEM to the format that nsIX509CertDB requires.
function pemToBase64(pem) {
  return pem
    .replace(/-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----/, "")
    .replace(/[\r\n]/g, "");
}

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, 0);
  let available = fstream.available();
  let data =
    available > 0 ? NetUtil.readInputStreamToString(fstream, available) : "";
  fstream.close();
  return data;
}

function constructCertFromFile(filename) {
  let certFile = do_get_file(filename, false);
  let certBytes = readFile(certFile);
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // Certs should always be in .pem format, don't bother with .dem handling.
  return certdb.constructX509FromBase64(pemToBase64(certBytes));
}

function setCertRoot(filename) {
  let cert = constructCertFromFile(filename);
  Services.prefs.setCharPref(
    "security.content.signature.root_hash",
    cert.sha256Fingerprint
  );
}

function loadCertChain(prefix, names) {
  let chain = [];
  for (let name of names) {
    let filename = `${prefix}_${name}.pem`;
    chain.push(readFile(do_get_file(filename)));
  }
  return chain;
}

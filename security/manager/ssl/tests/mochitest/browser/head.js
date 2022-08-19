/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

/**
 * List of certs imported via readCertificate(). Certs in this list are
 * automatically deleted from the cert DB when a test including this head file
 * finishes.
 * @type nsIX509Cert[]
 */
var gImportedCerts = [];

registerCleanupFunction(() => {
  for (let cert of gImportedCerts) {
    gCertDB.deleteCertificate(cert);
  }
});

// This function serves the same purpose as the one defined in head_psm.js.
function pemToBase64(pem) {
  return pem
    .replace(/-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----/, "")
    .replace(/[\r\n]/g, "");
}

/**
 * Given the filename of a certificate, returns a promise that will resolve with
 * a handle to the certificate when that certificate has been read and imported
 * with the given trust settings.
 *
 * Certs imported via this function will automatically be deleted from the cert
 * DB once the calling test finishes.
 *
 * @param {String} filename
 *        The filename of the certificate (assumed to be in the same directory).
 * @param {String} trustString
 *        A string describing how the certificate should be trusted (see
 *        `certutil -A --help`).
 * @return {Promise}
 *         A promise that will resolve with a handle to the certificate.
 */
function readCertificate(filename, trustString) {
  return IOUtils.readUTF8(getTestFilePath(filename)).then(
    pem => {
      let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
        Ci.nsIX509CertDB
      );
      let base64 = pemToBase64(pem);
      certdb.addCertFromBase64(base64, trustString);
      let cert = certdb.constructX509FromBase64(base64);
      gImportedCerts.push(cert);
      return cert;
    },
    error => {
      throw error;
    }
  );
}

/**
 * Asynchronously opens the certificate manager.
 *
 * @return {Window} a handle on the opened certificate manager window
 */
async function openCertManager() {
  let win = window.openDialog("chrome://pippki/content/certManager.xhtml");
  return new Promise((resolve, reject) => {
    win.addEventListener(
      "load",
      function() {
        executeSoon(() => resolve(win));
      },
      { once: true }
    );
  });
}

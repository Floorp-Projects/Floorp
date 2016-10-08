/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * This function serves the same purpose as the one defined in head_psm.js.
 */
function pemToBase64(pem) {
  return pem.replace(/-----BEGIN CERTIFICATE-----/, "")
            .replace(/-----END CERTIFICATE-----/, "")
            .replace(/[\r\n]/g, "");
}

/**
 * Given the filename of a certificate, returns a promise that will resolve with
 * a handle to the certificate when that certificate has been read and imported
 * with the given trust settings.
 *
 * @param {String} filename
 *        The filename of the certificate (assumed to be in the same directory).
 * @param {String} trustString
 *        A string describing how the certificate should be trusted (see
 *        `certutil -A --help`).
 * @param {nsIX509Cert[]} certificates
 *        An array to append the imported cert to. Useful for making sure
 *        imported certs are cleaned up.
 * @return {Promise}
 *         A promise that will resolve with a handle to the certificate.
 */
function readCertificate(filename, trustString, certificates) {
  return OS.File.read(getTestFilePath(filename)).then(data => {
    let decoder = new TextDecoder();
    let pem = decoder.decode(data);
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
    let base64 = pemToBase64(pem);
    certdb.addCertFromBase64(base64, trustString, "unused");
    let cert = certdb.constructX509FromBase64(base64);
    certificates.push(cert);
    return cert;
  }, error => { throw error; });
}

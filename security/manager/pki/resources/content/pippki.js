/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/*
 * These are helper functions to be included
 * pippki UI js files.
 */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

function setText(id, value) {
  let element = document.getElementById(id);
  if (!element) {
    return;
  }
  if (element.hasChildNodes()) {
    element.firstChild.remove();
  }
  element.appendChild(document.createTextNode(value));
}

const nsICertificateDialogs = Ci.nsICertificateDialogs;
const nsCertificateDialogs = "@mozilla.org/nsCertificateDialogs;1";

function viewCertHelper(parent, cert) {
  if (!cert) {
    return;
  }

  var cd = Cc[nsCertificateDialogs].getService(nsICertificateDialogs);
  cd.viewCert(parent, cert);
}

function getDERString(cert) {
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = "";
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function getPKCS7String(cert, chainMode) {
  var length = {};
  var pkcs7Array = cert.exportAsCMS(chainMode, length);
  var pkcs7String = "";
  for (var i = 0; i < pkcs7Array.length; i++) {
    pkcs7String += String.fromCharCode(pkcs7Array[i]);
  }
  return pkcs7String;
}

function getPEMString(cert) {
  var derb64 = btoa(getDERString(cert));
  // Wrap the Base64 string into lines of 64 characters with CRLF line breaks
  // (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return "-----BEGIN CERTIFICATE-----\r\n"
         + wrapped
         + "\r\n-----END CERTIFICATE-----\r\n";
}

function alertPromptService(title, message) {
  // XXX Bug 1425832 - Using Services.prompt here causes tests to report memory
  // leaks.
  // eslint-disable-next-line mozilla/use-services
  var ps = Cc["@mozilla.org/embedcomp/prompt-service;1"].
           getService(Ci.nsIPromptService);
  ps.alert(window, title, message);
}

const DEFAULT_CERT_EXTENSION = "crt";

/**
 * Generates a filename for a cert suitable to set as the |defaultString|
 * attribute on an nsIFilePicker.
 *
 * @param {nsIX509Cert} cert
 *        The cert to generate a filename for.
 * @returns {String}
 *          Generated filename.
 */
function certToFilename(cert) {
  let filename = cert.displayName;

  // Remove unneeded and/or unsafe characters.
  filename = filename.replace(/\s/g, "")
                     .replace(/\./g, "")
                     .replace(/\\/g, "")
                     .replace(/\//g, "");

  // nsIFilePicker.defaultExtension is more of a suggestion to some
  // implementations, so we include the extension in the file name as well. This
  // is what the documentation for nsIFilePicker.defaultString says we should do
  // anyways.
  return `${filename}.${DEFAULT_CERT_EXTENSION}`;
}

async function exportToFile(parent, cert) {
  var bundle = document.getElementById("pippki_bundle");
  if (!cert) {
    return undefined;
  }

  var nsIFilePicker = Ci.nsIFilePicker;
  var fp = Cc["@mozilla.org/filepicker;1"].
           createInstance(nsIFilePicker);
  fp.init(parent, bundle.getString("SaveCertAs"),
          nsIFilePicker.modeSave);
  fp.defaultString = certToFilename(cert);
  fp.defaultExtension = DEFAULT_CERT_EXTENSION;
  fp.appendFilter(bundle.getString("CertFormatBase64"), "*.crt; *.pem");
  fp.appendFilter(bundle.getString("CertFormatBase64Chain"), "*.crt; *.pem");
  fp.appendFilter(bundle.getString("CertFormatDER"), "*.der");
  fp.appendFilter(bundle.getString("CertFormatPKCS7"), "*.p7c");
  fp.appendFilter(bundle.getString("CertFormatPKCS7Chain"), "*.p7c");
  fp.appendFilters(nsIFilePicker.filterAll);
  return new Promise(resolve => {
    fp.open(res => {
      resolve(fpCallback(res));
    });
  });

  function fpCallback(res) {
    if (res != nsIFilePicker.returnOK && res != nsIFilePicker.returnReplace) {
      return;
    }

    var content = "";
    switch (fp.filterIndex) {
      case 1:
        content = getPEMString(cert);
        var chain = cert.getChain();
        for (let i = 1; i < chain.length; i++) {
          content += getPEMString(chain.queryElementAt(i, Ci.nsIX509Cert));
        }
        break;
      case 2:
        content = getDERString(cert);
        break;
      case 3:
        content = getPKCS7String(cert, Ci.nsIX509Cert.CMS_CHAIN_MODE_CertOnly);
        break;
      case 4:
        content = getPKCS7String(cert, Ci.nsIX509Cert.CMS_CHAIN_MODE_CertChainWithRoot);
        break;
      case 0:
      default:
        content = getPEMString(cert);
        break;
    }
    var msg;
    var written = 0;
    try {
      var file = Cc["@mozilla.org/file/local;1"].
                 createInstance(Ci.nsIFile);
      file.initWithPath(fp.file.path);
      var fos = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
      // flags: PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
      fos.init(file, 0x02 | 0x08 | 0x20, 0o0644, 0);
      written = fos.write(content, content.length);
      fos.close();
    } catch (e) {
      switch (e.result) {
        case Cr.NS_ERROR_FILE_ACCESS_DENIED:
          msg = bundle.getString("writeFileAccessDenied");
          break;
        case Cr.NS_ERROR_FILE_IS_LOCKED:
          msg = bundle.getString("writeFileIsLocked");
          break;
        case Cr.NS_ERROR_FILE_NO_DEVICE_SPACE:
        case Cr.NS_ERROR_FILE_DISK_FULL:
          msg = bundle.getString("writeFileNoDeviceSpace");
          break;
        default:
          msg = e.message;
          break;
      }
    }
    if (written != content.length) {
      if (msg.length == 0) {
        msg = bundle.getString("writeFileUnknownError");
      }
      alertPromptService(bundle.getString("writeFileFailure"),
                         bundle.getFormattedString("writeFileFailed",
                         [fp.file.path, msg]));
    }
  }
}

const PRErrorCodeSuccess = 0;

// Certificate usages we care about in the certificate viewer.
const certificateUsageSSLClient              = 0x0001;
const certificateUsageSSLServer              = 0x0002;
const certificateUsageSSLCA                  = 0x0008;
const certificateUsageEmailSigner            = 0x0010;
const certificateUsageEmailRecipient         = 0x0020;

// A map from the name of a certificate usage to the value of the usage.
// Useful for printing debugging information and for enumerating all supported
// usages.
const certificateUsages = {
  certificateUsageSSLClient,
  certificateUsageSSLServer,
  certificateUsageSSLCA,
  certificateUsageEmailSigner,
  certificateUsageEmailRecipient,
};

/**
 * Returns a promise that will resolve with a results array (see
 * `displayUsages` in certViewer.js) consisting of what usages the given
 * certificate successfully verified for.
 *
 * @param {nsIX509Cert} cert
 *        The certificate to determine valid usages for.
 * @return {Promise}
 *        A promise that will resolve with the results of the verifications.
 */
function asyncDetermineUsages(cert) {
  let promises = [];
  let now = Date.now() / 1000;
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  Object.keys(certificateUsages).forEach(usageString => {
    promises.push(new Promise((resolve, reject) => {
      let usage = certificateUsages[usageString];
      certdb.asyncVerifyCertAtTime(cert, usage, 0, null, now,
        (aPRErrorCode, aVerifiedChain, aHasEVPolicy) => {
          resolve({ usageString,
                    errorCode: aPRErrorCode,
                    chain: aVerifiedChain });
        });
    }));
  });
  return Promise.all(promises);
}

/**
 * Given a results array (see `displayUsages` in certViewer.js), returns the
 * "best" verified certificate chain. Since the primary use case is for TLS
 * server certificates in Firefox, such a verified chain will be returned if
 * present. Otherwise, the priority is: TLS client certificate, email signer,
 * email recipient, CA. Returns null if no usage verified successfully.
 *
 * @param {Array} results
 *        An array of results from `asyncDetermineUsages`. See `displayUsages`.
 * @param {Number} usage
 *        A numerical value corresponding to a usage. See `certificateUsages`.
 * @returns {Array} An array of `nsIX509Cert` representing the verified
 *          certificate chain for the given usage, or null if there is none.
 */
function getBestChain(results) {
  let usages = [ certificateUsageSSLServer, certificateUsageSSLClient,
                 certificateUsageEmailSigner, certificateUsageEmailRecipient,
                 certificateUsageSSLCA ];
  for (let usage of usages) {
    let chain = getChainForUsage(results, usage);
    if (chain) {
      return chain;
    }
  }
  return null;
}

/**
 * Given a results array (see `displayUsages` in certViewer.js), returns the
 * chain corresponding to the desired usage, if verifying for that usage
 * succeeded. Returns null otherwise.
 *
 * @param {Array} results
 *        An array of results from `asyncDetermineUsages`. See `displayUsages`.
 * @param {Number} usage
 *        A numerical value corresponding to a usage. See `certificateUsages`.
 * @returns {Array} An array of `nsIX509Cert` representing the verified
 *          certificate chain for the given usage, or null if there is none.
 */
function getChainForUsage(results, usage) {
  for (let result of results) {
    if (certificateUsages[result.usageString] == usage &&
        result.errorCode == PRErrorCodeSuccess) {
      let array = [];
      let enumerator = result.chain.getEnumerator();
      while (enumerator.hasMoreElements()) {
        let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
        array.push(cert);
      }
      return array;
    }
  }
  return null;
}

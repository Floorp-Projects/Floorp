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

async function viewCertHelper(parent, cert, openingOption = "tab") {
  if (!cert) {
    return;
  }

  let win = Services.wm.getMostRecentBrowserWindow();
  let results = await asyncDetermineUsages(cert);
  let chain = getBestChain(results);
  if (!chain) {
    chain = [cert];
  }
  let certs = chain.map(elem => encodeURIComponent(elem.getBase64DERString()));
  let certsStringURL = certs.map(elem => `cert=${elem}`);
  certsStringURL = certsStringURL.join("&");
  let url = `about:certificate?${certsStringURL}`;
  let opened = win.switchToTabHavingURI(url, false, {});
  if (!opened) {
    win.openTrustedLinkIn(url, openingOption);
  }
}

function getPKCS7Array(certArray) {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let pkcs7String = certdb.asPKCS7Blob(certArray);
  let pkcs7Array = new Uint8Array(pkcs7String.length);
  for (let i = 0; i < pkcs7Array.length; i++) {
    pkcs7Array[i] = pkcs7String.charCodeAt(i);
  }
  return pkcs7Array;
}

function getPEMString(cert) {
  var derb64 = cert.getBase64DERString();
  // Wrap the Base64 string into lines of 64 characters with CRLF line breaks
  // (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return (
    "-----BEGIN CERTIFICATE-----\r\n" +
    wrapped +
    "\r\n-----END CERTIFICATE-----\r\n"
  );
}

function alertPromptService(title, message) {
  // XXX Bug 1425832 - Using Services.prompt here causes tests to report memory
  // leaks.
  // eslint-disable-next-line mozilla/use-services
  var ps = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(
    Ci.nsIPromptService
  );
  ps.alert(window, title, message);
}

const DEFAULT_CERT_EXTENSION = "crt";

/**
 * Generates a filename for a cert suitable to set as the |defaultString|
 * attribute on an Ci.nsIFilePicker.
 *
 * @param {nsIX509Cert} cert
 *        The cert to generate a filename for.
 * @returns {String}
 *          Generated filename.
 */
function certToFilename(cert) {
  let filename = cert.displayName;

  // Remove unneeded and/or unsafe characters.
  filename = filename
    .replace(/\s/g, "")
    .replace(/\./g, "_")
    .replace(/\\/g, "")
    .replace(/\//g, "");

  // Ci.nsIFilePicker.defaultExtension is more of a suggestion to some
  // implementations, so we include the extension in the file name as well. This
  // is what the documentation for Ci.nsIFilePicker.defaultString says we should do
  // anyways.
  return `${filename}.${DEFAULT_CERT_EXTENSION}`;
}

async function exportToFile(parent, cert) {
  if (!cert) {
    return;
  }

  let results = await asyncDetermineUsages(cert);
  let chain = getBestChain(results);
  if (!chain) {
    chain = [cert];
  }

  let formats = {
    base64: "*.crt; *.pem",
    "base64-chain": "*.crt; *.pem",
    der: "*.der",
    pkcs7: "*.p7c",
    "pkcs7-chain": "*.p7c",
  };
  let [saveCertAs, ...formatLabels] = await document.l10n.formatValues(
    [
      "save-cert-as",
      ...Object.keys(formats).map(f => "cert-format-" + f),
    ].map(id => ({ id }))
  );

  var fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.init(parent, saveCertAs, Ci.nsIFilePicker.modeSave);
  fp.defaultString = certToFilename(cert);
  fp.defaultExtension = DEFAULT_CERT_EXTENSION;
  for (let format of Object.values(formats)) {
    fp.appendFilter(formatLabels.shift(), format);
  }
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  let filePickerResult = await new Promise(resolve => {
    fp.open(resolve);
  });

  if (
    filePickerResult != Ci.nsIFilePicker.returnOK &&
    filePickerResult != Ci.nsIFilePicker.returnReplace
  ) {
    return;
  }

  var content = "";
  switch (fp.filterIndex) {
    case 1:
      content = getPEMString(cert);
      for (let i = 1; i < chain.length; i++) {
        content += getPEMString(chain[i]);
      }
      break;
    case 2:
      // IOUtils.write requires a typed array.
      // nsIX509Cert.getRawDER() returns an array (not a typed array), so we
      // convert it here.
      content = Uint8Array.from(cert.getRawDER());
      break;
    case 3:
      // getPKCS7Array returns a typed array already, so no conversion is
      // necessary.
      content = getPKCS7Array([cert]);
      break;
    case 4:
      content = getPKCS7Array(chain);
      break;
    case 0:
    default:
      content = getPEMString(cert);
      break;
  }

  if (typeof content === "string") {
    content = new TextEncoder().encode(content);
  }

  try {
    await IOUtils.write(fp.file.path, content);
  } catch (ex) {
    let title = await document.l10n.formatValue("write-file-failure");
    alertPromptService(title, ex.toString());
  }
  if (Cu.isInAutomation) {
    Services.obs.notifyObservers(null, "cert-export-finished");
  }
}

const PRErrorCodeSuccess = 0;

// Certificate usages we care about in the certificate viewer.
const certificateUsageSSLClient = 0x0001;
const certificateUsageSSLServer = 0x0002;
const certificateUsageSSLCA = 0x0008;
const certificateUsageEmailSigner = 0x0010;
const certificateUsageEmailRecipient = 0x0020;

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
 * Returns a promise that will resolve with a results array consisting of what
 * usages the given certificate successfully verified for.
 *
 * @param {nsIX509Cert} cert
 *        The certificate to determine valid usages for.
 * @return {Promise}
 *        A promise that will resolve with the results of the verifications.
 */
function asyncDetermineUsages(cert) {
  let promises = [];
  let now = Date.now() / 1000;
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  Object.keys(certificateUsages).forEach(usageString => {
    promises.push(
      new Promise((resolve, reject) => {
        let usage = certificateUsages[usageString];
        certdb.asyncVerifyCertAtTime(
          cert,
          usage,
          0,
          null,
          now,
          (aPRErrorCode, aVerifiedChain, aHasEVPolicy) => {
            resolve({
              usageString,
              errorCode: aPRErrorCode,
              chain: aVerifiedChain,
            });
          }
        );
      })
    );
  });
  return Promise.all(promises);
}

/**
 * Given a results array, returns the "best" verified certificate chain. Since
 * the primary use case is for TLS server certificates in Firefox, such a
 * verified chain will be returned if present. Otherwise, the priority is: TLS
 * client certificate, email signer, email recipient, CA. Returns null if no
 * usage verified successfully.
 *
 * @param {Array} results
 *        An array of results from `asyncDetermineUsages`. See `displayUsages`.
 * @param {Number} usage
 *        A numerical value corresponding to a usage. See `certificateUsages`.
 * @returns {Array} An array of `nsIX509Cert` representing the verified
 *          certificate chain for the given usage, or null if there is none.
 */
function getBestChain(results) {
  let usages = [
    certificateUsageSSLServer,
    certificateUsageSSLClient,
    certificateUsageEmailSigner,
    certificateUsageEmailRecipient,
    certificateUsageSSLCA,
  ];
  for (let usage of usages) {
    let chain = getChainForUsage(results, usage);
    if (chain) {
      return chain;
    }
  }
  return null;
}

/**
 * Given a results array, returns the chain corresponding to the desired usage,
 * if verifying for that usage succeeded. Returns null otherwise.
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
    if (
      certificateUsages[result.usageString] == usage &&
      result.errorCode == PRErrorCodeSuccess
    ) {
      return result.chain;
    }
  }
  return null;
}

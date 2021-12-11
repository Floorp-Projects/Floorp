/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var EXPORTED_SYMBOLS = ["CertUtils"];

const Ce = Components.Exception;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

/**
 * Reads a set of expected certificate attributes from preferences. The returned
 * array can be passed to validateCert or checkCert to validate that a
 * certificate matches the expected attributes. The preferences should look like
 * this:
 *   prefix.1.attribute1
 *   prefix.1.attribute2
 *   prefix.2.attribute1
 *   etc.
 * Each numeric branch contains a set of required attributes for a single
 * certificate. Having multiple numeric branches means that multiple
 * certificates would be accepted by validateCert.
 *
 * @param  aPrefBranch
 *         The prefix for all preferences, should end with a ".".
 * @return An array of JS objects with names / values corresponding to the
 *         expected certificate's attribute names / values.
 */
function readCertPrefs(aPrefBranch) {
  if (!Services.prefs.getBranch(aPrefBranch).getChildList("").length) {
    return null;
  }

  let certs = [];
  let counter = 1;
  while (true) {
    let prefBranchCert = Services.prefs.getBranch(aPrefBranch + counter + ".");
    let prefCertAttrs = prefBranchCert.getChildList("");
    if (!prefCertAttrs.length) {
      break;
    }

    let certAttrs = {};
    for (let prefCertAttr of prefCertAttrs) {
      certAttrs[prefCertAttr] = prefBranchCert.getCharPref(prefCertAttr);
    }

    certs.push(certAttrs);
    counter++;
  }

  return certs;
}

/**
 * Verifies that an nsIX509Cert matches the expected certificate attribute
 * values.
 *
 * @param  aCertificate
 *         The nsIX509Cert to compare to the expected attributes.
 * @param  aCerts
 *         An array of JS objects with names / values corresponding to the
 *         expected certificate's attribute names / values. If this is null or
 *         an empty array then no checks are performed.
 * @throws NS_ERROR_ILLEGAL_VALUE if a certificate attribute name from the
 *         aCerts param does not exist or the value for a certificate attribute
 *         from the aCerts param is different than the expected value or
 *         aCertificate wasn't specified and aCerts is not null or an empty
 *         array.
 */
function validateCert(aCertificate, aCerts) {
  // If there are no certificate requirements then just exit
  if (!aCerts || !aCerts.length) {
    return;
  }

  if (!aCertificate) {
    const missingCertErr = "A required certificate was not present.";
    Cu.reportError(missingCertErr);
    throw new Ce(missingCertErr, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  var errors = [];
  for (var i = 0; i < aCerts.length; ++i) {
    var error = false;
    var certAttrs = aCerts[i];
    for (var name in certAttrs) {
      if (!(name in aCertificate)) {
        error = true;
        errors.push(
          "Expected attribute '" + name + "' not present in certificate."
        );
        break;
      }
      if (aCertificate[name] != certAttrs[name]) {
        error = true;
        errors.push(
          "Expected certificate attribute '" +
            name +
            "' " +
            "value incorrect, expected: '" +
            certAttrs[name] +
            "', got: '" +
            aCertificate[name] +
            "'."
        );
        break;
      }
    }

    if (!error) {
      break;
    }
  }

  if (error) {
    errors.forEach(Cu.reportError.bind(Cu));
    const certCheckErr =
      "Certificate checks failed. See previous errors for details.";
    Cu.reportError(certCheckErr);
    throw new Ce(certCheckErr, Cr.NS_ERROR_ILLEGAL_VALUE);
  }
}

/**
 * Checks if the connection must be HTTPS and if so, only allows built-in
 * certificates and validates application specified certificate attribute
 * values.
 * See bug 340198 and bug 544442.
 *
 * @param  aChannel
 *         The nsIChannel that will have its certificate checked.
 * @param  aAllowNonBuiltInCerts (optional)
 *         When true certificates that aren't builtin are allowed. When false
 *         or not specified the certificate must be a builtin certificate.
 * @param  aCerts (optional)
 *         An array of JS objects with names / values corresponding to the
 *         channel's expected certificate's attribute names / values. If it
 *         isn't null or not specified the the scheme for the channel's
 *         originalURI must be https.
 * @throws NS_ERROR_UNEXPECTED if a certificate is expected and the URI scheme
 *         is not https.
 *         NS_ERROR_ILLEGAL_VALUE if a certificate attribute name from the
 *         aCerts param does not exist or the value for a certificate attribute
 *         from the aCerts  param is different than the expected value.
 *         NS_ERROR_ABORT if the certificate issuer is not built-in.
 */
function checkCert(aChannel, aAllowNonBuiltInCerts, aCerts) {
  if (!aChannel.originalURI.schemeIs("https")) {
    // Require https if there are certificate values to verify
    if (aCerts) {
      throw new Ce(
        "SSL is required and URI scheme is not https.",
        Cr.NS_ERROR_UNEXPECTED
      );
    }
    return;
  }

  let secInfo = aChannel.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  let cert = secInfo.serverCert;

  validateCert(cert, aCerts);

  if (aAllowNonBuiltInCerts === true) {
    return;
  }

  let issuerCert = null;
  if (secInfo.succeededCertChain.length) {
    issuerCert =
      secInfo.succeededCertChain[secInfo.succeededCertChain.length - 1];
  }

  const certNotBuiltInErr = "Certificate issuer is not built-in.";
  if (!issuerCert) {
    throw new Ce(certNotBuiltInErr, Cr.NS_ERROR_ABORT);
  }

  if (!issuerCert.isBuiltInRoot) {
    throw new Ce(certNotBuiltInErr, Cr.NS_ERROR_ABORT);
  }
}

/**
 * This class implements nsIChannelEventSink. Its job is to perform extra checks
 * on the certificates used for some connections when those connections
 * redirect.
 *
 * @param  aAllowNonBuiltInCerts (optional)
 *         When true certificates that aren't builtin are allowed. When false
 *         or not specified the certificate must be a builtin certificate.
 */
function BadCertHandler(aAllowNonBuiltInCerts) {
  this.allowNonBuiltInCerts = aAllowNonBuiltInCerts;
}
BadCertHandler.prototype = {
  // nsIChannelEventSink
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    if (this.allowNonBuiltInCerts) {
      callback.onRedirectVerifyCallback(Cr.NS_OK);
      return;
    }

    // make sure the certificate of the old channel checks out before we follow
    // a redirect from it.  See bug 340198.
    // Don't call checkCert for internal redirects. See bug 569648.
    if (!(flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL)) {
      checkCert(oldChannel);
    }

    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([
    "nsIChannelEventSink",
    "nsIInterfaceRequestor",
  ]),
};

var CertUtils = {
  BadCertHandler,
  checkCert,
  readCertPrefs,
  validateCert,
};

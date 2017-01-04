/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
this.EXPORTED_SYMBOLS = [ "BadCertHandler", "checkCert", "readCertPrefs", "validateCert" ];

const Ce = Components.Exception;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/Services.jsm");

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
this.readCertPrefs =
  function readCertPrefs(aPrefBranch) {
  if (Services.prefs.getBranch(aPrefBranch).getChildList("").length == 0)
    return null;

  let certs = [];
  let counter = 1;
  while (true) {
    let prefBranchCert = Services.prefs.getBranch(aPrefBranch + counter + ".");
    let prefCertAttrs = prefBranchCert.getChildList("");
    if (prefCertAttrs.length == 0)
      break;

    let certAttrs = {};
    for (let prefCertAttr of prefCertAttrs)
      certAttrs[prefCertAttr] = prefBranchCert.getCharPref(prefCertAttr);

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
this.validateCert =
  function validateCert(aCertificate, aCerts) {
  // If there are no certificate requirements then just exit
  if (!aCerts || aCerts.length == 0)
    return;

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
        errors.push("Expected attribute '" + name + "' not present in " +
                    "certificate.");
        break;
      }
      if (aCertificate[name] != certAttrs[name]) {
        error = true;
        errors.push("Expected certificate attribute '" + name + "' " +
                    "value incorrect, expected: '" + certAttrs[name] +
                    "', got: '" + aCertificate[name] + "'.");
        break;
      }
    }

    if (!error)
      break;
  }

  if (error) {
    errors.forEach(Cu.reportError.bind(Cu));
    const certCheckErr = "Certificate checks failed. See previous errors " +
                         "for details.";
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
this.checkCert =
  function checkCert(aChannel, aAllowNonBuiltInCerts, aCerts) {
  if (!aChannel.originalURI.schemeIs("https")) {
    // Require https if there are certificate values to verify
    if (aCerts) {
      throw new Ce("SSL is required and URI scheme is not https.",
                   Cr.NS_ERROR_UNEXPECTED);
    }
    return;
  }

  var cert =
      aChannel.securityInfo.QueryInterface(Ci.nsISSLStatusProvider).
      SSLStatus.QueryInterface(Ci.nsISSLStatus).serverCert;

  validateCert(cert, aCerts);

  if (aAllowNonBuiltInCerts === true)
    return;

  var issuerCert = cert;
  while (issuerCert.issuer && !issuerCert.issuer.equals(issuerCert))
    issuerCert = issuerCert.issuer;

  const certNotBuiltInErr = "Certificate issuer is not built-in.";
  if (!issuerCert)
    throw new Ce(certNotBuiltInErr, Cr.NS_ERROR_ABORT);

  var tokenNames = issuerCert.getAllTokenNames({});

  if (!tokenNames || !tokenNames.some(isBuiltinToken))
    throw new Ce(certNotBuiltInErr, Cr.NS_ERROR_ABORT);
}

function isBuiltinToken(tokenName) {
  return tokenName == "Builtin Object Token";
}

/**
 * This class implements nsIBadCertListener.  Its job is to prevent "bad cert"
 * security dialogs from being shown to the user.  It is better to simply fail
 * if the certificate is bad. See bug 304286.
 *
 * @param  aAllowNonBuiltInCerts (optional)
 *         When true certificates that aren't builtin are allowed. When false
 *         or not specified the certificate must be a builtin certificate.
 */
this.BadCertHandler =
  function BadCertHandler(aAllowNonBuiltInCerts) {
  this.allowNonBuiltInCerts = aAllowNonBuiltInCerts;
}
BadCertHandler.prototype = {

  // nsIChannelEventSink
  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    if (this.allowNonBuiltInCerts) {
      callback.onRedirectVerifyCallback(Components.results.NS_OK);
      return;
    }

    // make sure the certificate of the old channel checks out before we follow
    // a redirect from it.  See bug 340198.
    // Don't call checkCert for internal redirects. See bug 569648.
    if (!(flags & Ci.nsIChannelEventSink.REDIRECT_INTERNAL))
      checkCert(oldChannel);

    callback.onRedirectVerifyCallback(Components.results.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface(iid) {
    return this.QueryInterface(iid);
  },

  // nsISupports
  QueryInterface(iid) {
    if (!iid.equals(Ci.nsIChannelEventSink) &&
        !iid.equals(Ci.nsIInterfaceRequestor) &&
        !iid.equals(Ci.nsISupports))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

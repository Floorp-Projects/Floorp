/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SecurityInfo"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const wpl = Ci.nsIWebProgressListener;
XPCOMUtils.defineLazyServiceGetter(this, "NSSErrorsService",
                                   "@mozilla.org/nss_errors_service;1",
                                   "nsINSSErrorsService");
XPCOMUtils.defineLazyServiceGetter(this, "sss",
                                   "@mozilla.org/ssservice;1",
                                   "nsISiteSecurityService");

// NOTE: SecurityInfo is largely reworked from the devtools NetworkHelper with changes
// to better support the WebRequest api.  The objects returned are formatted specifically
// to pass through as part of a response to webRequest listeners.

const SecurityInfo = {
  /**
   * Extracts security information from nsIChannel.securityInfo.
   *
   * @param {nsIChannel} channel
   *        If null channel is assumed to be insecure.
   * @param {Object} options
   *
   * @returns {Object}
   *         Returns an object containing following members:
   *          - state: The security of the connection used to fetch this
   *                   request. Has one of following string values:
   *                    * "insecure": the connection was not secure (only http)
   *                    * "weak": the connection has minor security issues
   *                    * "broken": secure connection failed (e.g. expired cert)
   *                    * "secure": the connection was properly secured.
   *          If state == broken:
   *            - errorMessage: full error message from
   *                            nsITransportSecurityInfo.
   *          If state == secure:
   *            - protocolVersion: one of TLSv1, TLSv1.1, TLSv1.2, TLSv1.3.
   *            - cipherSuite: the cipher suite used in this connection.
   *            - cert: information about certificate used in this connection.
   *                    See parseCertificateInfo for the contents.
   *            - hsts: true if host uses Strict Transport Security,
   *                    false otherwise
   *            - hpkp: true if host uses Public Key Pinning, false otherwise
   *          If state == weak: Same as state == secure and
   *            - weaknessReasons: list of reasons that cause the request to be
   *                               considered weak. See getReasonsForWeakness.
   */
  getSecurityInfo(channel, options = {}) {
    const info = {
      state: "insecure",
    };

    /**
     * Different scenarios to consider here and how they are handled:
     * - request is HTTP, the connection is not secure
     *   => securityInfo is null
     *      => state === "insecure"
     *
     * - request is HTTPS, the connection is secure
     *   => .securityState has STATE_IS_SECURE flag
     *      => state === "secure"
     *
     * - request is HTTPS, the connection has security issues
     *   => .securityState has STATE_IS_INSECURE flag
     *   => .errorCode is an NSS error code.
     *      => state === "broken"
     *
     * - request is HTTPS, the connection was terminated before the security
     *   could be validated
     *   => .securityState has STATE_IS_INSECURE flag
     *   => .errorCode is NOT an NSS error code.
     *   => .errorMessage is not available.
     *      => state === "insecure"
     *
     * - request is HTTPS but it uses a weak cipher or old protocol, see
     *   https://hg.mozilla.org/mozilla-central/annotate/def6ed9d1c1a/
     *   security/manager/ssl/nsNSSCallbacks.cpp#l1233
     * - request is mixed content (which makes no sense whatsoever)
     *   => .securityState has STATE_IS_BROKEN flag
     *   => .errorCode is NOT an NSS error code
     *   => .errorMessage is not available
     *      => state === "weak"
     */

    let securityInfo = channel.securityInfo;
    if (!securityInfo) {
      return info;
    }

    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
    securityInfo.QueryInterface(Ci.nsISSLStatusProvider);

    const SSLStatus = securityInfo.SSLStatus;
    if (NSSErrorsService.isNSSErrorCode(securityInfo.errorCode)) {
      // The connection failed.
      info.state = "broken";
      info.errorMessage = securityInfo.errorMessage;
      if (options.certificateChain && SSLStatus.failedCertChain) {
        info.certificates = this.getCertificateChain(SSLStatus.failedCertChain, options);
      }
      return info;
    }

    const state = securityInfo.securityState;

    let uri = channel.URI;
    if (uri && !uri.schemeIs("https") && !uri.schemeIs("wss")) {
      // it is not enough to look at the transport security info -
      // schemes other than https and wss are subject to
      // downgrade/etc at the scheme level and should always be
      // considered insecure.
      // Leave info.state = "insecure";
    } else if (state & wpl.STATE_IS_SECURE) {
      // The connection is secure if the scheme is sufficient
      info.state = "secure";
    } else if (state & wpl.STATE_IS_BROKEN) {
      // The connection is not secure, there was no error but there's some
      // minor security issues.
      info.state = "weak";
      info.weaknessReasons = this.getReasonsForWeakness(state);
    } else if (state & wpl.STATE_IS_INSECURE) {
      // This was most likely an https request that was aborted before
      // validation. Return info as info.state = insecure.
      return info;
    } else {
      // No known STATE_IS_* flags.
      return info;
    }

    // Cipher suite.
    info.cipherSuite = SSLStatus.cipherName;

    // Key exchange group name.
    if (SSLStatus.keaGroupName !== "none") {
      info.keaGroupName = SSLStatus.keaGroupName;
    }

    // Certificate signature scheme.
    if (SSLStatus.signatureSchemeName !== "none") {
      info.signatureSchemeName = SSLStatus.signatureSchemeName;
    }

    info.isDomainMismatch = SSLStatus.isDomainMismatch;
    info.isExtendedValidation = SSLStatus.isExtendedValidation;
    info.isNotValidAtThisTime = SSLStatus.isNotValidAtThisTime;
    info.isUntrusted = SSLStatus.isUntrusted;

    info.certificateTransparencyStatus = this.getTransparencyStatus(SSLStatus.certificateTransparencyStatus);

    // Protocol version.
    info.protocolVersion = this.formatSecurityProtocol(SSLStatus.protocolVersion);

    if (options.certificateChain && SSLStatus.succeededCertChain) {
      info.certificates = this.getCertificateChain(SSLStatus.succeededCertChain, options);
    } else {
      info.certificates = [this.parseCertificateInfo(SSLStatus.serverCert, options)];
    }

    // HSTS and HPKP if available.
    if (uri && uri.host) {
      // SiteSecurityService uses different storage if the channel is
      // private. Thus we must give isSecureURI correct flags or we
      // might get incorrect results.
      let flags = 0;
      if (channel instanceof Ci.nsIPrivateBrowsingChannel && channel.isChannelPrivate) {
        flags = Ci.nsISocketProvider.NO_PERMANENT_STORAGE;
      }

      info.hsts = sss.isSecureURI(sss.HEADER_HSTS, uri, flags);
      info.hpkp = sss.isSecureURI(sss.HEADER_HPKP, uri, flags);
    } else {
      info.hsts = false;
      info.hpkp = false;
    }

    return info;
  },

  getCertificateChain(certChain, options = {}) {
    let certificates = [];
    for (let cert of XPCOMUtils.IterSimpleEnumerator(certChain.getEnumerator(), Ci.nsIX509Cert)) {
      certificates.push(this.parseCertificateInfo(cert, options));
    }
    return certificates;
  },

  /**
   * Takes an nsIX509Cert and returns an object with certificate information.
   *
   * @param {nsIX509Cert} cert
   *        The certificate to extract the information from.
   * @param {Object} options
   * @returns {Object}
   *         An object with following format:
   *           {
   *             subject: subjectName,
   *             issuer: issuerName,
   *             validity: { start, end },
   *             fingerprint: { sha1, sha256 }
   *           }
   */
  parseCertificateInfo(cert, options = {}) {
    if (!cert) {
      return {};
    }

    let certData = {
      subject: cert.subjectName,
      issuer: cert.issuerName,
      validity: {
        start: cert.validity.notBefore,
        end: cert.validity.notAfter,
      },
      fingerprint: {
        sha1: cert.sha1Fingerprint,
        sha256: cert.sha256Fingerprint,
      },
      serialNumber: cert.serialNumber,
      isBuiltInRoot: cert.isBuiltInRoot,
      subjectPublicKeyInfoDigest: {
        sha256: cert.sha256SubjectPublicKeyInfoDigest,
      },
    };
    if (options.rawDER) {
      certData.rawDER = cert.getRawDER({});
    }
    return certData;
  },

  // Bug 1355903 Transparency is currently disabled using security.pki.certificate_transparency.mode
  getTransparencyStatus(status) {
    switch (status) {
      case Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_NOT_APPLICABLE:
        return "not_applicable";
      case Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_COMPLIANT:
        return "policy_compliant";
      case Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_NOT_ENOUGH_SCTS:
        return "policy_not_enough_scts";
      case Ci.nsISSLStatus.CERTIFICATE_TRANSPARENCY_POLICY_NOT_DIVERSE_SCTS:
        return "policy_not_diverse_scts";
    }
    return "unknown";
  },

  /**
   * Takes protocolVersion of SSLStatus object and returns human readable
   * description.
   *
   * @param {number} version
   *        One of nsISSLStatus version constants.
   * @returns {string}
   *         One of TLSv1, TLSv1.1, TLSv1.2, TLSv1.3 if version
   *         is valid, Unknown otherwise.
   */
  formatSecurityProtocol(version) {
    switch (version) {
      case Ci.nsISSLStatus.TLS_VERSION_1:
        return "TLSv1";
      case Ci.nsISSLStatus.TLS_VERSION_1_1:
        return "TLSv1.1";
      case Ci.nsISSLStatus.TLS_VERSION_1_2:
        return "TLSv1.2";
      case Ci.nsISSLStatus.TLS_VERSION_1_3:
        return "TLSv1.3";
    }
    return "unknown";
  },

  /**
   * Takes the securityState bitfield and returns reasons for weak connection
   * as an array of strings.
   *
   * @param {number} state
   *        nsITransportSecurityInfo.securityState.
   *
   * @returns {array<string>}
   *         List of weakness reasons. A subset of { cipher } where
   *         * cipher: The cipher suite is consireded to be weak (RC4).
   */
  getReasonsForWeakness(state) {
    // If there's non-fatal security issues the request has STATE_IS_BROKEN
    // flag set. See https://hg.mozilla.org/mozilla-central/file/44344099d119
    // /security/manager/ssl/nsNSSCallbacks.cpp#l1233
    let reasons = [];

    if (state & wpl.STATE_IS_BROKEN) {
      if (state & wpl.STATE_USES_WEAK_CRYPTO) {
        reasons.push("cipher");
      }
    }

    return reasons;
  },
};

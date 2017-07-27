/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("security.tls.version.min", 1);
pref("security.tls.version.max", 4);
pref("security.tls.version.fallback-limit", 3);
pref("security.tls.insecure_fallback_hosts", "");
pref("security.tls.enable_0rtt_data", false);

pref("security.ssl.treat_unsafe_negotiation_as_broken", false);
pref("security.ssl.require_safe_negotiation",  false);
pref("security.ssl.enable_ocsp_stapling", true);
pref("security.ssl.enable_false_start", true);
pref("security.ssl.enable_alpn", true);

pref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_sha", true);
pref("security.ssl3.dhe_rsa_aes_128_sha", true);
pref("security.ssl3.dhe_rsa_aes_256_sha", true);
pref("security.ssl3.rsa_aes_128_sha", true);
pref("security.ssl3.rsa_aes_256_sha", true);
pref("security.ssl3.rsa_des_ede3_sha", true);

pref("security.content.signature.root_hash",
     "97:E8:BA:9C:F1:2F:B3:DE:53:CC:42:A4:E6:57:7E:D6:4D:F4:93:C2:47:B4:14:FE:A0:36:81:8D:38:23:56:0E");

pref("security.default_personal_cert",   "Ask Every Time");
pref("security.remember_cert_checkbox_default_setting", true);
pref("security.ask_for_password",        0);
pref("security.password_lifetime",       30);

// The supported values of this pref are:
// 0: disable detecting Family Safety mode and importing the root
// 1: only attempt to detect Family Safety mode (don't import the root)
// 2: detect Family Safety mode and import the root
// (This is only relevant to Windows 8.1)
pref("security.family_safety.mode", 2);

pref("security.enterprise_roots.enabled", false);

// The supported values of this pref are:
// 0: do not fetch OCSP
// 1: fetch OCSP for DV and EV certificates
// 2: fetch OCSP only for EV certificates
#ifdef RELEASE_OR_BETA
pref("security.OCSP.enabled", 1);
#else
pref("security.OCSP.enabled", 2);
#endif
pref("security.OCSP.require", false);
pref("security.OCSP.GET.enabled", false);
#ifdef RELEASE_OR_BETA
pref("security.OCSP.timeoutMilliseconds.soft", 2000);
#else
pref("security.OCSP.timeoutMilliseconds.soft", 1000);
#endif
pref("security.OCSP.timeoutMilliseconds.hard", 10000);

pref("security.pki.cert_short_lifetime_in_days", 10);
// NB: Changes to this pref affect CERT_CHAIN_SHA1_POLICY_STATUS telemetry.
// See the comment in CertVerifier.cpp.
// 3 = only allow SHA-1 for certificates issued by an imported root.
pref("security.pki.sha1_enforcement_level", 3);

// security.pki.name_matching_mode controls how the platform matches hostnames
// to name information in TLS certificates. The possible values are:
// 0: always fall back to the subject common name if necessary (as in, if the
//    subject alternative name extension is either not present or does not
//    contain any DNS names or IP addresses)
// 1: fall back to the subject common name for certificates valid before 23
//    August 2016 if necessary
// 2: fall back to the subject common name for certificates valid before 23
//    August 2015 if necessary
// 3: only use name information from the subject alternative name extension
#ifdef RELEASE_OR_BETA
pref("security.pki.name_matching_mode", 1);
#else
pref("security.pki.name_matching_mode", 2);
#endif

// security.pki.netscape_step_up_policy controls how the platform handles the
// id-Netscape-stepUp OID in extended key usage extensions of CA certificates.
// 0: id-Netscape-stepUp is always considered equivalent to id-kp-serverAuth
// 1: it is considered equivalent when the notBefore is before 23 August 2016
// 2: similarly, but for 23 August 2015
// 3: it is never considered equivalent
#ifdef RELEASE_OR_BETA
pref("security.pki.netscape_step_up_policy", 1);
#else
pref("security.pki.netscape_step_up_policy", 2);
#endif

// Configures Certificate Transparency support mode:
// 0: Fully disabled.
// 1: Only collect telemetry. CT qualification checks are not performed.
pref("security.pki.certificate_transparency.mode", 0);

pref("security.webauth.u2f", false);
pref("security.webauth.u2f_enable_softtoken", false);
pref("security.webauth.u2f_enable_usbtoken", false);

pref("security.webauth.webauthn", false);
pref("security.webauth.webauthn_enable_softtoken", false);
pref("security.webauth.webauthn_enable_usbtoken", false);

pref("security.ssl.errorReporting.enabled", true);
pref("security.ssl.errorReporting.url", "https://incoming.telemetry.mozilla.org/submit/sslreports/");
pref("security.ssl.errorReporting.automatic", false);

// Impose a maximum age on HPKP headers, to avoid sites getting permanently
// blacking themselves out by setting a bad pin.  (60 days by default)
// https://tools.ietf.org/html/rfc7469#section-4.1
pref("security.cert_pinning.max_max_age_seconds", 5184000);

// HSTS Priming
// If a request is mixed-content, send an HSTS priming request to attempt to
// see if it is available over HTTPS.
// Don't change the order of evaluation of mixed-content and HSTS upgrades in
// order to be most compatible with current standards in Release
pref("security.mixed_content.send_hsts_priming", false);
pref("security.mixed_content.use_hsts", false);
#ifdef EARLY_BETA_OR_EARLIER
// Change the order of evaluation so HSTS upgrades happen before
// mixed-content blocking
pref("security.mixed_content.send_hsts_priming", true);
pref("security.mixed_content.use_hsts", true);
#endif
// Approximately 1 week default cache for HSTS priming failures, in seconds
pref ("security.mixed_content.hsts_priming_cache_timeout", 604800);
// Force the channel to timeout in 2 seconds if we have not received
// expects a time in milliseconds
pref ("security.mixed_content.hsts_priming_request_timeout", 2000);

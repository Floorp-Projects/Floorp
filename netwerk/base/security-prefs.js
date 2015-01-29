/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("security.tls.version.min", 1);
pref("security.tls.version.max", 3);
pref("security.tls.version.fallback-limit", 3);
# Do not add a site without filing a corresponding evangelism bug.
# bug 1095507, www.kredodirect.com.ua
# bug 1111354, web3.secureinternetbank.com
# bug 1112110, cmypage.kuronekoyamato.co.jp
# bug 1115883, www.timewarnercable.com and wayfarer.timewarnercable.com
# bug 1126652, www.animate-onlineshop.jp
# bug 1126654, www.gamers-onlineshop.jp
pref("security.tls.insecure_fallback_hosts", "www.kredodirect.com.ua,web3.secureinternetbank.com,cmypage.kuronekoyamato.co.jp,www.timewarnercable.com,wayfarer.timewarnercable.com,www.animate-onlineshop.jp,www.gamers-onlineshop.jp");

pref("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref", false);
pref("security.ssl.renego_unrestricted_hosts", "");
pref("security.ssl.treat_unsafe_negotiation_as_broken", false);
pref("security.ssl.require_safe_negotiation",  false);
pref("security.ssl.warn_missing_rfc5746",  1);
pref("security.ssl.enable_ocsp_stapling", true);
pref("security.ssl.enable_false_start", true);
pref("security.ssl.false_start.require-npn", false);
pref("security.ssl.enable_npn", true);
pref("security.ssl.enable_alpn", true);

pref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_rsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_sha", true);
pref("security.ssl3.dhe_rsa_aes_128_sha", true);
pref("security.ssl3.dhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_rsa_rc4_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_rc4_128_sha", true);
pref("security.ssl3.rsa_aes_128_sha", true);
pref("security.ssl3.rsa_aes_256_sha", true);
pref("security.ssl3.rsa_des_ede3_sha", true);
pref("security.ssl3.rsa_rc4_128_sha", true);
pref("security.ssl3.rsa_rc4_128_md5", true);

pref("security.default_personal_cert",   "Ask Every Time");
pref("security.remember_cert_checkbox_default_setting", true);
pref("security.ask_for_password",        0);
pref("security.password_lifetime",       30);

pref("security.OCSP.enabled", 1);
pref("security.OCSP.require", false);
pref("security.OCSP.GET.enabled", false);

pref("security.ssl.errorReporting.enabled", true);
pref("security.ssl.errorReporting.url", "https://data.mozilla.com/submit/sslreports");
pref("security.ssl.errorReporting.automatic", false);

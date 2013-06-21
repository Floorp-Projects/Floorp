/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pref("security.tls.version.min", 0);
pref("security.tls.version.max", 1);
pref("security.enable_tls_session_tickets", true);
pref("security.enable_md5_signatures", false);

pref("security.ssl.allow_unrestricted_renego_everywhere__temporarily_available_pref", false);
pref("security.ssl.renego_unrestricted_hosts", "");
pref("security.ssl.treat_unsafe_negotiation_as_broken", false);
pref("security.ssl.require_safe_negotiation",  false);
pref("security.ssl.warn_missing_rfc5746",  1);
pref("security.ssl.enable_false_start", false);

pref("security.ssl3.rsa_rc4_128_md5", true);
pref("security.ssl3.rsa_rc4_128_sha", true);
pref("security.ssl3.rsa_fips_des_ede3_sha", true);
pref("security.ssl3.rsa_des_ede3_sha", true);
pref("security.ssl3.dhe_rsa_camellia_256_sha", true);
pref("security.ssl3.dhe_dss_camellia_256_sha", true);
pref("security.ssl3.rsa_camellia_256_sha", true);
pref("security.ssl3.dhe_rsa_camellia_128_sha", true);
pref("security.ssl3.dhe_dss_camellia_128_sha", true);
pref("security.ssl3.rsa_camellia_128_sha", true);
pref("security.ssl3.dhe_rsa_aes_256_sha", true);
pref("security.ssl3.dhe_dss_aes_256_sha", true);
pref("security.ssl3.rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_des_ede3_sha", true);
pref("security.ssl3.ecdhe_ecdsa_rc4_128_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_rsa_des_ede3_sha", true);
pref("security.ssl3.ecdhe_rsa_rc4_128_sha", true);
pref("security.ssl3.ecdh_ecdsa_aes_256_sha", true);
pref("security.ssl3.ecdh_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdh_ecdsa_des_ede3_sha", true);
pref("security.ssl3.ecdh_ecdsa_rc4_128_sha", true);
pref("security.ssl3.ecdh_rsa_aes_256_sha", true);
pref("security.ssl3.ecdh_rsa_aes_128_sha", true);
pref("security.ssl3.ecdh_rsa_des_ede3_sha", true);
pref("security.ssl3.ecdh_rsa_rc4_128_sha", true);
pref("security.ssl3.dhe_rsa_aes_128_sha", true);
pref("security.ssl3.dhe_dss_aes_128_sha", true);
pref("security.ssl3.rsa_aes_128_sha", true);
pref("security.ssl3.dhe_rsa_des_ede3_sha", true);
pref("security.ssl3.dhe_dss_des_ede3_sha", true);
pref("security.ssl3.rsa_seed_sha", true);

pref("security.default_personal_cert",   "Ask Every Time");
pref("security.remember_cert_checkbox_default_setting", true);
pref("security.ask_for_password",        0);
pref("security.password_lifetime",       30);

pref("security.OCSP.enabled", 1);
pref("security.OCSP.require", false);

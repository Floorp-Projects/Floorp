/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

pref("security.ask_for_fortezza",           0);
pref("security.ask_for_password",           0);
pref("security.ciphers",                    "");
pref("security.default_mail_cert",          "");
pref("security.default_personal_cert",      "");
pref("security.email_as_ftp_password",      false);
pref("security.enable_java",                true);
pref("security.lower_java_network_security_by_trusting_proxies", false);
pref("security.enable_ssl2",                true);
pref("security.enable_ssl3",                true);
pref("security.fortezza_lifetime",          30);
pref("security.password_lifetime",          30);
pref("security.submit_email_forms",         true);
pref("security.use_password",               true);
pref("security.warn_accept_cookie",         false); // ML obsolete
pref("security.warn_entering_secure",       true);
pref("security.warn_leaving_secure",        true);
pref("security.warn_submit_insecure",       true);
pref("security.warn_viewing_mixed",         true);

pref("security.ssl2.des_ede3_192",          true);  // Domestic
pref("security.ssl2.des_64",                true);  // Domestic
pref("security.ssl2.rc2_128",               true);  // Domestic
pref("security.ssl2.rc2_40",                true);  // Export
pref("security.ssl2.rc4_40",                true);  // Export
pref("security.ssl2.rc4_128",               true);  // Domestic

pref("security.ssl3.fortezza_fortezza_sha", true);  // Fortezza
pref("security.ssl3.fortezza_null_sha",     false); // Fortezza
pref("security.ssl3.fortezza_rc4_sha",      true);  // Fortezza
pref("security.ssl3.rsa_des_ede3_sha",      true);  // Domestic
pref("security.ssl3.rsa_des_sha",           true);  // Domestic
pref("security.ssl3.rsa_null_md5",          false); // Export
pref("security.ssl3.rsa_rc2_40_md5",        true);  // Export
pref("security.ssl3.rsa_rc4_40_md5",        true);  // Export
pref("security.ssl3.rsa_rc4_128_md5",       true);  // Domestic
pref("security.ssl3.rsa_fips_des_ede3_sha", true);  // Domestic
pref("security.ssl3.rsa_fips_des_sha",      true);  // Domestic

pref("security.smime.des_ede3",             true);  // Domestic
pref("security.smime.des",                  true);  // Domestic
pref("security.smime.rc2_40",               true);  // Export
pref("security.smime.rc2_64",               true);  // Domestic
pref("security.smime.rc2_128",              true);  // Domestic
pref("security.smime.rc5.b64r16k40",        true);
pref("security.smime.rc5.b64r16k64",        true);
pref("security.smime.rc5.b64r16k128",       true);
pref("security.smime.fortezza",             true);  // Fortezza

pref("security.canotices.VeriSignInc.notice1",
"This certificate incorporates the VeriSign Certification Practice Statement "+
"(CPS) by reference. Use of this certificate is governed by the CPS.\n\n"+
"The CPS is available in the VeriSign repository at "+
"https://www.verisign.com/repository/CPS and "+
"ftp://ftp.verisign.com/repository/CPS; by E-mail at "+
"CPS-requests@verisign.com; and by mail at VeriSign, Inc., 2593 Coast Ave., "+
"Mountain View, CA 94043 USA, Attn: Certification Services.\n\n"+
"THE CPS DISCLAIMS AND LIMITS CERTAIN LIABILITIES, INCLUDING CONSEQUENTIAL "+
"AND PUNITIVE DAMAGES. THE CPS ALSO INCLUDES CAPS ON LIABILITY RELATED TO "+
"THIS CERTIFICATE. SEE THE CPS FOR DETAILS.\n\n"+
"The CPS and this certificate are copyrighted: Copyright (c) 1997 VeriSign, "+
"Inc. All Rights Reserved.");

pref("security.canotices.VeriSignInc.notice2",
"The subject name (i.e., user name) in this VeriSign Class 1 "+
"certificate contains nonverified subscriber information.");

pref("security.canotices.VeriSignInc.notice3",
"This certificate incorporates by reference, and its use is strictly subject "+
"to, the VeriSign Certification Practice Statement (CPS), available in the "+
"VeriSign repository at: https://www.verisign.co.jp; by E-mail at "+
"CPS-requests@verisign.co.jp; or by mail at VeriSign Japan K.K, 580-16 "+
"Horikawacho, Saiwai-ku, Kawasaki  210  Japan\n\n"+
"Copyright (c)1996, 1997 VeriSign, Inc.  All Rights Reserved. CERTAIN "+
"WARRANTIES DISCLAIMED AND LIABILITY LIMITED.\n\n"+
"WARNING: THE USE OF THIS CERTIFICATE IS STRICTLY SUBJECT TO THE  VERISIGN "+
"CERTIFICATION PRACTICE STATEMENT.  THE ISSUING AUTHORITY DISCLAIMS CERTAIN "+
"IMPLIED AND EXPRESS WARRANTIES, INCLUDING WARRANTIES OF MERCHANTABILITY OR "+
"FITNESS FOR A PARTICULAR PURPOSE, AND WILL NOT BE LIABLE FOR CONSEQUENTIAL, "+
"PUNITIVE, AND CERTAIN OTHER DAMAGES. SEE THE CPS FOR DETAILS.\n\n"+
"Contents of the VeriSign registered nonverifiedSubjectAttributes extension "+
"value shall not be considered as accurate information validated by the IA.");

pref("signed.applets.DefaultTo30Security",  false);
pref("signed.applets.securityIsOn",         true);
pref("signed.applets.codebase_principal_support", true);
pref("signed.applets.local_classes_have_30_powers", false);
pref("signed.applets.capabilitiesDB.lock_to_current", false);
pref("signed.applets.low_security_for_local_classes", false);
pref("signed.applets.simulate_signatures_on_system_classes", true);
pref("signed.applets.verbose_security_exception", false);

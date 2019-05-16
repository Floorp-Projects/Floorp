/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.AppConstants.Versions;

/**
 * Constant values common to all Android services.
 */
public class GlobalConstants {
  public static final String BROWSER_INTENT_PACKAGE = AppConstants.ANDROID_PACKAGE_NAME;
  public static final String BROWSER_INTENT_CLASS = AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS;

  public static final int SHARED_PREFERENCES_MODE = 0;

  // Common time values.
  public static final long MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
  public static final long MILLISECONDS_PER_SIX_MONTHS = 180 * MILLISECONDS_PER_DAY;

  // Acceptable cipher suites.
  /**
   * We support only a very limited range of strong cipher suites and protocols:
   * no SSLv3 or TLSv1.0 (if we can), no DHE ciphers that might be vulnerable to Logjam
   * (https://weakdh.org/), no RC4.
   *
   * Backstory: Bug 717691 (we no longer support Android 2.2, so the name
   * workaround is unnecessary), Bug 1081953, Bug 1061273, Bug 1166839.
   *
   * See <http://developer.android.com/reference/javax/net/ssl/SSLSocket.html> for
   * supported Android versions for each set of protocols and cipher suites.
   *
   * Note that currently we need to support connections to Sync 1.1 on Mozilla-hosted infra,
   * as well as connections to FxA and Sync 1.5 on AWS.
   *
   * ELB cipher suites:
   * <http://docs.aws.amazon.com/ElasticLoadBalancing/latest/DeveloperGuide/elb-security-policy-table.html>
   */
  public static final String[] DEFAULT_CIPHER_SUITES;
  public static final String[] DEFAULT_PROTOCOLS;

  static {
    // ChaCha20-Poly1305 seems fastest on mobile.
    // Otherwise prioritize 128 over 256 as a tradeoff between device CPU/battery
    // and the minor increase in strength.
    if (Versions.feature29Plus) {
      DEFAULT_CIPHER_SUITES = new String[]
          {
          "TLS_CHACHA20_POLY1305_SHA256",               // 29+
          "TLS_AES_128_GCM_SHA256",                     // 29+
          "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",      // 20+
          "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",         // 20+
          "TLS_AES_256_GCM_SHA384",                     // 29+
          "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",      // 20+
          "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",         // 11+
          };
    } else if (Versions.feature26Plus) {
      DEFAULT_CIPHER_SUITES = new String[]
          {
           "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",   // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",        // 11+
           "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",        // 11+

           // For Sync 1.1.
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    } else if (Versions.feature20Plus) {
      DEFAULT_CIPHER_SUITES = new String[]
          {
           "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256",   // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",        // 11+
           "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384",     // 20+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",        // 11+

           // For Sync 1.1.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",  // 9-25
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    } else {
      DEFAULT_CIPHER_SUITES = new String[]
          {
           "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA",        // 11+
           "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA",      // 11+
           "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA",        // 11+

           // For Sync 1.1.
           "TLS_DHE_RSA_WITH_AES_128_CBC_SHA",  // 9+
           "TLS_RSA_WITH_AES_128_CBC_SHA",      // 9+
          };
    }

    if (Versions.feature29Plus) {
      DEFAULT_PROTOCOLS = new String[]
          {
          "TLSv1.3",
          "TLSv1.2",
          };
    } else {
      DEFAULT_PROTOCOLS = new String[]
          {
           "TLSv1.2",
           "TLSv1.1",
           "TLSv1",             // We would like to remove this, and will do so when we can.
          };
    }
  }
}

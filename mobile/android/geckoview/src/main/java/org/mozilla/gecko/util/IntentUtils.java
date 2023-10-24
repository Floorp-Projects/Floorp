/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.util;

import android.content.Intent;
import android.net.Uri;
import java.net.URISyntaxException;
import java.util.Locale;

/** Utilities for Intents. */
public class IntentUtils {
  private IntentUtils() {}

  /**
   * Return a Uri instance which is equivalent to uri, but with a guaranteed-lowercase scheme as if
   * the API level 16 method Uri.normalizeScheme had been called.
   *
   * @param uri The URI string to normalize.
   * @return The corresponding normalized Uri.
   */
  private static Uri normalizeUriScheme(final Uri uri) {
    final String scheme = uri.getScheme();
    if (scheme == null) {
      return uri;
    }
    final String lower = scheme.toLowerCase(Locale.ROOT);
    if (lower.equals(scheme)) {
      return uri;
    }

    // Otherwise, return a new URI with a normalized scheme.
    return uri.buildUpon().scheme(lower).build();
  }

  /**
   * Return a normalized Uri instance that corresponds to the given URI string with cross-API-level
   * compatibility.
   *
   * @param aUri The URI string to normalize.
   * @return The corresponding normalized Uri.
   */
  public static Uri normalizeUri(final String aUri) {
    return normalizeUriScheme(
        aUri.indexOf(':') >= 0 ? Uri.parse(aUri) : new Uri.Builder().scheme(aUri).build());
  }

  public static boolean isUriSafeForScheme(final String aUri) {
    return isUriSafeForScheme(normalizeUri(aUri));
  }

  /**
   * Verify whether the given URI is considered safe to load in respect to its scheme. Unsafe URIs
   * should be blocked from further handling.
   *
   * @param aUri The URI instance to test.
   * @return Whether the provided URI is considered safe in respect to its scheme.
   */
  public static boolean isUriSafeForScheme(final Uri aUri) {
    final String scheme = aUri.getScheme();
    if ("tel".equals(scheme) || "sms".equals(scheme)) {
      // Bug 794034 - We don't want to pass MWI or USSD codes to the
      // dialer, and ensure the Uri class doesn't parse a URI
      // containing a fragment ('#')
      final String number = aUri.getSchemeSpecificPart();
      if (number.contains("#") || number.contains("*") || aUri.getFragment() != null) {
        return false;
      }
    }

    if (("intent".equals(scheme) || "android-app".equals(scheme))) {
      // Bug 1356893 - Rject intents with file data schemes.
      return getSafeIntent(aUri) != null;
    }

    return true;
  }

  /**
   * Create a safe intent for the given URI. Intents with file data schemes are considered unsafe.
   *
   * @param aUri The URI for the intent.
   * @return A safe intent for the given URI or null if URI is considered unsafe.
   */
  public static Intent getSafeIntent(final Uri aUri) {
    final Intent intent;
    try {
      intent = Intent.parseUri(aUri.toString(), 0);
    } catch (final URISyntaxException e) {
      return null;
    }

    final Uri data = intent.getData();
    if (data != null && "file".equals(normalizeUriScheme(data).getScheme())) {
      return null;
    }

    // Only open applications which can accept arbitrary data from a browser.
    intent.addCategory(Intent.CATEGORY_BROWSABLE);

    // Prevent site from explicitly opening our internal activities,
    // which can leak data.
    intent.setComponent(null);
    nullIntentSelector(intent);

    return intent;
  }

  // We create a separate method to better encapsulate the @TargetApi use.
  private static void nullIntentSelector(final Intent intent) {
    intent.setSelector(null);
  }
}

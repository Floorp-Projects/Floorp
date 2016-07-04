/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class WebURLFinder {
  /**
   * These regular expressions are taken from Android's Patterns.java.
   * We brought them in to standardize URL matching across Android versions, instead of relying
   * on Android version-dependent built-ins that can vary across Android versions.
   * The original code can be found here:
   * http://androidxref.com/6.0.1_r10/xref/frameworks/base/core/java/android/util/Patterns.java
   *
   */
  public static final String GOOD_IRI_CHAR = "a-zA-Z0-9\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";
  public static final String GOOD_GTLD_CHAR = "a-zA-Z\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";
  public static final String IRI = "[" + GOOD_IRI_CHAR + "]([" + GOOD_IRI_CHAR + "\\-]{0,61}[" + GOOD_IRI_CHAR + "]){0,1}";
  public static final String GTLD = "[" + GOOD_GTLD_CHAR + "]{2,63}";
  public static final String HOST_NAME = "(" + IRI + "\\.)+" + GTLD;
  public static final Pattern IP_ADDRESS = Pattern.compile("((25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9])\\.(25[0-5]|2[0-4]"
          + "[0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]"
          + "[0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}"
          + "|[1-9][0-9]|[0-9]))");
  public static final Pattern DOMAIN_NAME = Pattern.compile("(" + HOST_NAME + "|" + IP_ADDRESS + ")");
  public static final Pattern WEB_URL = Pattern.compile("((?:(http|https|Http|Https|rtsp|Rtsp):\\/\\/(?:(?:[a-zA-Z0-9\\$\\-\\_\\.\\+\\!\\*\\'\\(\\)"
          + "\\,\\;\\?\\&\\=]|(?:\\%[a-fA-F0-9]{2})){1,64}(?:\\:(?:[a-zA-Z0-9\\$\\-\\_"
          + "\\.\\+\\!\\*\\'\\(\\)\\,\\;\\?\\&\\=]|(?:\\%[a-fA-F0-9]{2})){1,25})?\\@)?)?"
          + "(?:" + DOMAIN_NAME + ")"
          + "(?:\\:\\d{1,5})?)"
          + "(\\/(?:(?:[" + GOOD_IRI_CHAR + "\\;\\/\\?\\:\\@\\&\\=\\#\\~"
          + "\\-\\.\\+\\!\\*\\'\\(\\)\\,\\_])|(?:\\%[a-fA-F0-9]{2}))*)?"
          + "(?:\\b|$)");

  public final List<String> candidates;

  public WebURLFinder(String string) {
    if (string == null) {
      throw new IllegalArgumentException("string must not be null");
    }

    this.candidates = candidateWebURLs(string);
  }

  public WebURLFinder(List<String> strings) {
    if (strings == null) {
      throw new IllegalArgumentException("strings must not be null");
    }

    this.candidates = candidateWebURLs(strings);
  }

  /**
   * Check if string is a Web URL.
   * <p>
   * A Web URL is a URI that is not a <code>file:</code> or
   * <code>javascript:</code> scheme.
   *
   * @param string
   *          to check.
   * @return <code>true</code> if <code>string</code> is a Web URL.
   */
  public static boolean isWebURL(String string) {
    try {
      new URI(string);
    } catch (Exception e) {
      return false;
    }

    if (android.webkit.URLUtil.isFileUrl(string) ||
        android.webkit.URLUtil.isJavaScriptUrl(string)) {
      return false;
    }

    return true;
  }

  /**
   * Return best Web URL.
   * <p>
   * "Best" means a Web URL with a scheme, and failing that, a Web URL without a
   * scheme.
   *
   * @return a Web URL or <code>null</code>.
   */
  public String bestWebURL() {
    String firstWebURLWithScheme = firstWebURLWithScheme();
    if (firstWebURLWithScheme != null) {
      return firstWebURLWithScheme;
    }

    return firstWebURLWithoutScheme();
  }

  protected static List<String> candidateWebURLs(Collection<String> strings) {
    List<String> candidates = new ArrayList<String>();

    for (String string : strings) {
      if (string == null) {
        continue;
      }

      candidates.addAll(candidateWebURLs(string));
    }

    return candidates;
  }

  protected static List<String> candidateWebURLs(String string) {
    Matcher matcher = WEB_URL.matcher(string);
    List<String> matches = new LinkedList<String>();

    while (matcher.find()) {
      // Remove URLs with bad schemes.
      if (!isWebURL(matcher.group())) {
        continue;
      }

      // Remove parts of email addresses.
      if (matcher.start() > 0 && (string.charAt(matcher.start() - 1) == '@')) {
        continue;
      }

      matches.add(matcher.group());
    }

    return matches;
  }

  protected String firstWebURLWithScheme() {
    for (String match : candidates) {
      try {
        if (new URI(match).getScheme() != null) {
          return match;
        }
      } catch (URISyntaxException e) {
        // Ignore: on to the next.
        continue;
      }
    }

    return null;
  }

  protected String firstWebURLWithoutScheme() {
    if (!candidates.isEmpty()) {
      return candidates.get(0);
    }

    return null;
  }
}

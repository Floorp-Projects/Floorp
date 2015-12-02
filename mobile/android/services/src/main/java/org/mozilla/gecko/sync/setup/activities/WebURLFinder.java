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

import android.util.Patterns;

public class WebURLFinder {
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
    Matcher matcher = Patterns.WEB_URL.matcher(string);
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

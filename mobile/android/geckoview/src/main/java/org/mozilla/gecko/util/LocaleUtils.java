/* -*- Mode: Java; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.text.TextUtils;
import java.util.Locale;

public class LocaleUtils {
  // Locale.getLanguage() may return legacy language code until Java 17
  // https://developer.android.com/reference/java/util/Locale#legacy_language_codes
  public static String getLanguageTagForAcceptLanguage(final Locale locale) {
    String language = locale.getLanguage();
    if (language.equals("in")) {
      language = "id";
    } else if (language.equals("iw")) {
      language = "he";
    } else if (language.equals("ji")) {
      language = "yi";
    }
    final StringBuilder out = new StringBuilder(language);
    final String country = locale.getCountry();
    if (!TextUtils.isEmpty(country)) {
      out.append('-').append(country);
    }
    // e.g. "en", "en-US"
    return out.toString();
  }
}

/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.util;

import static org.junit.Assert.*;

import android.test.suitebuilder.annotation.SmallTest;
import java.util.Locale;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
@SmallTest
public class LocaleUtilsTest {
  @Test
  public void languageTagForAcceptLanguage() {
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("zn-Hans-CN")), "zn-CN");
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("zn-Hant-TW")), "zn-TW");

    // If builder is Java 17+, Locale.getLanguage doesn't repelace with old language code.
    // But we should keep this to make things understandable.
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("id-ID")), "id-ID");
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("in-ID")), "id-ID");

    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("yi-US")), "yi-US");
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("ji-US")), "yi-US");

    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("he-IL")), "he-IL");
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(Locale.forLanguageTag("iw-IL")), "he-IL");

    // Android 14 may add extension (Bug 1873578)
    assertEquals(
        LocaleUtils.getLanguageTagForAcceptLanguage(
            Locale.forLanguageTag("en-US-u-fw-mon-mu-celsius")),
        "en-US");
  }
}

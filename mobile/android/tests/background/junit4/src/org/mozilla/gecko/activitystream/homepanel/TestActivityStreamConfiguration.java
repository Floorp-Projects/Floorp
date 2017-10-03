/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

import static android.R.attr.tag;
import static junit.framework.Assert.*;

@RunWith(TestRunner.class)
public class TestActivityStreamConfiguration {

    @Test
    public void testIsPocketEnabledByLocaleInnerForWhitelistedLocaleTags() throws Exception {
        for (final String tag : ActivityStreamConfiguration.pocketEnabledLocaleTags) {
            final Locale whitelistedLocale = getLocaleFromLanguageTag(tag);
            assertTrue("Expected Pocket enabled for locale: " + tag,
                    ActivityStreamConfiguration.isPocketEnabledByLocaleInner(whitelistedLocale));
        }
    }

    @Test
    public void testIsPocketEnabledByLocaleInnerForNonWhitelistedLocaleTags() throws Exception {
        final String nonWhitelistedLocaleTag = "sbp-TZ"; // Sangu, chosen b/c I think it'll take a while for us to have Pocket feeds for it.
        if (Arrays.binarySearch(ActivityStreamConfiguration.pocketEnabledLocaleTags, nonWhitelistedLocaleTag) >= 0) {
            throw new IllegalStateException("Precondition failed: locale, " + nonWhitelistedLocaleTag + ", has been " +
                    "added to whitelisted locales. Please choose a new tag.");
        }

        final Locale nonWhitelistedLocale = getLocaleFromLanguageTag(nonWhitelistedLocaleTag);
        assertFalse("Expected Pocket disabled for locale: " + nonWhitelistedLocaleTag,
                ActivityStreamConfiguration.isPocketEnabledByLocaleInner(nonWhitelistedLocale));
    }

    @Test
    public void testIsPocketEnabledByLocaleInnerEnglishVariant() throws Exception {
        // Oxford english spelling variant via https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry.
        final Locale enGBVariant = new Locale("en", "gb", "oxendict");

        // See ActivityStreamConfiguration.isPocketenabledByLocaleInner for reasoning behind assertFalse.
        assertFalse("Expected Pocket disabled for locale: " + enGBVariant.toLanguageTag(),
                ActivityStreamConfiguration.isPocketEnabledByLocaleInner(enGBVariant));
    }

    /**
     * Gets the {@link Locale} from the language tag, e.g. en-US. This duplicates
     * {@link org.mozilla.gecko.Locales#parseLocaleCode(String)} which is intentional since that is not the method we're
     * trying to test.
     */
    private Locale getLocaleFromLanguageTag(final String tag) {
        final String[] split = tag.split("-");
        if (split.length == 1) {
            return new Locale(split[0]);
        } else {
            return new Locale(split[0], split[1]);
        }
    }
}
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale;

import org.junit.Test;

import java.util.Locale;

import static org.junit.Assert.assertEquals;

public class LocalesTest {
    @Test
    public void testLanguage() {
        assertEquals("en", Locales.getLanguage(Locale.getDefault()));
    }

    @Test
    public void testHebrewIsrael() {
        final Locale locale = new Locale("iw", "IL");

        assertEquals("he", Locales.getLanguage(locale));
        assertEquals("he-IL", Locales.getLanguageTag(locale));
    }

    @Test
    public void testIndonesianIndonesia() {
        final Locale locale = new Locale("in", "ID");

        assertEquals("id", Locales.getLanguage(locale));
        assertEquals("id-ID", Locales.getLanguageTag(locale));
    }

    @Test
    public void testYiddishUnitedStates() {
        final Locale locale = new Locale("ji", "US");

        assertEquals("yi", Locales.getLanguage(locale));
        assertEquals("yi-US", Locales.getLanguageTag(locale));
    }

    @Test
    public void testEmptyCountry() {
        final Locale locale = new Locale("en");

        assertEquals("en", Locales.getLanguage(locale));
        assertEquals("en", Locales.getLanguageTag(locale));
    }
}
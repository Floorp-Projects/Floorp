/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.focus.locale.Locales.getLanguage
import org.mozilla.focus.locale.Locales.getLanguageTag
import java.util.Locale

class LocalesTest {
    @Test
    fun testLanguage() {
        assertEquals("en", getLanguage(Locale.getDefault()))
    }

    @Test
    fun testHebrewIsrael() {
        val locale = Locale("iw", "IL")
        assertEquals("he", getLanguage(locale))
        assertEquals("he-IL", getLanguageTag(locale))
    }

    @Test
    fun testIndonesianIndonesia() {
        val locale = Locale("in", "ID")
        assertEquals("id", getLanguage(locale))
        assertEquals("id-ID", getLanguageTag(locale))
    }

    @Test
    fun testYiddishUnitedStates() {
        val locale = Locale("ji", "US")
        assertEquals("yi", getLanguage(locale))
        assertEquals("yi-US", getLanguageTag(locale))
    }

    @Test
    fun testEmptyCountry() {
        val locale = Locale("en")
        assertEquals("en", getLanguage(locale))
        assertEquals("en", getLanguageTag(locale))
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.LanguageSetting.ALWAYS
import mozilla.components.concept.engine.translate.LanguageSetting.NEVER
import mozilla.components.concept.engine.translate.LanguageSetting.OFFER
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.concept.engine.translate.findLanguage
import mozilla.components.concept.engine.translate.mapLanguageSettings
import mozilla.components.concept.engine.translate.toLanguageMap
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TranslationSupportTest {

    private val spanish = Language(code = "es", "Spanish")
    private val english = Language(code = "en", "English")
    private val german = Language(code = "de", "German")

    @Test
    fun `GIVEN a populated TranslationSupport THEN map to a language map`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = listOf(english, spanish),
            toLanguages = listOf(english, spanish),
        )

        val map = translationsSupport.toLanguageMap()

        assertTrue(map!!.contains(spanish.code))
        assertTrue(map.contains(english.code))
        assertEquals(map.size, 2)
    }

    @Test
    fun `GIVEN a partially populated TranslationSupport THEN map to a language map`() {
        val supportToPopulated = TranslationSupport(
            fromLanguages = null,
            toLanguages = listOf(english, spanish),
        )

        val toMap = supportToPopulated.toLanguageMap()
        assertTrue(toMap!!.contains(spanish.code))
        assertTrue(toMap.contains(english.code))
        assertEquals(toMap.size, 2)

        val supportFromPopulated = TranslationSupport(
            fromLanguages = listOf(english, spanish),
            toLanguages = null,
        )

        val fromMap = supportFromPopulated.toLanguageMap()
        assertTrue(fromMap!!.contains(spanish.code))
        assertTrue(fromMap.contains(english.code))
        assertEquals(fromMap.size, 2)
    }

    @Test
    fun `GIVEN a null TranslationSupport THEN map to a null language map`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = null,
            toLanguages = null,
        )

        val map = translationsSupport.toLanguageMap()
        assertNull(map)
    }

    @Test
    fun `GIVEN a populated TranslationSupport THEN find a language`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = listOf(spanish, english),
            toLanguages = listOf(spanish, english),
        )

        assertEquals(translationsSupport.findLanguage("es"), spanish)
        assertEquals(translationsSupport.findLanguage("en"), english)
        assertNull(translationsSupport.findLanguage("de"))
    }

    @Test
    fun `GIVEN a null TranslationSupport THEN do not find a language`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = null,
            toLanguages = null,
        )

        assertNull(translationsSupport.findLanguage("es"))
    }

    @Test
    fun `GIVEN a populated TranslationSupport THEN map the language settings`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = listOf(spanish, english, german),
            toLanguages = listOf(spanish, english),
        )

        val languageSettings = mapOf<String, LanguageSetting>(
            spanish.code to ALWAYS,
            english.code to NEVER,
            german.code to OFFER,
            "some unknown code" to OFFER,
            "some unknown code2" to OFFER,
        )

        val map = translationsSupport.mapLanguageSettings(languageSettings)
        assertTrue(map!!.contains(spanish))
        assertTrue(map.contains(english))
        assertTrue(map.contains(german))
        assertEquals(map.size, 3)
    }

    @Test
    fun `GIVEN an unpopulated TranslationSupport THEN map the language settings`() {
        val translationsSupport = TranslationSupport(
            fromLanguages = null,
            toLanguages = null,
        )

        val languageSettings = mapOf<String, LanguageSetting>(
            spanish.code to ALWAYS,
            english.code to NEVER,
            german.code to OFFER,
            "some unknown code" to OFFER,
            "some unknown code2" to OFFER,
        )

        val map = translationsSupport.mapLanguageSettings(languageSettings)
        assertFalse(map!!.contains(spanish))
        assertFalse(map.contains(english))
        assertFalse(map.contains(german))
        assertEquals(map.size, 0)
    }
}

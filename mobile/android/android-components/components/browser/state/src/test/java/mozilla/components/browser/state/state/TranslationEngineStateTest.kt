/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationPair
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

private val mockCandidateLanguages = listOf(
    Language("de", "German"),
    Language("fr", "French"),
    Language("en", "English"),
    Language("es", "Spanish"),
)
class TranslationEngineStateTest {
    @Test
    fun `GIVEN an untranslated case THEN initialToLanguage and initialFromLanguage match the expected tags`() {
        val detectedLanguages = DetectedLanguages(
            documentLangTag = "en",
            supportedDocumentLang = true,
            userPreferredLangTag = "es",
        )

        val translationEngineState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = null,
            isEngineReady = true,
            requestedTranslationPair = null,
        )

        assertEquals(translationEngineState.initialToLanguage(mockCandidateLanguages)?.code, "es")
        assertEquals(translationEngineState.initialFromLanguage(mockCandidateLanguages)?.code, "en")
    }

    @Test
    fun `GIVEN a translated case THEN initialToLanguage and initialFromLanguage match the translated tags`() {
        val detectedLanguages = DetectedLanguages(
            documentLangTag = "en",
            supportedDocumentLang = true,
            userPreferredLangTag = "es",
        )

        val translationPair = TranslationPair(
            fromLanguage = "fr",
            toLanguage = "de",
        )

        val translationEngineState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = null,
            isEngineReady = true,
            requestedTranslationPair = translationPair,
        )

        assertEquals(translationEngineState.initialToLanguage(mockCandidateLanguages)?.code, "de")
        assertEquals(translationEngineState.initialFromLanguage(mockCandidateLanguages)?.code, "fr")
    }

    @Test
    fun `GIVEN invalid codes WHEN not translated THEN initialToLanguage and initialFromLanguage are null`() {
        val detectedLanguages = DetectedLanguages(
            documentLangTag = "not-a-code",
            supportedDocumentLang = true,
            userPreferredLangTag = "not-a-code",
        )

        val translationEngineState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = null,
            isEngineReady = true,
            requestedTranslationPair = null,
        )

        assertNull(translationEngineState.initialToLanguage(mockCandidateLanguages))
        assertNull(translationEngineState.initialFromLanguage(mockCandidateLanguages))
    }

    @Test
    fun `GIVEN invalid codes WHEN translated THEN initialToLanguage and initialFromLanguage are null`() {
        val detectedLanguages = DetectedLanguages(
            documentLangTag = "en",
            supportedDocumentLang = true,
            userPreferredLangTag = "es",
        )

        // This would be a highly unexpected state
        val translationPair = TranslationPair(
            fromLanguage = "not-a-code",
            toLanguage = "not-a-code",
        )

        val translationEngineState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = null,
            isEngineReady = true,
            requestedTranslationPair = translationPair,
        )

        assertNull(translationEngineState.initialToLanguage(mockCandidateLanguages))
        assertNull(translationEngineState.initialFromLanguage(mockCandidateLanguages))
    }

    @Test
    fun `GIVEN unexpected THEN initialToLanguage and initialFromLanguage are null`() {
        val translationEngineState = TranslationEngineState(
            detectedLanguages = null,
            error = null,
            isEngineReady = true,
            requestedTranslationPair = null,
        )

        assertNull(translationEngineState.initialToLanguage(mockCandidateLanguages))
        assertNull(translationEngineState.initialFromLanguage(mockCandidateLanguages))
    }
}

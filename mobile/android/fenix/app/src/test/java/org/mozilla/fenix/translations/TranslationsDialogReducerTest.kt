/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations

import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationDownloadSize
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TranslationsDialogReducerTest {

    @Test
    fun `WHEN the reducer is called for UpdateFromSelectedLanguage THEN a new state with updated fromSelectedLanguage is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")

        val translationsDialogState = TranslationsDialogState(initialTo = spanishLanguage)

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateFromSelectedLanguage(englishLanguage),
        )

        assertEquals(englishLanguage, updatedState.initialFrom)
        assertEquals(PositiveButtonType.Enabled, updatedState.positiveButtonType)

        val updatedStateTwo = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateFromSelectedLanguage(spanishLanguage),
        )

        assertEquals(PositiveButtonType.Disabled, updatedStateTwo.positiveButtonType)
    }

    @Test
    fun `WHEN the reducer is called for UpdateToSelectedLanguage THEN a new state with updated toSelectedLanguage is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")

        val translationsDialogState = TranslationsDialogState(initialFrom = spanishLanguage)

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateToSelectedLanguage(englishLanguage),
        )

        assertEquals(englishLanguage, updatedState.initialTo)
        assertEquals(PositiveButtonType.Enabled, updatedState.positiveButtonType)

        val updatedStateTwo = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateToSelectedLanguage(spanishLanguage),
        )

        assertEquals(PositiveButtonType.Disabled, updatedStateTwo.positiveButtonType)
    }

    @Test
    fun `WHEN the reducer is called for UpdateTranslateToLanguages THEN a new state with updated translateToLanguages is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")

        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslateToLanguages(
                listOf(
                    spanishLanguage,
                    englishLanguage,
                ),
            ),
        )

        assertEquals(listOf(spanishLanguage, englishLanguage), updatedState.toLanguages)
    }

    @Test
    fun `WHEN the reducer is called for UpdateTranslateFromLanguages THEN a new state with updated translatefromLanguages is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")

        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslateFromLanguages(
                listOf(
                    spanishLanguage,
                    englishLanguage,
                ),
            ),
        )

        assertEquals(listOf(spanishLanguage, englishLanguage), updatedState.fromLanguages)
    }

    @Test
    fun `WHEN the reducer is called for DismissDialog THEN a new state with updated dismiss dialog is returned`() {
        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.DismissDialog(DismissDialogState.Dismiss),
        )

        assertEquals(DismissDialogState.Dismiss, updatedState.dismissDialogState)
    }

    @Test
    fun `WHEN the reducer is called for UpdateInProgress THEN a new state with translation in progress is returned`() {
        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslationInProgress(true),
        )

        assertEquals(true, updatedState.isTranslationInProgress)
        assertEquals(PositiveButtonType.InProgress, updatedState.positiveButtonType)
    }

    @Test
    fun `WHEN the reducer is called for InitTranslationsDialog THEN a new state for PositiveButtonType is returned`() {
        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.InitTranslationsDialog,
        )

        assertEquals(PositiveButtonType.Disabled, updatedState.positiveButtonType)

        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")
        val translationsDialogStateTwo = TranslationsDialogState(
            initialFrom = spanishLanguage,
            initialTo = englishLanguage,
            positiveButtonType = PositiveButtonType.Enabled,
        )

        val updatedStateTwo = TranslationsDialogReducer.reduce(
            translationsDialogStateTwo,
            TranslationsDialogAction.InitTranslationsDialog,
        )

        assertEquals(PositiveButtonType.Enabled, updatedStateTwo.positiveButtonType)
    }

    @Test
    fun `WHEN the reducer is called for UpdateTranslationError THEN a new state with translation error is returned`() {
        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslationError(
                TranslationError.LanguageNotSupportedError(
                    null,
                ),
            ),
        )

        assertTrue(updatedState.error is TranslationError.LanguageNotSupportedError)
        assertEquals(PositiveButtonType.Disabled, updatedState.positiveButtonType)

        val updatedStateTwo = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslationError(
                TranslationError.CouldNotLoadLanguagesError(
                    null,
                ),
            ),
        )

        assertTrue(updatedStateTwo.error is TranslationError.CouldNotLoadLanguagesError)
        assertEquals(PositiveButtonType.Enabled, updatedStateTwo.positiveButtonType)
    }

    @Test
    fun `WHEN the reducer is called for UpdateTranslated THEN a new state with translation translated is returned`() {
        val translationsDialogState = TranslationsDialogState()

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslated(
                true,
            ),
        )

        assertEquals(PositiveButtonType.Disabled, updatedState.positiveButtonType)
        assertEquals(true, updatedState.isTranslated)
    }

    @Test
    fun `WHEN the reducer is called for UpdateTranslatedPageTitle THEN a new state with translation title is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")
        val translationsDialogState =
            TranslationsDialogState(initialTo = englishLanguage, initialFrom = spanishLanguage)

        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateTranslatedPageTitle(
                testContext.getString(
                    R.string.translations_bottom_sheet_title_translation_completed,
                    spanishLanguage.localizedDisplayName,
                    englishLanguage.localizedDisplayName,
                ),
            ),
        )

        assertEquals(
            testContext.getString(
                R.string.translations_bottom_sheet_title_translation_completed,
                spanishLanguage.localizedDisplayName,
                englishLanguage.localizedDisplayName,
            ),
            updatedState.translatedPageTitle,
        )
    }

    @Test
    fun `WHEN the reducer is called for UpdateDownloadTranslationDownloadSize THEN a new state with translationDownloadSize is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")
        val translationsDialogState =
            TranslationsDialogState(initialTo = englishLanguage, initialFrom = spanishLanguage)
        val translationDownloadSize = TranslationDownloadSize(
            fromLanguage = spanishLanguage,
            toLanguage = englishLanguage,
            size = 1000L,
        )
        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateDownloadTranslationDownloadSize(
                translationDownloadSize,
            ),
        )

        assertEquals(
            translationDownloadSize,
            updatedState.translationDownloadSize,
        )
    }

    @Test
    fun `WHEN the reducer is called for UpdateDownloadTranslationDownloadSize with a invalid object THEN a new state with translationDownloadSize null is returned`() {
        val spanishLanguage = Language("es", "Spanish")
        val englishLanguage = Language("en", "English")
        val translationsDialogState =
            TranslationsDialogState(initialTo = englishLanguage, initialFrom = spanishLanguage)
        val translationDownloadSize = TranslationDownloadSize(
            fromLanguage = englishLanguage,
            toLanguage = spanishLanguage,
            size = 0L,
        )
        val updatedState = TranslationsDialogReducer.reduce(
            translationsDialogState,
            TranslationsDialogAction.UpdateDownloadTranslationDownloadSize(
                translationDownloadSize,
            ),
        )

        assertEquals(
            null,
            updatedState.translationDownloadSize,
        )
    }
}

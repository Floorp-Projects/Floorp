/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import java.lang.Exception

class TranslationsActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )
    }

    private fun tabState(): TabSessionState = store.state.findTab(tab.id)!!

    @Test
    fun `WHEN a TranslateExpectedAction is dispatched THEN update translation expected status`() {
        assertEquals(false, tabState().translationsState.isExpectedTranslate)

        store.dispatch(TranslationsAction.TranslateExpectedAction(tabId = tab.id))
            .joinBlocking()

        assertEquals(true, tabState().translationsState.isExpectedTranslate)
    }

    @Test
    fun `WHEN a TranslateOfferAction is dispatched THEN update translation expected status`() {
        assertEquals(false, tabState().translationsState.isOfferTranslate)

        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tab.id))
            .joinBlocking()

        assertEquals(true, tabState().translationsState.isOfferTranslate)
    }

    @Test
    fun `WHEN a TranslateStateChangeAction is dispatched THEN update translation expected status`() {
        assertEquals(null, tabState().translationsState.translationEngineState)

        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, mock()))
            .joinBlocking()

        assertEquals(true, tabState().translationsState.translationEngineState != null)
    }

    @Test
    fun `WHEN a TranslateAction is dispatched AND successful THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateAction(tabId = tab.id, "en", "es", null))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isTranslateProcessing)

        // Action success
        store.dispatch(TranslationsAction.TranslateSuccessAction(tabId = tab.id, operation = TranslationOperation.TRANSLATE))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isTranslateProcessing)
        assertEquals(true, tabState().translationsState.isTranslated)
        assertEquals(null, tabState().translationsState.translationError)
    }

    @Test
    fun `WHEN a TranslateAction is dispatched AND fails THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateAction(tabId = tab.id, "en", "es", null))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isTranslateProcessing)

        // Action failure
        val error = TranslationError.UnknownError(Exception())
        store.dispatch(TranslationsAction.TranslateExceptionAction(tabId = tab.id, operation = TranslationOperation.TRANSLATE, error))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isTranslateProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
        assertEquals(error, tabState().translationsState.translationError)
    }

    @Test
    fun `WHEN a TranslateRestoreAction is dispatched AND successful THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isRestoreProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateRestoreAction(tabId = tab.id))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isRestoreProcessing)

        // Action success
        store.dispatch(TranslationsAction.TranslateSuccessAction(tabId = tab.id, operation = TranslationOperation.RESTORE))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isRestoreProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
        assertEquals(null, tabState().translationsState.translationError)
    }

    @Test
    fun `WHEN a TranslateRestoreAction is dispatched AND fails THEN update translation processing status`() {
        // Initial
        assertEquals(false, tabState().translationsState.isRestoreProcessing)

        // Action started
        store.dispatch(TranslationsAction.TranslateRestoreAction(tabId = tab.id))
            .joinBlocking()
        assertEquals(true, tabState().translationsState.isRestoreProcessing)

        // Action failure
        val error = TranslationError.UnknownError(Exception())
        store.dispatch(TranslationsAction.TranslateExceptionAction(tabId = tab.id, operation = TranslationOperation.RESTORE, error))
            .joinBlocking()
        assertEquals(false, tabState().translationsState.isRestoreProcessing)
        assertEquals(false, tabState().translationsState.isTranslated)
        assertEquals(error, tabState().translationsState.translationError)
    }

    @Test
    fun `WHEN a TranslateSetLanguagesAction is dispatched AND successful THEN update supportedLanguages`() {
        // Initial
        assertEquals(null, tabState().translationsState.supportedLanguages)

        // Action started
        val toLanguage = Language("de", "German")
        val fromLanguage = Language("es", "Spanish")
        val supportedLanguages = TranslationSupport(listOf(fromLanguage), listOf(toLanguage))
        store.dispatch(
            TranslationsAction.TranslateSetLanguagesAction(
                tabId = tab.id,
                supportedLanguages = supportedLanguages,
            ),
        )
            .joinBlocking()

        // Action success
        assertEquals(supportedLanguages, tabState().translationsState.supportedLanguages)
    }

    @Test
    fun `WHEN a TranslateExceptionAction is dispatched due to an error THEN update the error condition according to the operation`() {
        // Initial state
        assertEquals(null, tabState().translationsState.translationError)

        // TRANSLATE usage
        val translateError = TranslationError.CouldNotLoadLanguagesError(null)
        store.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.TRANSLATE,
                translationError = translateError,
            ),
        ).joinBlocking()
        assertEquals(translateError, tabState().translationsState.translationError)

        // RESTORE usage
        val restoreError = TranslationError.CouldNotRestoreError(null)
        store.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.RESTORE,
                translationError = restoreError,
            ),
        ).joinBlocking()
        assertEquals(restoreError, tabState().translationsState.translationError)

        // FETCH_LANGUAGES usage
        val fetchError = TranslationError.CouldNotLoadLanguagesError(null)
        store.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_LANGUAGES,
                translationError = fetchError,
            ),
        ).joinBlocking()
        assertEquals(fetchError, tabState().translationsState.translationError)
    }

    @Test
    fun `WHEN a TranslateSuccessAction is dispatched THEN update the condition according to the operation`() {
        // Initial state
        assertEquals(null, tabState().translationsState.translationError)
        assertEquals(false, tabState().translationsState.isTranslated)
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // TRANSLATE usage
        store.dispatch(
            TranslationsAction.TranslateSuccessAction(
                tabId = tab.id,
                operation = TranslationOperation.TRANSLATE,
            ),
        ).joinBlocking()
        assertEquals(null, tabState().translationsState.translationError)
        assertEquals(true, tabState().translationsState.isTranslated)
        assertEquals(false, tabState().translationsState.isTranslateProcessing)

        // RESTORE usage
        store.dispatch(
            TranslationsAction.TranslateSuccessAction(
                tabId = tab.id,
                operation = TranslationOperation.RESTORE,
            ),
        ).joinBlocking()
        assertEquals(null, tabState().translationsState.translationError)
        assertEquals(false, tabState().translationsState.isTranslated)
        assertEquals(false, tabState().translationsState.isRestoreProcessing)

        // FETCH_LANGUAGES usage
        store.dispatch(
            TranslationsAction.TranslateSuccessAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_LANGUAGES,
            ),
        ).joinBlocking()
        assertEquals(null, tabState().translationsState.translationError)
        assertEquals(false, tabState().translationsState.isTranslated)
    }
}

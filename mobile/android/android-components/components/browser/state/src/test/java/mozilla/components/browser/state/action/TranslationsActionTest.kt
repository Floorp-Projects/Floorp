/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationDownloadSize
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPageSettingOperation
import mozilla.components.concept.engine.translate.TranslationPageSettings
import mozilla.components.concept.engine.translate.TranslationPair
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
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
    fun `WHEN a TranslateStateChangeAction is dispatched THEN the translation status updates accordingly`() {
        assertNull(tabState().translationsState.translationEngineState)
        assertFalse(tabState().translationsState.isTranslated)
        assertFalse(tabState().translationsState.isExpectedTranslate)

        val translatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = true, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = "es", toLanguage = "en"),
        )

        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()

        // Translated state
        assertEquals(translatedEngineState, tabState().translationsState.translationEngineState)
        assertTrue(tabState().translationsState.isTranslated)
        assertFalse(tabState().translationsState.isExpectedTranslate)

        val nonTranslatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = true, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = null, toLanguage = null),
        )

        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, nonTranslatedEngineState))
            .joinBlocking()

        // Non-translated state
        assertEquals(nonTranslatedEngineState, tabState().translationsState.translationEngineState)
        assertFalse(tabState().translationsState.isTranslated)
        assertFalse(tabState().translationsState.isExpectedTranslate)
    }

    @Test
    fun `WHEN a TranslateStateChangeAction is dispatched THEN the isExpectedTranslate status updates accordingly`() {
        // Initial State
        assertNull(tabState().translationsState.translationEngineState)

        // Sending an initial request to set state; however, the engine hasn't decided if it is an
        // expected state
        var translatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = true, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = "es", toLanguage = "en"),
        )
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertFalse(tabState().translationsState.isExpectedTranslate)

        // Engine is sending a translation expected action
        store.dispatch(TranslationsAction.TranslateExpectedAction(tabId = tab.id))
            .joinBlocking()

        // Initial expected translation state
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertTrue(tabState().translationsState.isExpectedTranslate)

        // Not expected translation state, because it is no longer supported
        translatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = false, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = "es", toLanguage = "en"),
        )

        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertFalse(tabState().translationsState.isExpectedTranslate)
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
    fun `WHEN a SetSupportedLanguagesAction is dispatched AND successful THEN update supportedLanguages`() {
        // Initial
        assertEquals(null, tabState().translationsState.supportedLanguages)

        // Action started
        val toLanguage = Language("de", "German")
        val fromLanguage = Language("es", "Spanish")
        val supportedLanguages = TranslationSupport(listOf(fromLanguage), listOf(toLanguage))
        store.dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                tabId = tab.id,
                supportedLanguages = supportedLanguages,
            ),
        )
            .joinBlocking()

        // Action success
        assertEquals(supportedLanguages, tabState().translationsState.supportedLanguages)
    }

    @Test
    fun `WHEN a SetNeverTranslateSitesAction is dispatched AND successful THEN update neverTranslateSites`() {
        // Initial
        assertEquals(null, tabState().translationsState.neverTranslateSites)

        // Action started
        val neverTranslateSites = listOf("google.com")
        store.dispatch(
            TranslationsAction.SetNeverTranslateSitesAction(
                tabId = tab.id,
                neverTranslateSites = neverTranslateSites,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(neverTranslateSites, tabState().translationsState.neverTranslateSites)
    }

    @Test
    fun `WHEN a RemoveNeverTranslateSiteAction is dispatched AND successful THEN update neverTranslateSites`() {
        // Initial add to neverTranslateSites
        assertEquals(null, tabState().translationsState.neverTranslateSites)
        val neverTranslateSites = listOf("google.com")
        store.dispatch(
            TranslationsAction.SetNeverTranslateSitesAction(
                tabId = tab.id,
                neverTranslateSites = neverTranslateSites,
            ),
        ).joinBlocking()
        assertEquals(neverTranslateSites, tabState().translationsState.neverTranslateSites)

        // Action started
        store.dispatch(
            TranslationsAction.RemoveNeverTranslateSiteAction(
                tabId = tab.id,
                origin = "google.com",
            ),
        ).joinBlocking()

        // Action success
        assertEquals(listOf<String>(), tabState().translationsState.neverTranslateSites)
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
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
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
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
            ),
        ).joinBlocking()
        assertEquals(null, tabState().translationsState.translationError)
        assertEquals(false, tabState().translationsState.isTranslated)
    }

    @Test
    fun `WHEN a SetPageSettingsAction is dispatched THEN set pageSettings`() {
        // Initial
        assertNull(tabState().translationsState.pageSettings)

        // Action started
        val pageSettings = TranslationPageSettings(
            alwaysOfferPopup = true,
            alwaysTranslateLanguage = true,
            neverTranslateLanguage = true,
            neverTranslateSite = true,
        )
        store.dispatch(
            TranslationsAction.SetPageSettingsAction(
                tabId = tab.id,
                pageSettings = pageSettings,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(pageSettings, tabState().translationsState.pageSettings)
    }

    @Test
    fun `WHEN a SetTranslationDownloadSize is dispatched THEN set translationSize is set`() {
        // Initial
        assertNull(tabState().translationsState.translationDownloadSize)

        // Action started
        val translationSize = TranslationDownloadSize(
            fromLanguage = Language("en", "English"),
            toLanguage = Language("fr", "French"),
            size = 10000L,
            error = null,
        )
        store.dispatch(
            TranslationsAction.SetTranslationDownloadSizeAction(
                tabId = tab.id,
                translationSize = translationSize,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(translationSize, tabState().translationsState.translationDownloadSize)
    }

    @Test
    fun `WHEN a FetchTranslationDownloadSize is dispatched THEN translationSize is cleared`() {
        // Initial setting size for a more robust test
        val translationSize = TranslationDownloadSize(
            fromLanguage = Language("en", "English"),
            toLanguage = Language("fr", "French"),
            size = 10000L,
            error = null,
        )
        store.dispatch(
            TranslationsAction.SetTranslationDownloadSizeAction(
                tabId = tab.id,
                translationSize = translationSize,
            ),
        ).joinBlocking()

        assertEquals(translationSize, tabState().translationsState.translationDownloadSize)

        // Action started
        store.dispatch(
            TranslationsAction.FetchTranslationDownloadSizeAction(
                tabId = tab.id,
                fromLanguage = Language("en", "English"),
                toLanguage = Language("fr", "French"),
            ),
        ).joinBlocking()

        // Action success
        assertNull(tabState().translationsState.translationDownloadSize)
    }

    @Test
    fun `WHEN a OperationRequestedAction is dispatched for FETCH_PAGE_SETTINGS THEN clear pageSettings`() {
        // Setting first to have a more robust initial state
        assertNull(tabState().translationsState.pageSettings)

        val pageSettings = TranslationPageSettings(
            alwaysOfferPopup = true,
            alwaysTranslateLanguage = true,
            neverTranslateLanguage = true,
            neverTranslateSite = true,
        )

        store.dispatch(
            TranslationsAction.SetPageSettingsAction(
                tabId = tab.id,
                pageSettings = pageSettings,
            ),
        ).joinBlocking()

        assertEquals(pageSettings, tabState().translationsState.pageSettings)
        assertNull(tabState().translationsState.settingsError)

        // Action started
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_PAGE_SETTINGS,
            ),
        ).joinBlocking()

        // Action success
        assertNull(tabState().translationsState.pageSettings)
    }

    @Test
    fun `WHEN a OperationRequestedAction is dispatched for FETCH_SUPPORTED_LANGUAGES THEN clear supportLanguages`() {
        // Setting first to have a more robust initial state
        assertNull(tabState().translationsState.supportedLanguages)

        val supportLanguages = TranslationSupport(
            fromLanguages = listOf(Language("en", "English")),
            toLanguages = listOf(Language("en", "English")),
        )

        store.dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                tabId = tab.id,
                supportedLanguages = supportLanguages,
            ),
        ).joinBlocking()

        assertEquals(supportLanguages, tabState().translationsState.supportedLanguages)

        // Action started
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
            ),
        ).joinBlocking()

        // Action success
        assertNull(tabState().translationsState.supportedLanguages)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for UPDATE_ALWAYS_OFFER_POPUP THEN set page settings for alwaysOfferPopup `() {
        // Action started
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_ALWAYS_OFFER_POPUP,
                setting = true,
            ),
        ).joinBlocking()

        // Action success
        assertTrue(tabState().translationsState.pageSettings?.alwaysOfferPopup!!)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for UPDATE_ALWAYS_TRANSLATE_LANGUAGE THEN set page settings for alwaysTranslateLanguage `() {
        // Action started
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_ALWAYS_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        // Action success
        assertTrue(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
        assertFalse(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for UPDATE_NEVER_TRANSLATE_LANGUAGE THEN set page settings for alwaysTranslateLanguage `() {
        // Action started
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        // Action success
        assertTrue(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)
        assertFalse(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for UPDATE_NEVER_TRANSLATE_SITE THEN set page settings for neverTranslateSite`() {
        // Action started
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_SITE,
                setting = true,
            ),
        ).joinBlocking()

        // Action success
        assertTrue(tabState().translationsState.pageSettings?.neverTranslateSite!!)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for each option THEN the page setting is consistent`() {
        // Action started
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_ALWAYS_OFFER_POPUP,
                setting = true,
            ),
        ).joinBlocking()

        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_ALWAYS_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_SITE,
                setting = true,
            ),
        ).joinBlocking()

        // Action success
        assertTrue(tabState().translationsState.pageSettings?.alwaysOfferPopup!!)
        // neverTranslateLanguage was posted last and will prevent a contradictory state on the alwaysTranslateLanguage state.
        assertFalse(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
        assertTrue(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)
        assertTrue(tabState().translationsState.pageSettings?.neverTranslateSite!!)
    }

    fun `WHEN a SetEngineSupportAction is dispatched THEN the browser store is updated to match`() {
        // Initial state
        assertNull(store.state.translationEngine.isEngineSupported)

        // Dispatch
        store.dispatch(
            TranslationsAction.SetEngineSupportedAction(
                isEngineSupported = true,
            ),
        ).joinBlocking()

        // Final state
        assertTrue(store.state.translationEngine.isEngineSupported!!)
    }

    @Test
    fun `WHEN an EngineExceptionAction is dispatched THEN the browser store is updated to match`() {
        // Initial state
        assertNull(store.state.translationEngine.engineError)

        // Dispatch
        val error = TranslationError.UnknownError(Throwable())
        store.dispatch(
            TranslationsAction.EngineExceptionAction(
                error = error,
            ),
        ).joinBlocking()

        // Final state
        assertEquals(store.state.translationEngine.engineError!!, error)
    }
}

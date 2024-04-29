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
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.LanguageSetting
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
import org.junit.Assert.assertNotNull
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

        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tab.id, isOfferTranslate = true))
            .joinBlocking()

        assertEquals(true, tabState().translationsState.isOfferTranslate)

        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tab.id, isOfferTranslate = false))
            .joinBlocking()

        assertFalse(tabState().translationsState.isOfferTranslate)
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
    fun `GIVEN isOfferTranslate is true WHEN a TranslateAction is dispatched THEN isOfferTranslate should be set to false`() {
        // Initial State
        assertFalse(tabState().translationsState.isOfferTranslate)

        // Initial Offer State
        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tab.id, true)).joinBlocking()
        assertTrue(tabState().translationsState.isOfferTranslate)

        // Action
        store.dispatch(TranslationsAction.TranslateAction(tabId = tab.id, fromLanguage = "en", toLanguage = "en", options = null)).joinBlocking()

        // Should revert to false
        assertFalse(tabState().translationsState.isOfferTranslate)
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
    fun `WHEN a TranslateStateChangeAction is dispatched THEN the isOfferTranslate status updates accordingly`() {
        // Initial State
        assertNull(tabState().translationsState.translationEngineState)

        // Sending an initial request to set state; however, the engine hasn't decided if it is an
        // offered state
        var translatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = true, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = "es", toLanguage = "en"),
        )
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertFalse(tabState().translationsState.isOfferTranslate)

        // Engine is sending a translation offer action
        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tab.id, isOfferTranslate = true))
            .joinBlocking()

        // Initial expected translation state
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertTrue(tabState().translationsState.isOfferTranslate)

        // Not in an offer translation state, because it is no longer supported
        translatedEngineState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = false, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = TranslationPair(fromLanguage = "es", toLanguage = "en"),
        )

        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = translatedEngineState))
            .joinBlocking()
        assertFalse(tabState().translationsState.isOfferTranslate)
    }

    @Test
    fun `WHEN a TranslateStateChangeAction is dispatched THEN the translationError status updates accordingly`() {
        // Initial State
        assertNull(tabState().translationsState.translationEngineState)
        assertNull(tabState().translationsState.translationError)

        // Sending an initial request to set state, notice the supportedDocumentLang isn't supported
        val noSupportedState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "unknown", supportedDocumentLang = false, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = null,
        )
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = noSupportedState))
            .joinBlocking()

        // Response state
        assertEquals(noSupportedState, tabState().translationsState.translationEngineState)
        assertNotNull(tabState().translationsState.translationError)

        // Sending a request to show state change, notice the supportedDocumentLang is now supported
        val supportedState = TranslationEngineState(
            detectedLanguages = DetectedLanguages(documentLangTag = "es", supportedDocumentLang = true, userPreferredLangTag = "en"),
            error = null,
            isEngineReady = true,
            requestedTranslationPair = null,
        )
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId = tab.id, translationEngineState = supportedState))
            .joinBlocking()

        // Response state
        assertEquals(supportedState, tabState().translationsState.translationEngineState)
        assertNull(tabState().translationsState.translationError)
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
        assertNull(store.state.translationEngine.supportedLanguages)

        // Action started
        val toLanguage = Language("de", "German")
        val fromLanguage = Language("es", "Spanish")
        val supportedLanguages = TranslationSupport(listOf(fromLanguage), listOf(toLanguage))
        store.dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                supportedLanguages = supportedLanguages,
            ),
        )
            .joinBlocking()

        // Action success
        assertEquals(supportedLanguages, store.state.translationEngine.supportedLanguages)
    }

    @Test
    fun `WHEN a SetNeverTranslateSitesAction is dispatched AND successful THEN update neverTranslateSites`() {
        // Initial
        assertNull(store.state.translationEngine.neverTranslateSites)

        // Action started
        val neverTranslateSites = listOf("google.com")
        store.dispatch(
            TranslationsAction.SetNeverTranslateSitesAction(
                neverTranslateSites = neverTranslateSites,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(neverTranslateSites, store.state.translationEngine.neverTranslateSites)
    }

    @Test
    fun `WHEN a RemoveNeverTranslateSiteAction is dispatched AND successful THEN update neverTranslateSites`() {
        // Initial add to neverTranslateSites
        assertNull(store.state.translationEngine.neverTranslateSites)
        val neverTranslateSites = listOf("google.com")
        store.dispatch(
            TranslationsAction.SetNeverTranslateSitesAction(
                neverTranslateSites = neverTranslateSites,
            ),
        ).joinBlocking()
        assertEquals(neverTranslateSites, store.state.translationEngine.neverTranslateSites)

        // Action started
        store.dispatch(
            TranslationsAction.RemoveNeverTranslateSiteAction(
                origin = "google.com",
            ),
        ).joinBlocking()

        // Action success
        assertEquals(listOf<String>(), store.state.translationEngine.neverTranslateSites)
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
        val fetchLanguagesError = TranslationError.CouldNotLoadLanguagesError(null)

        // Testing setting tab level error
        store.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                translationError = fetchLanguagesError,
            ),
        ).joinBlocking()
        assertEquals(fetchLanguagesError, tabState().translationsState.translationError)

        // Testing setting browser level error
        store.dispatch(
            TranslationsAction.EngineExceptionAction(
                error = fetchLanguagesError,
            ),
        ).joinBlocking()
        assertEquals(fetchLanguagesError, store.state.translationEngine.engineError)
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
        assertNull(store.state.translationEngine.supportedLanguages)

        val supportLanguages = TranslationSupport(
            fromLanguages = listOf(Language("en", "English")),
            toLanguages = listOf(Language("en", "English")),
        )

        store.dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                supportedLanguages = supportLanguages,
            ),
        ).joinBlocking()

        assertEquals(supportLanguages, store.state.translationEngine.supportedLanguages)

        // Action started
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
            ),
        ).joinBlocking()

        // Action success
        assertNull(store.state.translationEngine.supportedLanguages)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for UPDATE_ALWAYS_OFFER_POPUP THEN set page settings for alwaysOfferPopup `() {
        // Initial State
        assertNull(tabState().translationsState.pageSettings?.alwaysOfferPopup)

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
        // Initial State
        assertNull(tabState().translationsState.pageSettings?.alwaysTranslateLanguage)
        assertNull(tabState().translationsState.pageSettings?.neverTranslateLanguage)

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
        // Initial State
        assertNull(tabState().translationsState.pageSettings?.neverTranslateLanguage)
        assertNull(tabState().translationsState.pageSettings?.alwaysTranslateLanguage)

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
        // Initial State
        assertNull(tabState().translationsState.pageSettings?.neverTranslateLanguage)

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
    fun `WHEN an UpdatePageSettingAction is dispatched for UPDATE_ALWAYS_TRANSLATE_LANGUAGE AND UPDATE_ALWAYS_TRANSLATE_LANGUAGE THEN must be opposites of each other or both must be false `() {
        // Initial state
        assertNull(tabState().translationsState.pageSettings?.alwaysTranslateLanguage)
        assertNull(tabState().translationsState.pageSettings?.neverTranslateLanguage)

        // Action started to update the always offer setting to true
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_ALWAYS_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        // When always is true, never should be false
        assertTrue(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
        assertFalse(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)

        // Action started to update the never offer setting to true
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE,
                setting = true,
            ),
        ).joinBlocking()

        // When never is true, always should be false
        assertFalse(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
        assertTrue(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)

        // Action started to update the never language setting to false
        store.dispatch(
            TranslationsAction.UpdatePageSettingAction(
                tabId = tab.id,
                operation = TranslationPageSettingOperation.UPDATE_NEVER_TRANSLATE_LANGUAGE,
                setting = false,
            ),
        ).joinBlocking()

        // When never is false, always may also be false
        assertFalse(tabState().translationsState.pageSettings?.alwaysTranslateLanguage!!)
        assertFalse(tabState().translationsState.pageSettings?.neverTranslateLanguage!!)
    }

    @Test
    fun `WHEN a UpdatePageSettingAction is dispatched for each option THEN the page setting is consistent`() {
        // Initial State
        assertNull(tabState().translationsState.pageSettings?.alwaysOfferPopup)
        assertNull(tabState().translationsState.pageSettings?.alwaysTranslateLanguage)
        assertNull(tabState().translationsState.pageSettings?.neverTranslateLanguage)
        assertNull(tabState().translationsState.pageSettings?.neverTranslateSite)

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

    @Test
    fun `WHEN a SetLanguageSettingsAction is dispatched THEN the browser store is updated to match`() {
        // Initial state
        assertNull(store.state.translationEngine.languageSettings)

        // Dispatch
        val languageSetting = mapOf("es" to LanguageSetting.OFFER)
        store.dispatch(
            TranslationsAction.SetLanguageSettingsAction(
                languageSettings = languageSetting,
            ),
        ).joinBlocking()

        // Final state
        assertEquals(store.state.translationEngine.languageSettings!!, languageSetting)
    }

    @Test
    fun `WHEN a OperationRequestedAction is dispatched for FETCH_AUTOMATIC_LANGUAGE_SETTINGS THEN clear languageSettings`() {
        // Setting first to have a more robust initial state
        val languageSetting = mapOf("es" to LanguageSetting.OFFER)
        store.dispatch(
            TranslationsAction.SetLanguageSettingsAction(
                languageSettings = languageSetting,
            ),
        ).joinBlocking()
        assertEquals(store.state.translationEngine.languageSettings, languageSetting)

        // Action started
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_AUTOMATIC_LANGUAGE_SETTINGS,
            ),
        ).joinBlocking()

        // Action success
        assertNull(store.state.translationEngine.languageSettings)
    }

    @Test
    fun `WHEN a TranslateExceptionAction is dispatched for FETCH_AUTOMATIC_LANGUAGE_SETTINGS THEN set the error`() {
        // Action started
        val error = TranslationError.UnknownError(IllegalStateException())
        store.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_AUTOMATIC_LANGUAGE_SETTINGS,
                translationError = error,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(error, tabState().translationsState.translationError)
    }

    @Test
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

    @Test
    fun `WHEN a SetLanguageModelsAction is dispatched and successful THEN the browser store is updated to match`() {
        // Initial state
        assertNull(store.state.translationEngine.languageModels)

        val code = "es"
        val localizedDisplayName = "Spanish"
        val isDownloaded = true
        val size: Long = 1234
        val language = Language(code, localizedDisplayName)
        val languageModel = LanguageModel(language, isDownloaded, size)
        val languageModels = mutableListOf(languageModel)

        // Dispatch
        store.dispatch(
            TranslationsAction.SetLanguageModelsAction(
                languageModels = languageModels,
            ),
        ).joinBlocking()

        // Final state
        assertEquals(languageModels, store.state.translationEngine.languageModels)
    }

    @Test
    fun `WHEN SetOfferTranslateSettingAction is called then set offerToTranslate`() {
        // Initial State
        assertNull(store.state.translationEngine.offerTranslation)

        // Action started
        store.dispatch(
            TranslationsAction.SetGlobalOfferTranslateSettingAction(
                offerTranslation = false,
            ),
        ).joinBlocking()

        // Action success
        assertFalse(store.state.translationEngine.offerTranslation!!)
    }

    @Test
    fun `WHEN UpdateOfferTranslateSettingAction is called then set offerToTranslate`() {
        // Initial State
        assertNull(store.state.translationEngine.offerTranslation)

        // Action started
        store.dispatch(
            TranslationsAction.UpdateGlobalOfferTranslateSettingAction(
                offerTranslation = false,
            ),
        ).joinBlocking()

        // Action success
        assertFalse(store.state.translationEngine.offerTranslation!!)
    }

    @Test
    fun `WHEN UpdateGlobalLanguageSettingAction is called then update languageSettings`() {
        // Initial State
        assertNull(store.state.translationEngine.languageSettings)

        // No-op null test
        store.dispatch(
            TranslationsAction.UpdateLanguageSettingsAction(
                languageCode = "fr",
                setting = LanguageSetting.ALWAYS,
            ),
        ).joinBlocking()

        assertNull(store.state.translationEngine.languageSettings)

        // Setting Initial State
        val languageSettings = mapOf<String, LanguageSetting>(
            "en" to LanguageSetting.OFFER,
            "es" to LanguageSetting.NEVER,
            "de" to LanguageSetting.ALWAYS,
        )

        store.dispatch(
            TranslationsAction.SetLanguageSettingsAction(
                languageSettings = languageSettings,
            ),
        ).joinBlocking()

        assertEquals(languageSettings, store.state.translationEngine.languageSettings)

        // No-op update test
        store.dispatch(
            TranslationsAction.UpdateLanguageSettingsAction(
                languageCode = "fr",
                setting = LanguageSetting.ALWAYS,
            ),
        ).joinBlocking()

        assertEquals(languageSettings, store.state.translationEngine.languageSettings)

        // Main action started
        store.dispatch(
            TranslationsAction.UpdateLanguageSettingsAction(
                languageCode = "es",
                setting = LanguageSetting.ALWAYS,
            ),
        ).joinBlocking()

        // Action success
        assertEquals(LanguageSetting.ALWAYS, store.state.translationEngine.languageSettings!!["es"])
    }
}

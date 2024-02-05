/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.TranslationsState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class TranslationsMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope
    private val engine: Engine = mock()
    private val engineSession: EngineSession = mock()
    private val tab: TabSessionState = spy(
        createTab(
            url = "https://www.firefox.com",
            title = "Firefox",
            id = "1",
            engineSession = engineSession,
        ),
    )
    private val translationsMiddleware = TranslationsMiddleware(engine = engine, scope = scope)
    private val tabs = spy(listOf(tab))
    private val state = spy(BrowserState(tabs = tabs))
    private val store = spy(BrowserStore(middleware = listOf(translationsMiddleware), initialState = state))
    private val context = mock<MiddlewareContext<BrowserState, BrowserAction>>()

    @Before
    fun setup() {
        whenever(context.store).thenReturn(store)
        whenever(context.state).thenReturn(state)
    }

    private fun waitForIdle() {
        scope.testScheduler.runCurrent()
        scope.testScheduler.advanceUntilIdle()
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
    }

    @Test
    fun `WHEN OperationRequestedAction is dispatched to fetch languages AND succeeds THEN SetSupportedLanguagesAction is dispatched`() = runTest {
        val supportedLanguages = TranslationSupport(
            fromLanguages = listOf(Language("en", "English")),
            toLanguages = listOf(Language("en", "English")),
        )
        val languageCallback = argumentCaptor<((TranslationSupport) -> Unit)>()

        val action =
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
            )

        translationsMiddleware.invoke(context, {}, action)
        verify(engine).getSupportedTranslationLanguages(onSuccess = languageCallback.capture(), onError = any())
        languageCallback.value.invoke(supportedLanguages)

        waitForIdle()

        verify(context.store).dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                tabId = tab.id,
                supportedLanguages = supportedLanguages,
            ),
        )

        waitForIdle()
    }

    @Test
    fun `WHEN OperationRequestedAction is dispatched with FETCH_SUPPORTED_LANGUAGES AND fails THEN TranslateExceptionAction is dispatched`() = runTest {
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
            ),
        ).joinBlocking()

        waitForIdle()

        verify(store).dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_SUPPORTED_LANGUAGES,
                translationError = TranslationError.CouldNotLoadLanguagesError(any()),
            ),
        )

        waitForIdle()
    }

    @Test
    fun `WHEN OperationRequestedAction is dispatched WITH FETCH_PAGE_SETTINGS AND fetching languages is successful THEN TranslationPageSettings is dispatched`() = runTest {
        // Mock detected languages are required to pull the language settings
        val mockFrom = "es"
        val mockTo = "en"
        val mockDetectedLanguages = DetectedLanguages(
            documentLangTag = mockFrom,
            supportedDocumentLang = true,
            userPreferredLangTag = mockTo,
        )
        val mockState = TranslationsState(
            translationEngineState = TranslationEngineState(mockDetectedLanguages),
        )
        // Had mock issues when mocking anything more granular
        whenever(store.state.findTab(tab.id)?.translationsState).thenReturn(mockState)

        // Always offer behavior
        val offerResponse = true
        whenever(engine.getTranslationsOfferPopup()).thenAnswer { offerResponse }

        // Page language
        val languageSettingCallback = argumentCaptor<((LanguageSetting) -> Unit)>()
        val languageResponse = LanguageSetting.ALWAYS
        whenever(
            engine.getLanguageSetting(
                languageCode = any(),
                onSuccess = languageSettingCallback.capture(),
                onError = any(),
            ),
        )
            .thenAnswer {
                languageSettingCallback.value.invoke(languageResponse)
            }

        // Never translate site behavior
        val neverTranslateSiteCallback = argumentCaptor<((Boolean) -> Unit)>()
        val neverSiteResponse = true
        whenever(
            engineSession.getNeverTranslateSiteSetting(
                onResult = neverTranslateSiteCallback.capture(),
                onException = any(),
            ),
        )
            .thenAnswer {
                neverTranslateSiteCallback.value.invoke(neverSiteResponse)
            }

        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_PAGE_SETTINGS,
            ),
        ).joinBlocking()

        waitForIdle()

        verify(store).dispatch(
            TranslationsAction.SetPageSettingsAction(
                tabId = tab.id,
                pageSettings = any(),
            ),
        )
    }

    @Test
    fun `WHEN OperationRequestedAction AND fetching data fails THEN TranslateExceptionAction is dispatched`() {
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_PAGE_SETTINGS,
            ),
        ).joinBlocking()
        waitForIdle()

        verify(store).dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_PAGE_SETTINGS,
                translationError = TranslationError.CouldNotLoadPageSettingsError(any()),
            ),
        )

        waitForIdle()
    }

    @Test
    fun `WHEN OperationRequestedAction is dispatched to fetch never translate sites AND succeeds THEN SetNeverTranslateSitesAction is dispatched`() = runTest {
        val neverTranslateSites = listOf("google.com")
        val sitesCallback = argumentCaptor<((List<String>) -> Unit)>()
        val action =
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_NEVER_TRANSLATE_SITES,
            )
        translationsMiddleware.invoke(context, {}, action)
        verify(engine).getNeverTranslateSiteList(onSuccess = sitesCallback.capture(), onError = any())
        sitesCallback.value.invoke(neverTranslateSites)

        verify(context.store).dispatch(
            TranslationsAction.SetNeverTranslateSitesAction(
                tabId = tab.id,
                neverTranslateSites = neverTranslateSites,
            ),
        )

        waitForIdle()
    }

    @Test
    fun `WHEN OperationRequestedAction is dispatched to fetch never translate sites AND fails THEN TranslateExceptionAction is dispatched`() = runTest {
        store.dispatch(
            TranslationsAction.OperationRequestedAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_NEVER_TRANSLATE_SITES,
            ),
        ).joinBlocking()
        waitForIdle()

        verify(store).dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_NEVER_TRANSLATE_SITES,
                translationError = TranslationError.CouldNotLoadNeverTranslateSites(any()),
            ),
        )
        waitForIdle()
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.launch
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

    private fun waitForIdle() {
        scope.testScheduler.runCurrent()
        scope.testScheduler.advanceUntilIdle()
        coroutinesTestRule.testDispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
    }

    @Test
    fun `WHEN TranslateExpectedAction is dispatched AND fetching languages is successful THEN TranslateSetLanguagesAction is dispatched`() {
        val supportedLanguages = TranslationSupport(null, null)
        val callbackCaptor = argumentCaptor<((TranslationSupport) -> Unit)>()
        val action = TranslationsAction.TranslateExpectedAction(tabId = tab.id)
        val middlewareContext = mock<MiddlewareContext<BrowserState, BrowserAction>>()

        // Important for ensuring that the tests verify on the same store
        whenever(middlewareContext.store).thenReturn(store)
        whenever(engine.getSupportedTranslationLanguages(callbackCaptor.capture(), any()))
            .thenAnswer { callbackCaptor.value(supportedLanguages) }

        translationsMiddleware.invoke(middlewareContext, {}, action)

        waitForIdle()

        scope.launch {
            verify(store).dispatch(
                TranslationsAction.TranslateSetLanguagesAction(
                    tabId = tab.id,
                    supportedLanguages = supportedLanguages,
                ),
            )
        }

        waitForIdle()
    }

    @Test
    fun `WHEN TranslateExpectedAction is dispatched AND fetching languages fails THEN TranslateExceptionAction is dispatched`() {
        store.dispatch(TranslationsAction.TranslateExpectedAction(tabId = tab.id)).joinBlocking()

        waitForIdle()

        verify(store).dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId = tab.id,
                operation = TranslationOperation.FETCH_LANGUAGES,
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
}

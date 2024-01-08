/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
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
    private lateinit var engine: Engine
    private lateinit var engineSession: EngineSession
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore
    private lateinit var translationsMiddleware: TranslationsMiddleware

    @Before
    fun setUp() {
        engine = mock()
        engineSession = mock()
        tab = createTab("https://www.mozilla.org", id = "test-tab", engineSession = engineSession)
        translationsMiddleware = TranslationsMiddleware(engine = engine, scope = scope)
        store = spy(
            BrowserStore(
                middleware = listOf(translationsMiddleware),
                initialState = BrowserState(
                    tabs = listOf(tab),
                ),
            ),
        )
    }

    private fun waitForIdle() {
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
}

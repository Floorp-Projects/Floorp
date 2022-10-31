/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import android.content.ComponentCallbacks2
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class TrimMemoryMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    private lateinit var engineSessionReddit: EngineSession
    private lateinit var engineSessionTheVerge: EngineSession
    private lateinit var engineSessionTwitch: EngineSession
    private lateinit var engineSessionGoogleNews: EngineSession
    private lateinit var engineSessionAmazon: EngineSession
    private lateinit var engineSessionYouTube: EngineSession
    private lateinit var engineSessionFacebook: EngineSession

    private lateinit var store: BrowserStore

    private lateinit var engineSessionStateReddit: EngineSessionState
    private lateinit var engineSessionStateTheVerge: EngineSessionState
    private lateinit var engineSessionStateTwitch: EngineSessionState

    @Before
    fun setUp() {
        engineSessionTheVerge = mock()
        engineSessionReddit = mock()
        engineSessionTwitch = mock()
        engineSessionGoogleNews = mock()
        engineSessionAmazon = mock()
        engineSessionYouTube = mock()
        engineSessionFacebook = mock()

        engineSessionStateReddit = mock()
        engineSessionStateTheVerge = mock()
        engineSessionStateTwitch = mock()

        store = BrowserStore(
            middleware = listOf(
                TrimMemoryMiddleware(),
                SuspendMiddleware(scope),
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla").copy(
                        lastAccess = 5,
                    ),
                    createTab("https://www.theverge.com/", id = "theverge").copy(
                        engineState = EngineState(
                            engineSession = engineSessionTheVerge,
                            engineSessionState = engineSessionStateTheVerge,
                            engineObserver = mock(),
                        ),
                        lastAccess = 2,
                    ),
                    createTab(
                        "https://www.reddit.com/r/firefox/",
                        id = "reddit",
                        private = true,
                    ).copy(
                        engineState = EngineState(
                            engineSession = engineSessionReddit,
                            engineSessionState = engineSessionStateReddit,
                            engineObserver = mock(),
                        ),
                        lastAccess = 20,
                    ),
                    createTab("https://github.com/", id = "github").copy(
                        lastAccess = 12,
                    ),
                    createTab("https://news.google.com", id = "google-news").copy(
                        engineState = EngineState(engineSessionGoogleNews, engineObserver = mock()),
                        lastAccess = 10,
                    ),
                    createTab("https://www.amazon.com", id = "amazon").copy(
                        engineState = EngineState(engineSessionAmazon, engineObserver = mock()),
                        lastAccess = 4,
                    ),
                    createTab("https://www.youtube.com", id = "youtube").copy(
                        engineState = EngineState(engineSessionYouTube, engineObserver = mock()),
                        lastAccess = 4,
                    ),
                    createTab("https://www.facebook.com", id = "facebook").copy(
                        engineState = EngineState(engineSessionFacebook, engineObserver = mock()),
                        lastAccess = 7,
                    ),
                ),
                customTabs = listOf(
                    createCustomTab("https://www.twitch.tv/", id = "twitch").copy(
                        engineState = EngineState(
                            engineSession = engineSessionTwitch,
                            engineSessionState = engineSessionStateTwitch,
                            engineObserver = mock(),
                        ),
                    ),
                    createCustomTab("https://twitter.com/home", id = "twitter"),
                ),
                selectedTabId = "reddit",
            ),
        )
    }

    @Test
    fun `TrimMemoryMiddleware - TRIM_MEMORY_UI_HIDDEN`() {
        store.dispatch(
            SystemAction.LowMemoryAction(
                level = ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN,
            ),
        ).joinBlocking()

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        store.state.findTab("theverge")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findTab("reddit")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findTab("google-news")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNull(engineSessionState)
        }

        store.state.findTab("amazon")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNull(engineSessionState)
        }

        store.state.findTab("youtube")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNull(engineSessionState)
        }

        store.state.findTab("facebook")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNull(engineSessionState)
        }

        store.state.findCustomTab("twitch")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findCustomTab("twitter")!!.engineState.apply {
            assertNull(engineSession)
            assertNull(engineObserver)
            assertNull(engineSessionState)
        }

        verify(engineSessionTheVerge, never()).close()
        verify(engineSessionReddit, never()).close()
        verify(engineSessionTwitch, never()).close()
    }

    @Test
    fun `TrimMemoryMiddleware - TRIM_MEMORY_RUNNING_CRITICAL`() {
        store.dispatch(
            SystemAction.LowMemoryAction(
                level = ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL,
            ),
        ).joinBlocking()

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        store.state.findTab("theverge")!!.engineState.apply {
            assertNull(engineSession)
            assertNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findTab("reddit")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findTab("google-news")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
        }

        store.state.findTab("facebook")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
        }

        store.state.findTab("amazon")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
        }

        store.state.findTab("youtube")!!.engineState.apply {
            assertNull(engineSession)
            assertNull(engineObserver)
        }

        store.state.findCustomTab("twitch")!!.engineState.apply {
            assertNotNull(engineSession)
            assertNotNull(engineObserver)
            assertNotNull(engineSessionState)
        }

        store.state.findCustomTab("twitter")!!.engineState.apply {
            assertNull(engineSession)
            assertNull(engineObserver)
            assertNull(engineSessionState)
        }

        verify(engineSessionTheVerge).close()
        verify(engineSessionYouTube).close()

        verify(engineSessionReddit, never()).close()
        verify(engineSessionTwitch, never()).close()
        verify(engineSessionGoogleNews, never()).close()
        verify(engineSessionFacebook, never()).close()
        verify(engineSessionAmazon, never()).close()
    }
}

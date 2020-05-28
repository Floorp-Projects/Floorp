/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.service

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.createMockMediaElement
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MediaServiceDelegateTest {
    private val dispatcher: TestCoroutineDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(dispatcher)

    @Test
    fun `PLAYING state starts service in foreground`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(createMockMediaElement(id = "test-media"))
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(service).startForeground(anyInt(), any())

        delegate.onDestroy()
    }

    @Test
    fun `Switching from PLAYING to NONE stops service`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(createMockMediaElement(id = "test-media"))
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(service, never()).stopSelf()

        store.dispatch(MediaAction.UpdateMediaAggregateAction(
            MediaState.Aggregate(MediaState.State.NONE)
        )).joinBlocking()

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        verify(service).stopSelf()

        delegate.onDestroy()
    }

    @Test
    @Ignore("Since we started using FLAG_UPDATE_CURRENT on the PendingIntent Robolectric started failing with a NullPointerException")
    // https://github.com/robolectric/robolectric/issues/5673
    fun `Switching from playing to pause stops serving from being in the foreground`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(createMockMediaElement(id = "test-media"))
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(service).startForeground(anyInt(), any())
        verify(service, never()).stopForeground(false)

        store.dispatch(MediaAction.UpdateMediaAggregateAction(
            MediaState.Aggregate(MediaState.State.PAUSED, "test-tab", listOf("test-media"))
        )).joinBlocking()

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        verify(service).stopForeground(false)

        delegate.onDestroy()
    }

    @Test
    fun `Task getting removed stops service`() {
        val media = createMockMediaElement(id = "test-media")

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(media)
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(service).startForeground(anyInt(), any())
        verify(service, never()).stopSelf()
        verify(media.controller, never()).pause()

        delegate.onTaskRemoved()

        verify(media.controller).pause()
        verify(service).stopSelf()

        delegate.onDestroy()
    }

    @Test
    fun `Task getting removed does not stop service if media state is for custom tab`() {
        val media = createMockMediaElement(id = "test-media")

        val store = BrowserStore(BrowserState(
            tabs = emptyList(),
            customTabs = listOf(
                createCustomTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(media)
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(service).startForeground(anyInt(), any())
        verify(service, never()).stopSelf()
        verify(media.controller, never()).pause()

        delegate.onTaskRemoved()

        verify(service, never()).stopSelf()
        verify(media.controller, never()).pause()

        delegate.onDestroy()
    }

    @Test
    fun `PAUSE intent will pause playing media`() {
        val media = createMockMediaElement(id = "test-media")

        val store = BrowserStore(BrowserState(
            tabs = emptyList(),
            customTabs = listOf(
                createCustomTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PLAYING,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(media)
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(media.controller, never()).pause()

        delegate.onStartCommand(AbstractMediaService.pauseIntent(
            testContext,
            service::class.java
        ))

        verify(media.controller).pause()
    }

    @Test
    fun `PLAY intent will play paused media`() {
        val media = createMockMediaElement(id = "test-media", state = Media.State.PAUSED)

        val store = BrowserStore(BrowserState(
            tabs = emptyList(),
            customTabs = listOf(
                createCustomTab("https://www.mozilla.org", id = "test-tab")
            ),
            media = MediaState(
                aggregate = MediaState.Aggregate(
                    state = MediaState.State.PAUSED,
                    activeTabId = "test-tab",
                    activeMedia = listOf("test-media")
                ),
                elements = mapOf(
                    "test-tab" to listOf(media)
                )

            )
        ))

        val service: AbstractMediaService = mock()
        val delegate = MediaServiceDelegate(testContext, service, store)

        delegate.onCreate()

        verify(media.controller, never()).play()

        delegate.onStartCommand(AbstractMediaService.playIntent(
            testContext,
            service::class.java
        ))

        verify(media.controller).play()
    }
}

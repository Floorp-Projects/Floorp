/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware.sideeffects

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.createMockMediaElement
import mozilla.components.lib.state.MiddlewareStore
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class MediaAggregateUpdaterTest {
    @Test
    fun `WHEN NONE state THEN empty state will not aggregate new state`() {
        val store = MockStore(BrowserState())
        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)
        assertNull(result)
    }

    @Test
    fun `WHEN has playing media THEN aggregate will have PLAYING state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id",
                                state = Media.State.PLAYING
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNotNull(result!!)

        assertEquals("test-tab", result.activeTabId)
        assertEquals(1, result.activeMedia.size)
        assertEquals("media-id", result.activeMedia[0])
        assertEquals(MediaState.State.PLAYING, result.state)
        assertEquals(null, result.activeFullscreenOrientation)
    }

    @Test
    fun `WHEN has media in fullscreen THEN aggregate will have fullscreen state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id",
                                metadata = Media.Metadata(height = 1L, width = 2L),
                                state = Media.State.PLAYING,
                                fullscreenInfo = true
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNotNull(result!!)

        assertEquals("test-tab", result.activeTabId)
        assertEquals(1, result.activeMedia.size)
        assertEquals("media-id", result.activeMedia[0])
        assertEquals(MediaState.State.PLAYING, result.state)
        assertEquals(
            MediaState.FullscreenOrientation.LANDSCAPE,
            result.activeFullscreenOrientation
        )
    }

    @Test
    fun `WHEN is PLAYING state and media is paused THEN aggregate will have PAUSED state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.PLAYING,
                        activeTabId = "test-tab",
                        activeMedia = listOf("media-id")
                    ),
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id",
                                state = Media.State.PAUSED
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNotNull(result!!)

        assertEquals("test-tab", result.activeTabId)
        assertEquals(1, result.activeMedia.size)
        assertEquals("media-id", result.activeMedia[0])
        assertEquals(MediaState.State.PAUSED, result.state)
        assertEquals(null, result.activeFullscreenOrientation)
    }

    @Test
    fun `WHEN is PLAYING state and media changes but still plays THEN aggregate will have PLAYING state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.PLAYING,
                        activeTabId = "test-tab",
                        activeMedia = listOf("media-id")
                    ),
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "new-media-id",
                                state = Media.State.PLAYING
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNotNull(result!!)

        assertEquals("test-tab", result.activeTabId)
        assertEquals(1, result.activeMedia.size)
        assertEquals("new-media-id", result.activeMedia[0])
        assertEquals(MediaState.State.PLAYING, result.state)
        assertEquals(null, result.activeFullscreenOrientation)
    }

    @Test
    fun `WHEN media has short length THEN does not switch to PLAYING state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id-1",
                                state = Media.State.PLAYING,
                                metadata = Media.Metadata(duration = 2.0)
                            ),
                            createMockMediaElement(
                                id = "media-id-2",
                                state = Media.State.PLAYING,
                                metadata = Media.Metadata(duration = 1.0)
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNull(result)
    }

    @Test
    fun `WHEN media is muted THEN does not switch to PLAYING state`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id-1",
                                state = Media.State.PLAYING,
                                volume = Media.Volume(muted = true)
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNull(result)
    }

    @Test
    fun `WHEN state is PAUSED and media is paused THEN state remains PAUSED`() {
        val store = MockStore(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.PAUSED,
                        activeTabId = "test-tab",
                        activeMedia = listOf("media-id", "media-id2")
                    ),
                    elements = mapOf(
                        "test-tab" to listOf(
                            createMockMediaElement(
                                id = "media-id",
                                state = Media.State.PAUSED
                            )
                        )
                    )
                )
            )
        )

        val aggregator = MediaAggregateUpdater()

        val result = aggregate(store, aggregator)

        assertNotNull(result!!)

        assertEquals("test-tab", result.activeTabId)
        assertEquals(1, result.activeMedia.size)
        assertEquals("media-id", result.activeMedia[0])
        assertEquals(MediaState.State.PAUSED, result.state)
        assertEquals(null, result.activeFullscreenOrientation)
    }

    private fun aggregate(
        store: MockStore,
        aggregator: MediaAggregateUpdater
    ): MediaState.Aggregate? {
        aggregator.process(store)

        runBlocking {
            aggregator.updateAggregateJob?.join() ?: return@runBlocking null
        }

        return (store.lastAction as? MediaAction.UpdateMediaAggregateAction)?.aggregate
    }
}

private class MockStore(
    initialState: BrowserState
) : MiddlewareStore<BrowserState, BrowserAction> {
    var lastAction: BrowserAction? = null
        private set

    override var state: BrowserState = initialState

    override fun dispatch(action: BrowserAction) {
        lastAction = action
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.createMockMediaElement
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class MediaStateKtTest {
    @Test
    fun `pauseIfPlaying() - NONE state`() {
        val media = createMockMediaElement()

        val noneState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.NONE
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    media
                )
            )
        )

        noneState.pauseIfPlaying()

        verify(media.controller, never()).pause()
    }

    @Test
    fun `pauseIfPlaying() - PLAYING state`() {
        val playingMedia = createMockMediaElement(id = "playing-id", state = Media.State.PLAYING)
        val pausedMedia = createMockMediaElement(id = "paused-id", state = Media.State.PAUSED)

        val playingState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PLAYING,
                activeTabId = "tab-id",
                activeMedia = listOf("playing-id")
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    playingMedia,
                    pausedMedia
                )
            )
        )

        playingState.pauseIfPlaying()

        verify(playingMedia.controller).pause()
        verify(pausedMedia.controller, never()).pause()
    }

    @Test
    fun `pauseIfPlaying() - PAUSED state`() {
        val playingMedia = createMockMediaElement(id = "playing-id", state = Media.State.PLAYING)
        val pausedMedia = createMockMediaElement(id = "paused-id", state = Media.State.PAUSED)

        val playingState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PAUSED,
                activeTabId = "tab-id",
                activeMedia = listOf("paused-id")
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    playingMedia,
                    pausedMedia
                )
            )
        )

        playingState.pauseIfPlaying()

        verify(playingMedia.controller, never()).pause()
        verify(pausedMedia.controller, never()).pause()
    }

    @Test
    fun `playIfPaused() - NONE state`() {
        val media = createMockMediaElement(state = Media.State.PAUSED)

        val noneState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.NONE
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    media
                )
            )
        )

        noneState.playIfPaused()

        verify(media.controller, never()).play()
    }

    @Test
    fun `playIfPaused() - PLAYING state`() {
        val playingMedia = createMockMediaElement(id = "playing-id", state = Media.State.PLAYING)
        val pausedMedia = createMockMediaElement(id = "paused-id", state = Media.State.PAUSED)

        val playingState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PLAYING,
                activeTabId = "tab-id",
                activeMedia = listOf("playing-id")
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    playingMedia,
                    pausedMedia
                )
            )
        )

        playingState.playIfPaused()

        verify(playingMedia.controller, never()).play()
        verify(pausedMedia.controller, never()).play()
    }

    @Test
    fun `playIfPaused() - PAUSED state`() {
        val playingMedia = createMockMediaElement(id = "playing-id", state = Media.State.PLAYING)
        val pausedMedia = createMockMediaElement(id = "paused-id", state = Media.State.PAUSED)

        val playingState = MediaState(
            aggregate = MediaState.Aggregate(
                state = MediaState.State.PAUSED,
                activeTabId = "tab-id",
                activeMedia = listOf("paused-id")
            ),
            elements = mapOf(
                "tab-id" to listOf(
                    playingMedia,
                    pausedMedia
                )
            )
        )

        playingState.playIfPaused()

        verify(playingMedia.controller, never()).play()
        verify(pausedMedia.controller).play()
    }

    @Test
    fun `isMediaStateForCustomTab() extension method`() {
        assertFalse(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
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
            ).isMediaStateForCustomTab()
        )

        assertTrue(
            BrowserState(
                customTabs = listOf(createCustomTab("https://www.mozilla.org", id = "test-tab")),
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
            ).isMediaStateForCustomTab()
        )

        assertFalse(
            BrowserState(
                tabs = listOf(createTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.PAUSED,
                        activeTabId = "test-tab",
                        activeMedia = listOf("test-media")
                    ),
                    elements = mapOf(
                        "test-tab" to listOf(createMockMediaElement(id = "test-media", state = Media.State.PAUSED))
                    )
                )
            ).isMediaStateForCustomTab()
        )

        assertTrue(
            BrowserState(
                customTabs = listOf(createCustomTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.PAUSED,
                        activeTabId = "test-tab",
                        activeMedia = listOf("test-media")
                    ),
                    elements = mapOf(
                        "test-tab" to listOf(createMockMediaElement(id = "test-media", state = Media.State.PAUSED))
                    )
                )
            ).isMediaStateForCustomTab()
        )

        assertFalse(
            BrowserState(
                customTabs = listOf(createCustomTab("https://www.mozilla.org", id = "test-tab")),
                media = MediaState(
                    aggregate = MediaState.Aggregate(
                        state = MediaState.State.NONE
                    )
                )
            ).isMediaStateForCustomTab()
        )
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.media.Media
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.util.UUID

class MediaActionTest {
    @Test
    fun `AddMediaAction - Adds media for tab`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            )
        ))

        assertNull(store.state.media.elements["test-tab"])
        assertNull(store.state.media.elements["other-tab"])

        val element: MediaState.Element = mock()

        store.dispatch(MediaAction.AddMediaAction(
            "test-tab",
            element
        )).joinBlocking()

        assertNotNull(store.state.media.elements["test-tab"])
        assertEquals(1, store.state.media.elements["test-tab"]?.size)
        assertEquals(element, store.state.media.elements["test-tab"]?.getOrNull(0))
        assertNull(store.state.media.elements["other-tab"])
    }

    @Test
    fun `AddMediaAction - Not existing tab`() {
        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            )
        ))

        assertNull(store.state.media.elements["test-tab"])
        assertNull(store.state.media.elements["other-tab"])

        val element: MediaState.Element = mock()

        store.dispatch(MediaAction.AddMediaAction(
            "unknown-tab",
            element
        )).joinBlocking()

        assertNull(store.state.media.elements["test-tab"])
        assertNull(store.state.media.elements["other-tab"])
    }

    @Test
    fun `RemoveMediaAction - Removes media for tab`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        store.dispatch(MediaAction.RemoveMediaAction(
            "test-tab",
            element1
        )).joinBlocking()

        assertEquals(1, store.state.media.elements["test-tab"]?.size)
        assertEquals(1, store.state.media.elements["other-tab"]?.size)
        assertEquals(element2, store.state.media.elements["test-tab"]?.getOrNull(0))
        assertEquals(element3, store.state.media.elements["other-tab"]?.getOrNull(0))

        store.dispatch(MediaAction.RemoveMediaAction(
            "other-tab",
            element3
        )).joinBlocking()

        assertEquals(1, store.state.media.elements["test-tab"]?.size)
        assertNull(store.state.media.elements["other-tab"])
        assertEquals(element2, store.state.media.elements["test-tab"]?.getOrNull(0))
    }

    @Test
    fun `RemoveMediaAction - Not existing tab`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        store.dispatch(MediaAction.RemoveMediaAction(
            "unknown-tab",
            element1
        )).joinBlocking()

        assertEquals(2, store.state.media.elements["test-tab"]?.size)
        assertEquals(1, store.state.media.elements["other-tab"]?.size)
        assertEquals(element1, store.state.media.elements["test-tab"]?.getOrNull(0))
        assertEquals(element2, store.state.media.elements["test-tab"]?.getOrNull(1))
        assertEquals(element3, store.state.media.elements["other-tab"]?.getOrNull(0))
    }

    @Test
    fun `RemoveTabMediaAction - Removes all media for a tab`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()
        val element4 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab"),
                createTab("https;//getpocket.com", id = "pocket-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3),
                    "pocket-tab" to listOf(element4)
                )
            )
        ))

        store.dispatch(
            MediaAction.RemoveTabMediaAction(listOf("test-tab", "pocket-tab"))
        ).joinBlocking()

        assertEquals(1, store.state.media.elements["other-tab"]?.size)
        assertEquals(element3, store.state.media.elements["other-tab"]?.getOrNull(0))
        assertNull(store.state.media.elements["test-tab"])
        assertNull(store.state.media.elements["pocket-tab"])
    }

    @Test
    fun `UpdateMediaStateAction - Updates media state of element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)

        store.dispatch(MediaAction.UpdateMediaStateAction(
            tabId = "test-tab",
            mediaId = element2.id,
            state = Media.State.PAUSED
        )).joinBlocking()

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PAUSED, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)

        store.dispatch(MediaAction.UpdateMediaStateAction(
            tabId = "test-tab",
            mediaId = element1.id,
            state = Media.State.UNKNOWN
        )).joinBlocking()

        assertEquals(Media.State.UNKNOWN, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PAUSED, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)
    }

    @Test
    fun `UpdateMediaStateAction - Unknown media element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)

        store.dispatch(MediaAction.UpdateMediaStateAction(
            tabId = "test-tab",
            mediaId = "unknown media",
            state = Media.State.PAUSED
        )).joinBlocking()

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)
    }

    @Test
    fun `UpdateMediaStateAction - Unknown tab`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)

        store.dispatch(MediaAction.UpdateMediaStateAction(
            tabId = "unknown-tab",
            mediaId = element1.id,
            state = Media.State.PAUSED
        )).joinBlocking()

        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.state)
        assertEquals(Media.State.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.state)
    }

    @Test
    fun `UpdateMediaPlaybackStateAction - Updates media playback state of element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.playbackState)
        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(1)?.playbackState)
        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.playbackState)

        store.dispatch(MediaAction.UpdateMediaPlaybackStateAction(
            tabId = "test-tab",
            mediaId = element2.id,
            playbackState = Media.PlaybackState.EMPTIED
        )).joinBlocking()

        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.playbackState)
        assertEquals(Media.PlaybackState.EMPTIED, store.state.media.elements["test-tab"]?.getOrNull(1)?.playbackState)
        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["other-tab"]?.getOrNull(0)?.playbackState)

        store.dispatch(MediaAction.UpdateMediaPlaybackStateAction(
            tabId = "other-tab",
            mediaId = element3.id,
            playbackState = Media.PlaybackState.SEEKING
        )).joinBlocking()

        assertEquals(Media.PlaybackState.PLAYING, store.state.media.elements["test-tab"]?.getOrNull(0)?.playbackState)
        assertEquals(Media.PlaybackState.EMPTIED, store.state.media.elements["test-tab"]?.getOrNull(1)?.playbackState)
        assertEquals(Media.PlaybackState.SEEKING, store.state.media.elements["other-tab"]?.getOrNull(0)?.playbackState)
    }

    @Test
    fun `UpdateMediaMetadataAction - Updates media metadata of element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(-1.0, store.state.media.elements["test-tab"]?.getOrNull(0)?.metadata?.duration)
        assertEquals(-1.0, store.state.media.elements["test-tab"]?.getOrNull(1)?.metadata?.duration)
        assertEquals(-1.0, store.state.media.elements["other-tab"]?.getOrNull(0)?.metadata?.duration)

        store.dispatch(MediaAction.UpdateMediaMetadataAction(
            tabId = "test-tab",
            mediaId = element1.id,
            metadata = Media.Metadata(
                duration = 42.2
            )
        )).joinBlocking()

        assertEquals(42.2, store.state.media.elements["test-tab"]?.getOrNull(0)?.metadata?.duration)
        assertEquals(-1.0, store.state.media.elements["test-tab"]?.getOrNull(1)?.metadata?.duration)
        assertEquals(-1.0, store.state.media.elements["other-tab"]?.getOrNull(0)?.metadata?.duration)

        store.dispatch(MediaAction.UpdateMediaMetadataAction(
            tabId = "test-tab",
            mediaId = element2.id,
            metadata = Media.Metadata(
                duration = -13.37
            )
        )).joinBlocking()

        assertEquals(42.2, store.state.media.elements["test-tab"]?.getOrNull(0)?.metadata?.duration)
        assertEquals(-13.37, store.state.media.elements["test-tab"]?.getOrNull(1)?.metadata?.duration)
        assertEquals(-1.0, store.state.media.elements["other-tab"]?.getOrNull(0)?.metadata?.duration)

        store.dispatch(MediaAction.UpdateMediaMetadataAction(
            tabId = "test-tab",
            mediaId = element2.id,
            metadata = Media.Metadata(
                height = 400L,
                width = 200L
            )
        )).joinBlocking()

        assertEquals(0L, store.state.media.elements["test-tab"]?.getOrNull(0)?.metadata?.height)
        assertEquals(400L, store.state.media.elements["test-tab"]?.getOrNull(1)?.metadata?.height)
        assertEquals(200L, store.state.media.elements["test-tab"]?.getOrNull(1)?.metadata?.width)
        assertEquals(0L, store.state.media.elements["other-tab"]?.getOrNull(0)?.metadata?.height)
    }

    @Test
    fun `UpdateMediaVolumeAction - Updates media volume of element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(0)?.volume?.muted)
        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(1)?.volume?.muted)
        assertEquals(false, store.state.media.elements["other-tab"]?.getOrNull(0)?.volume?.muted)

        store.dispatch(MediaAction.UpdateMediaVolumeAction(
            tabId = "test-tab",
            mediaId = element1.id,
            volume = Media.Volume(
                muted = true
            )
        )).joinBlocking()

        assertEquals(true, store.state.media.elements["test-tab"]?.getOrNull(0)?.volume?.muted)
        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(1)?.volume?.muted)
        assertEquals(false, store.state.media.elements["other-tab"]?.getOrNull(0)?.volume?.muted)

        store.dispatch(MediaAction.UpdateMediaVolumeAction(
            tabId = "test-tab",
            mediaId = element2.id,
            volume = Media.Volume(
                muted = true
            )
        )).joinBlocking()

        assertEquals(true, store.state.media.elements["test-tab"]?.getOrNull(0)?.volume?.muted)
        assertEquals(true, store.state.media.elements["test-tab"]?.getOrNull(1)?.volume?.muted)
        assertEquals(false, store.state.media.elements["other-tab"]?.getOrNull(0)?.volume?.muted)
    }

    @Test
    fun `UpdateMediaFullscreenAction - Updates media fullscreen of element`() {
        val element1 = createMockMediaElement()
        val element2 = createMockMediaElement()
        val element3 = createMockMediaElement()

        val store = BrowserStore(BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "test-tab"),
                createTab("https://www.firefox.com", id = "other-tab")
            ),
            media = MediaState(
                elements = mapOf(
                    "test-tab" to listOf(element1, element2),
                    "other-tab" to listOf(element3)
                )
            )
        ))

        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(0)?.fullscreen)
        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(1)?.fullscreen)
        assertEquals(false, store.state.media.elements["other-tab"]?.getOrNull(0)?.fullscreen)

        store.dispatch(MediaAction.UpdateMediaFullscreenAction(
            tabId = "test-tab",
            mediaId = element1.id,
            fullScreen = true
        )).joinBlocking()

        assertEquals(true, store.state.media.elements["test-tab"]?.getOrNull(0)?.fullscreen)
        assertEquals(false, store.state.media.elements["test-tab"]?.getOrNull(1)?.fullscreen)
        assertEquals(false, store.state.media.elements["other-tab"]?.getOrNull(0)?.fullscreen)
    }

    @Test
    fun `UpdateMediaAggregateAction - Updates aggregate`() {
        val store = BrowserStore(BrowserState())

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)
        assertNull(store.state.media.aggregate.activeTabId)

        store.dispatch(MediaAction.UpdateMediaAggregateAction(
            MediaState.Aggregate(
                state = MediaState.State.PLAYING,
                activeTabId = "test-tab",
                activeMedia = listOf("a", "b", "c")
            )
        )).joinBlocking()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals(listOf("a", "b", "c"), store.state.media.aggregate.activeMedia)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)

        store.dispatch(MediaAction.UpdateMediaAggregateAction(
            MediaState.Aggregate(
                state = MediaState.State.PAUSED,
                activeTabId = "other-tab",
                activeMedia = listOf("a")
            )
        )).joinBlocking()

        assertEquals(MediaState.State.PAUSED, store.state.media.aggregate.state)
        assertEquals(listOf("a"), store.state.media.aggregate.activeMedia)
        assertEquals("other-tab", store.state.media.aggregate.activeTabId)

        store.dispatch(MediaAction.UpdateMediaAggregateAction(
            MediaState.Aggregate(
                state = MediaState.State.NONE
            )
        )).joinBlocking()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)
        assertNull(store.state.media.aggregate.activeTabId)
    }
}

private fun createMockMediaElement(): MediaState.Element {
    return MediaState.Element(
        id = UUID.randomUUID().toString(),
        state = Media.State.PLAYING,
        playbackState = Media.PlaybackState.PLAYING,
        controller = mock(),
        metadata = Media.Metadata(),
        volume = Media.Volume(),
        fullscreen = false
    )
}

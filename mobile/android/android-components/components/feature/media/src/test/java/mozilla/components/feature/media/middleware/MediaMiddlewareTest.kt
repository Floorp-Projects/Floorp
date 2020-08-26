/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.MediaAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.media.Media
import mozilla.components.feature.media.createMockMediaElement
import mozilla.components.feature.media.service.AbstractMediaService
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class MediaMiddlewareTest {
    @Test
    fun `State changes when existing media session pauses and new media session starts playing`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media = createMockMediaElement(
            id = "test-media",
            state = Media.State.PLAYING
        )

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media", store.state.media.aggregate.activeMedia[0])

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media", Media.State.PAUSED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PAUSED, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media", store.state.media.aggregate.activeMedia[0])

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media", Media.State.STOPPED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media", Media.State.PLAYING)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media", store.state.media.aggregate.activeMedia[0])
    }

    @Test
    fun `State is updated after media is removed`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media = createMockMediaElement(
            id = "test-media",
            state = Media.State.PLAYING
        )

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media", store.state.media.aggregate.activeMedia[0])

        store.dispatch(
            MediaAction.RemoveMediaAction("test-tab", media)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)
    }

    @Test
    fun `State is updated after session is removed`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media = createMockMediaElement(
            id = "test-media",
            state = Media.State.PLAYING
        )

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media", store.state.media.aggregate.activeMedia[0])

        store.dispatch(
            TabListAction.RemoveTabAction("test-tab")
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)
    }

    @Test
    fun `Multiple media of session start playing and stop`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media1 = createMockMediaElement(
            id = "test-media-1",
            state = Media.State.PLAYING
        )

        val media2 = createMockMediaElement(
            id = "test-media-2",
            state = Media.State.PLAYING
        )

        val media3 = createMockMediaElement(
            id = "test-media-3",
            state = Media.State.PLAYING
        )

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media1)
        ).joinBlocking()

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media2)
        ).joinBlocking()

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media3)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(3, store.state.media.aggregate.activeMedia.size)
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-1"))
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-2"))
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-3"))

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media-1", Media.State.PAUSED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(2, store.state.media.aggregate.activeMedia.size)
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-2"))
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-3"))

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media-3", Media.State.PAUSED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-2"))

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media-2", Media.State.STOPPED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertNull(store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)
    }

    @Test
    fun `Only media that was playing is added to paused state`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media1 = createMockMediaElement(
            id = "test-media-1",
            state = Media.State.PLAYING
        )

        val media2 = createMockMediaElement(
            id = "test-media-2",
            state = Media.State.PAUSED
        )

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media1)
        ).joinBlocking()

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media2)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media-1", store.state.media.aggregate.activeMedia[0])

        store.dispatch(
            MediaAction.UpdateMediaStateAction("test-tab", "test-media-1", Media.State.PAUSED)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PAUSED, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
        assertEquals("test-media-1", store.state.media.aggregate.activeMedia[0])
    }

    @Test
    fun `Does not switch to playing state if media has short duration`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab")
                )
            ),
            middleware = listOf(middleware)
        )

        val media1 = createMockMediaElement(
            id = "test-media-1",
            state = Media.State.PLAYING,
            metadata = Media.Metadata(duration = 2.0)
        )

        val media2 = createMockMediaElement(
            id = "test-media-2",
            state = Media.State.PLAYING,
            metadata = Media.Metadata(duration = 4.0)
        )

        val media3 = createMockMediaElement(
            id = "test-media-3",
            state = Media.State.PLAYING,
            metadata = Media.Metadata(duration = 30.0)
        )

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media1)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media2)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
            MediaAction.AddMediaAction("test-tab", media3)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(3, store.state.media.aggregate.activeMedia.size)
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-1"))
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-2"))
        assertTrue(store.state.media.aggregate.activeMedia.contains("test-media-3"))
    }

    @Test
    fun `Muting playing media stays in the playing state`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
                initialState = BrowserState(
                        tabs = listOf(
                                createTab("https://www.mozilla.org", id = "test-tab")
                        )
                ),
                middleware = listOf(middleware)
        )

        val media1 = createMockMediaElement(
                id = "test-media",
                state = Media.State.PLAYING,
                metadata = Media.Metadata(duration = 30.0),
                volume = Media.Volume(muted = false)
        )

        store.dispatch(
                MediaAction.AddMediaAction("test-tab", media1)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
                MediaAction.UpdateMediaVolumeAction("test-tab", "test-media", Media.Volume(muted = true))
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
    }

    @Test
    fun `Unmuting playing media enters the playing state`() {
        val middleware = MediaMiddleware(testContext, AbstractMediaService::class.java)

        val store = BrowserStore(
                initialState = BrowserState(
                        tabs = listOf(
                                createTab("https://www.mozilla.org", id = "test-tab")
                        )
                ),
                middleware = listOf(middleware)
        )

        val media1 = createMockMediaElement(
                id = "test-media",
                state = Media.State.PLAYING,
                metadata = Media.Metadata(duration = 30.0),
                volume = Media.Volume(muted = true)
        )

        store.dispatch(
                MediaAction.AddMediaAction("test-tab", media1)
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.NONE, store.state.media.aggregate.state)
        assertEquals(null, store.state.media.aggregate.activeTabId)
        assertEquals(0, store.state.media.aggregate.activeMedia.size)

        store.dispatch(
                MediaAction.UpdateMediaVolumeAction("test-tab", "test-media", Media.Volume(muted = false))
        ).joinBlocking()

        middleware.mediaAggregateUpdate.updateAggregateJob!!.joinBlocking()
        store.waitUntilIdle()

        assertEquals(MediaState.State.PLAYING, store.state.media.aggregate.state)
        assertEquals("test-tab", store.state.media.aggregate.activeTabId)
        assertEquals(1, store.state.media.aggregate.activeMedia.size)
    }
}

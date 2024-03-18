/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LastMediaAccessMiddlewareTest {

    @Test
    fun `GIVEN a normal tab WHEN media started playing THEN then lastMediaAccess is updated`() {
        val mediaTabId = "42"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(content = ContentState("https://mozilla.org/1", private = true)),
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = false),
                    id = mediaTabId,
                ),
                TabSessionState(content = ContentState("https://mozilla.org/3", private = false)),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.PLAYING))
            .joinBlocking()

        val updatedMediaState = store.state.tabs[1].lastMediaAccessState
        assertTrue(
            "expected lastMediaAccess (${updatedMediaState.lastMediaAccess}) > 0",
            updatedMediaState.lastMediaAccess > 0,
        )
        assertEquals(mediaTabUrl, updatedMediaState.lastMediaUrl)
    }

    @Test
    fun `GIVEN a private tab WHEN media started playing THEN then lastMediaAccess is updated`() {
        val mediaTabId = "43"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(content = ContentState("https://mozilla.org/1", private = true)),
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = true),
                    id = mediaTabId,
                ),
                TabSessionState(content = ContentState("https://mozilla.org/3", private = false)),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.PLAYING))
            .joinBlocking()

        val updatedMediaState = store.state.tabs[1].lastMediaAccessState
        assertTrue(
            "expected lastMediaAccess (${updatedMediaState.lastMediaAccess}) > 0",
            updatedMediaState.lastMediaAccess > 0,
        )
        assertEquals(mediaTabUrl, updatedMediaState.lastMediaUrl)
    }

    @Test
    fun `GIVEN a normal tab WHEN media is paused THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "42"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = false),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 222),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.PAUSED))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media is paused THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "43"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = true),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 333),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.PAUSED))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN a normal tab WHEN media is stopped THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "42"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = false),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 222),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.STOPPED))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media is stopped THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "43"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = true),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 333),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.STOPPED))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN a normal tab WHEN media status is unknown THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "42"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = false),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 222),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.UNKNOWN))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media status is unknown THEN then lastMediaAccess is not changed`() {
        val mediaTabId = "43"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = true),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 333),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(mediaTabId, MediaSession.PlaybackState.UNKNOWN))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
    }

    @Test
    fun `GIVEN lastMediaAccess is set for a normal tab WHEN media session is deactivated THEN reset mediaSessionActive to false`() {
        val mediaTabId = "42"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = false),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 222, true),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.DeactivatedMediaSessionAction(mediaTabId))
            .joinBlocking()

        assertEquals(mediaTabUrl, store.state.tabs[0].lastMediaAccessState.lastMediaUrl)
        assertEquals(222, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
        assertFalse(store.state.tabs[0].lastMediaAccessState.mediaSessionActive)
    }

    @Test
    fun `GIVEN lastMediaAccess is set for a private tab WHEN media session is deactivated THEN reset lastMediaAccess to 0`() {
        val mediaTabId = "43"
        val mediaTabUrl = "https://mozilla.org/2"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState(mediaTabUrl, private = true),
                    id = mediaTabId,
                    lastMediaAccessState = LastMediaAccessState(mediaTabUrl, 333, true),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware()),
        )

        store
            .dispatch(MediaSessionAction.DeactivatedMediaSessionAction(mediaTabId))
            .joinBlocking()

        assertEquals(mediaTabUrl, store.state.tabs[0].lastMediaAccessState.lastMediaUrl)
        assertEquals(333, store.state.tabs[0].lastMediaAccessState.lastMediaAccess)
        assertFalse(store.state.tabs[0].lastMediaAccessState.mediaSessionActive)
    }
}

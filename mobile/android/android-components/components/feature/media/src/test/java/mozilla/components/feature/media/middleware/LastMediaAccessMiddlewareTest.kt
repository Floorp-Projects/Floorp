/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.media.middleware

import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LastMediaAccessMiddlewareTest {

    @Test
    fun `GIVEN a normal tab WHEN media started playing THEN then lastMediaAccess is updated`() {
        val tabId = "42"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(content = ContentState("https://mozilla.org/1", private = true)),
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = false),
                    id = tabId
                ),
                TabSessionState(content = ContentState("https://mozilla.org/3", private = false))
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.PLAYING))
            .joinBlocking()

        val updatesLastMediaAccess = store.state.tabs[1].lastMediaAccess
        assertTrue("expected lastMediaAccess ($updatesLastMediaAccess) > 0", updatesLastMediaAccess > 0)
    }

    @Test
    fun `GIVEN a private tab WHEN media started playing THEN then lastMediaAccess is updated`() {
        val tabId = "43"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(content = ContentState("https://mozilla.org/1", private = true)),
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = true),
                    id = tabId
                ),
                TabSessionState(content = ContentState("https://mozilla.org/3", private = false))
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.PLAYING))
            .joinBlocking()

        val updatesLastMediaAccess = store.state.tabs[1].lastMediaAccess
        assertTrue("expected lastMediaAccess ($updatesLastMediaAccess) > 0", updatesLastMediaAccess > 0)
    }

    @Test
    fun `GIVEN a normal tab WHEN media is paused THEN then lastMediaAccess is not changed`() {
        val tabId = "42"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = false),
                    id = tabId,
                    lastMediaAccess = 222
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.PAUSED))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media is paused THEN then lastMediaAccess is not changed`() {
        val tabId = "43"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = true),
                    id = tabId,
                    lastMediaAccess = 333
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.PAUSED))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN a normal tab WHEN media is stopped THEN then lastMediaAccess is not changed`() {
        val tabId = "42"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = false),
                    id = tabId,
                    lastMediaAccess = 222
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.STOPPED))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media is stopped THEN then lastMediaAccess is not changed`() {
        val tabId = "43"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = true),
                    id = tabId,
                    lastMediaAccess = 333
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.STOPPED))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN a normal tab WHEN media status is unknown THEN then lastMediaAccess is not changed`() {
        val tabId = "42"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = false),
                    id = tabId,
                    lastMediaAccess = 222
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.UNKNOWN))
            .joinBlocking()

        assertEquals(222, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN a private tab WHEN media status is unknown THEN then lastMediaAccess is not changed`() {
        val tabId = "43"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = true),
                    id = tabId,
                    lastMediaAccess = 333
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.UpdateMediaPlaybackStateAction(tabId, MediaSession.PlaybackState.UNKNOWN))
            .joinBlocking()

        assertEquals(333, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN lastMediaAccess is set for a normal tab WHEN media session is deactivated THEN reset lastMediaAccess to 0`() {
        val tabId = "42"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = false),
                    id = tabId,
                    lastMediaAccess = 222
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.DeactivatedMediaSessionAction(tabId))
            .joinBlocking()

        assertEquals(0, store.state.tabs[0].lastMediaAccess)
    }

    @Test
    fun `GIVEN lastMediaAccess is set for a private tab WHEN media session is deactivated THEN reset lastMediaAccess to 0`() {
        val tabId = "43"
        val browserState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    content = ContentState("https://mozilla.org/2", private = true),
                    id = tabId,
                    lastMediaAccess = 333
                )
            )
        )
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(LastMediaAccessMiddleware())
        )

        store
            .dispatch(MediaSessionAction.DeactivatedMediaSessionAction(tabId))
            .joinBlocking()

        assertEquals(0, store.state.tabs[0].lastMediaAccess)
    }
}

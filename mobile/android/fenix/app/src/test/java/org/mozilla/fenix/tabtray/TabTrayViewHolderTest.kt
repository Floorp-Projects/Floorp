/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import android.view.View
import android.widget.ImageButton
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.MediaSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabTrayViewHolderTest {

    private lateinit var view: View
    @MockK private lateinit var imageLoader: ImageLoader
    @MockK private lateinit var store: BrowserStore
    @MockK private lateinit var sessionState: SessionState
    @MockK private lateinit var mediaSessionState: MediaSessionState
    @MockK private lateinit var metrics: MetricController
    private var state = BrowserState()

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        view = LayoutInflater.from(testContext)
            .inflate(R.layout.tab_tray_item, null, false)
        state = BrowserState()

        every { imageLoader.loadIntoView(any(), any(), any(), any()) } just Runs
        every { store.state } answers { state }
    }

    @Test
    fun `extremely long URLs are truncated to prevent slowing down the UI`() {
        val tabViewHolder = createViewHolder()

        val extremelyLongUrl = "m".repeat(MAX_URI_LENGTH + 1)
        val tab = Tab(
            id = "123",
            url = extremelyLongUrl
        )
        tabViewHolder.bind(tab, false, mockk(), mockk())

        assertEquals("m".repeat(MAX_URI_LENGTH), tabViewHolder.urlView?.text)
        verify { imageLoader.loadIntoView(any(), ImageLoadRequest("123", 92)) }
    }

    @Test
    fun `show play button if media is paused in tab`() {
        val playPauseButtonView: ImageButton = view.findViewById(R.id.play_pause_button)
        val tabViewHolder = createViewHolder()

        val tab = Tab(
            id = "123",
            url = "https://example.com"
        )

        state = state.copy(
            tabs = listOf(
                TabSessionState(
                    id = "123",
                    content = ContentState(
                        url = "https://example.com",
                        searchTerms = "search terms"
                    ),
                    mediaSessionState = mediaSessionState
                )
            )
        )

        every { mediaSessionState.playbackState } answers { MediaSession.PlaybackState.PAUSED }

        tabViewHolder.bind(tab, false, mockk(), mockk())

        assertEquals("Play", playPauseButtonView.contentDescription)
    }

    @Test
    fun `show pause button if media is playing in tab`() {
        val playPauseButtonView: ImageButton = view.findViewById(R.id.play_pause_button)
        val tabViewHolder = createViewHolder()

        val tab = Tab(
            id = "123",
            url = "https://example.com"
        )

        state = state.copy(
            tabs = listOf(
                TabSessionState(
                    id = "123",
                    content = ContentState(
                        url = "https://example.com",
                        searchTerms = "search terms"
                    ),
                    mediaSessionState = mediaSessionState
                )
            )
        )

        every { mediaSessionState.playbackState } answers { MediaSession.PlaybackState.PLAYING }

        tabViewHolder.bind(tab, false, mockk(), mockk())

        assertEquals("Pause", playPauseButtonView.contentDescription)
    }

    private fun createViewHolder() = TabTrayViewHolder(
        view,
        imageLoader = imageLoader,
        store = store,
        metrics = metrics
    )
}

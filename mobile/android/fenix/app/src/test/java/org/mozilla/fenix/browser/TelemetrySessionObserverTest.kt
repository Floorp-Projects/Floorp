/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.search.telemetry.ads.AdsTelemetry
import org.mozilla.fenix.utils.Settings

class TelemetrySessionObserverTest {

    private val settings: Settings = mockk(relaxed = true)
    private val owner: LifecycleOwner = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)
    private val ads: AdsTelemetry = mockk(relaxed = true)

    private lateinit var singleSessionObserver: TelemetrySessionObserver

    @Before
    fun setup() {
        singleSessionObserver =
            UriOpenedObserver(settings, owner, sessionManager, metrics, ads).singleSessionObserver
    }

    @Test
    fun `tracks that a url was loaded`() {
        val session: Session = mockk(relaxed = true)
        every { session.url } returns "https://mozilla.com"

        singleSessionObserver.onLoadingStateChanged(session, loading = false)
        verify(exactly = 0) { metrics.track(Event.UriOpened) }

        singleSessionObserver.onLoadingStateChanged(session, loading = true)
        singleSessionObserver.onLoadingStateChanged(session, loading = false)
        verify { metrics.track(Event.UriOpened) }
    }

    @Test
    fun `add originSessionUrl on first link of redirect chain and start chain`() {
        val session: Session = mockk(relaxed = true)
        val sessionUrl = "https://www.google.com/search"
        val url = "www.aaa.com"
        every { session.url } returns sessionUrl
        singleSessionObserver.onLoadRequest(
            session,
            url,
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )
        assertEquals(sessionUrl, singleSessionObserver.originSessionUrl)
        assertEquals(url, singleSessionObserver.redirectChain[0])
    }

    @Test
    fun `add to redirect chain on subsequent onLoadRequests`() {
        val session: Session = mockk(relaxed = true)
        val url = "https://www.google.com/search"
        val newUrl = "www.aaa.com"
        every { session.url } returns url
        singleSessionObserver.originSessionUrl = url
        singleSessionObserver.redirectChain.add(url)
        singleSessionObserver.onLoadRequest(
            session,
            newUrl,
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )
        assertEquals(url, singleSessionObserver.originSessionUrl)
        assertEquals(url, singleSessionObserver.redirectChain[0])
        assertEquals(newUrl, singleSessionObserver.redirectChain[1])
    }

    @Test
    fun `do nothing onLoadRequest when it's the first url of the session`() {
        val session: Session = mockk(relaxed = true)
        val url = "https://www.google.com/search"
        every { session.url } returns url
        singleSessionObserver.onLoadRequest(
            session,
            url,
            triggeredByRedirect = false,
            triggeredByWebContent = false
        )
        assertNull(singleSessionObserver.originSessionUrl)
        assertEquals(0, singleSessionObserver.redirectChain.size)
    }

    @Test
    fun `check if metric for ad clicked should be sent`() {
        val session: Session = mockk(relaxed = true)
        val sessionUrl = "doesn't matter"
        val originSessionUrl = "https://www.google.com/search"
        val url = "www.aaa.com"
        every { session.url } returns sessionUrl
        val redirectChain = mutableListOf(url)
        singleSessionObserver.redirectChain = redirectChain
        singleSessionObserver.originSessionUrl = originSessionUrl

        singleSessionObserver.onUrlChanged(session, url)

        verify {
            ads.trackAdClickedMetric(
                originSessionUrl,
                redirectChain
            )
        }
        assertNull(singleSessionObserver.originSessionUrl)
        assertEquals(0, singleSessionObserver.redirectChain.size)
    }
}

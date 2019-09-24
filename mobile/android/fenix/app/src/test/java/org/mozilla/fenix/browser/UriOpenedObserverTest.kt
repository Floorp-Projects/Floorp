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
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class UriOpenedObserverTest {

    private lateinit var owner: LifecycleOwner
    private lateinit var sessionManager: SessionManager
    private lateinit var metrics: MetricController

    @Before
    fun setup() {
        owner = mockk(relaxed = true)
        sessionManager = mockk(relaxed = true)
        metrics = mockk(relaxed = true)
    }

    @Test
    fun `registers self as observer`() {
        val observer = UriOpenedObserver(owner, sessionManager, metrics)
        verify { sessionManager.register(observer, owner) }
    }

    @Test
    fun `registers single session observer`() {
        val observer = UriOpenedObserver(owner, sessionManager, metrics)
        val session: Session = mockk(relaxed = true)

        observer.onSessionAdded(session)
        verify { session.register(observer.singleSessionObserver, owner) }

        observer.onSessionRemoved(session)
        verify { session.unregister(observer.singleSessionObserver) }
    }

    @Test
    fun `tracks that a url was loaded`() {
        val observer = UriOpenedObserver(owner, sessionManager, metrics).singleSessionObserver
        val session: Session = mockk(relaxed = true)
        every { session.url } returns "https://mozilla.com"

        observer.onLoadingStateChanged(session, loading = false)
        verify(exactly = 0) { metrics.track(Event.UriOpened) }

        observer.onLoadingStateChanged(session, loading = true)
        observer.onLoadingStateChanged(session, loading = false)
        verify { metrics.track(Event.UriOpened) }
    }
}

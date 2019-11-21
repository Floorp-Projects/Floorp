/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import android.view.View
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.session.Session
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class TrackingProtectionOverlayTest {

    private lateinit var context: Context
    private lateinit var settings: Settings
    private lateinit var toolbar: View
    private lateinit var icon: View
    private lateinit var session: Session
    private lateinit var overlay: TrackingProtectionOverlay

    @Before
    fun setup() {
        context = spyk(testContext)
        settings = mockk(relaxed = true)
        toolbar = mockk(relaxed = true)
        icon = mockk(relaxed = true)
        session = mockk(relaxed = true)

        overlay = TrackingProtectionOverlay(context, settings, toolbar, icon)
    }

    @Test
    fun `no-op when loading`() {
        every { settings.shouldShowTrackingProtectionOnboarding } returns true
        every { session.trackerBlockingEnabled } returns true
        every { session.trackersBlocked } returns listOf(mockk())

        overlay.onLoadingStateChanged(session, loading = true)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when should not show onboarding`() {
        every { settings.shouldShowTrackingProtectionOnboarding } returns false

        overlay.onLoadingStateChanged(session, loading = false)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when tracking protection disabled`() {
        every { settings.shouldShowTrackingProtectionOnboarding } returns true
        every { session.trackerBlockingEnabled } returns false

        overlay.onLoadingStateChanged(session, loading = false)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `no-op when no trackers blocked`() {
        every { settings.shouldShowTrackingProtectionOnboarding } returns true
        every { session.trackerBlockingEnabled } returns true
        every { session.trackersBlocked } returns emptyList()

        overlay.onLoadingStateChanged(session, loading = false)
        verify(exactly = 0) { settings.incrementTrackingProtectionOnboardingCount() }
    }

    @Test
    fun `show onboarding when trackers are blocked`() {
        every { settings.shouldShowTrackingProtectionOnboarding } returns true
        every { session.trackerBlockingEnabled } returns true
        every { session.trackersBlocked } returns listOf(mockk())

        overlay.onLoadingStateChanged(session, loading = false)
        verify { settings.incrementTrackingProtectionOnboardingCount() }
    }
}

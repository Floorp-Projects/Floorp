/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import junit.framework.Assert.assertEquals
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.app.links.AppLinkRedirect
import mozilla.components.feature.app.links.AppLinksUseCases
import org.junit.Test
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.Core
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics

class QuickActionInteractorTest {
    @Test
    fun onOpened() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetOpened) } just Runs

        interactor.onOpened()

        verify { metrics.track(Event.QuickActionSheetOpened) }
    }

    @Test
    fun onClosed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetClosed) } just Runs

        interactor.onClosed()

        verify { metrics.track(Event.QuickActionSheetClosed) }
    }

    @Test
    fun onSharedPressed() {
        val context: Context = mockk()
        val session: Session = mockk()
        var selectedSessionUrl = ""

        val metrics: MetricController = mockk()
        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            { selectedSessionUrl = it },
            mockk(),
            mockk()
        )

        val components: Components = mockk()
        val core: Core = mockk()
        val sessionManager: SessionManager = mockk()

        val analytics: Analytics = mockk()

        every { session.url } returns "mozilla.org"
        every { context.components } returns components
        every { components.analytics } returns analytics
        every { metrics.track(Event.QuickActionSheetShareTapped) } just Runs
        // Since we are mocking components, we must manually define metrics as `analytics.metrics`
        every { analytics.metrics } returns metrics
        every { components.core } returns core
        every { core.sessionManager } returns sessionManager
        every { sessionManager.selectedSession } returns session

        interactor.onSharedPressed()

        verify { metrics.track(Event.QuickActionSheetShareTapped) }
        assertEquals("mozilla.org", selectedSessionUrl)
    }

    @Test
    fun onDownloadsPressed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetDownloadTapped) } just Runs

        interactor.onDownloadsPressed()

        verify { metrics.track(Event.QuickActionSheetDownloadTapped) }
    }

    @Test
    fun onBookmarkPressed() {
        val context: Context = mockk()
        val session: Session = mockk()
        var bookmarkedSession: Session? = null

        val metrics: MetricController = mockk()
        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            { bookmarkedSession = it },
            mockk()
        )

        val components: Components = mockk()
        val core: Core = mockk()
        val sessionManager: SessionManager = mockk()

        val analytics: Analytics = mockk()

        every { session.url } returns "mozilla.org"
        every { context.components } returns components
        every { components.analytics } returns analytics
        every { metrics.track(Event.QuickActionSheetBookmarkTapped) } just Runs
        // Since we are mocking components, we must manually define metrics as `analytics.metrics`
        every { analytics.metrics } returns metrics
        every { components.core } returns core
        every { core.sessionManager } returns sessionManager
        every { sessionManager.selectedSession } returns session

        interactor.onBookmarkPressed()

        verify { metrics.track(Event.QuickActionSheetBookmarkTapped) }
        assertEquals("mozilla.org", bookmarkedSession?.url)
    }

    @Test
    fun onReadPressed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val session: Session = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val quickActionSheetStore: QuickActionSheetStore = mockk(relaxed = true)

        val interactor = QuickActionInteractor(
            context,
            readerModeController,
            quickActionSheetStore,
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { context.components.core.sessionManager.selectedSession } returns session
        every { session.readerMode } returns false
        every { metrics.track(Event.QuickActionSheetReadTapped) } just Runs

        interactor.onReadPressed()

        verify { metrics.track(Event.QuickActionSheetReadTapped) }
        verify { readerModeController.showReaderView() }
    }

    @Test
    fun onReadPressedWithActiveReaderMode() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val session: Session = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val quickActionSheetStore: QuickActionSheetStore = mockk(relaxed = true)

        val interactor = QuickActionInteractor(
            context,
            readerModeController,
            quickActionSheetStore,
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { context.components.core.sessionManager.selectedSession } returns session
        every { session.readerMode } returns true
        every { metrics.track(Event.QuickActionSheetReadTapped) } just Runs

        interactor.onReadPressed()

        verify { metrics.track(Event.QuickActionSheetReadTapped) }
        verify { readerModeController.hideReaderView() }
    }

    @Test
    fun onAppearancePressed() {
        val context: Context = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)

        val interactor = QuickActionInteractor(
            context,
            readerModeController,
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        interactor.onAppearancePressed()

        verify { readerModeController.showControls() }
    }

    @Test
    fun onOpenAppLink() {
        val context: Context = mockk()
        val session: Session = mockk()
        val appLinksUseCases: AppLinksUseCases = mockk()

        val interactor = QuickActionInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            appLinksUseCases
        )

        every { context.components.core.sessionManager.selectedSession } returns session
        every { session.url } returns "mozilla.org"

        val getAppLinkRedirect: AppLinksUseCases.GetAppLinkRedirect = mockk()
        val appLinkRedirect: AppLinkRedirect = mockk()
        val openAppLink: AppLinksUseCases.OpenAppLinkRedirect = mockk(relaxed = true)

        every { appLinksUseCases.appLinkRedirect } returns getAppLinkRedirect
        every { getAppLinkRedirect.invoke("mozilla.org") } returns appLinkRedirect
        every { appLinksUseCases.openAppLink } returns openAppLink
        every { appLinkRedirect.appIntent } returns mockk(relaxed = true)

        interactor.onOpenAppLinkPressed()

        verify { openAppLink.invoke(appLinkRedirect) }
    }
}

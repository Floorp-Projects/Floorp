package org.mozilla.fenix.components.toolbar

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.Session
import org.junit.Test

import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.quickactionsheet.QuickActionSheetController

class BrowserInteractorTest {

    @Test
    fun onBrowserToolbarClicked() {
        val context: Context = mockk()
        val browserToolbarController: BrowserToolbarController = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            mockk(),
            browserToolbarController,
            mockk(),
            mockk(),
            mockk()
        )

        interactor.onBrowserToolbarClicked()

        verify { browserToolbarController.handleToolbarClick() }
    }

    @Test
    fun onBrowserToolbarMenuItemTapped() {
        val context: Context = mockk()
        val browserToolbarController: BrowserToolbarController = mockk(relaxed = true)
        val item: ToolbarMenu.Item = mockk()

        val interactor = BrowserInteractor(
            context,
            mockk(),
            browserToolbarController,
            mockk(),
            mockk(),
            mockk()
        )

        interactor.onBrowserToolbarMenuItemTapped(item)

        verify { browserToolbarController.handleToolbarItemInteraction(item) }
    }

    @Test
    fun onQuickActionSheetOpened() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetOpened) } just Runs

        interactor.onQuickActionSheetOpened()

        verify { metrics.track(Event.QuickActionSheetOpened) }
    }

    @Test
    fun onQuickActionSheetClosed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetClosed) } just Runs

        interactor.onQuickActionSheetClosed()

        verify { metrics.track(Event.QuickActionSheetClosed) }
    }

    @Test
    fun onQuickActionSheetSharePressed() {
        val context: Context = mockk()
        val session: Session = mockk()
        val quickActionSheetController: QuickActionSheetController = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            quickActionSheetController,
            mockk(),
            session
        )

        interactor.onQuickActionSheetSharePressed()

        verify { quickActionSheetController.handleShare() }
    }

    @Test
    fun onQuickActionSheetDownloadPressed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val quickActionSheetController: QuickActionSheetController = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            quickActionSheetController,
            mockk(),
            mockk()
        )

        every { context.metrics } returns metrics
        every { metrics.track(Event.QuickActionSheetDownloadTapped) } just Runs

        interactor.onQuickActionSheetDownloadPressed()

        verify { quickActionSheetController.handleDownload() }
    }

    @Test
    fun onQuickActionSheetBookmarkPressed() {
        val context: Context = mockk()
        val session: Session = mockk()
        val quickActionSheetController: QuickActionSheetController = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            quickActionSheetController,
            mockk(),
            session
        )

        interactor.onQuickActionSheetBookmarkPressed()

        verify { quickActionSheetController.handleBookmark() }
    }

    @Test
    fun onQuickActionSheetReadPressed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val session: Session = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val browserStore: BrowserStore = mockk(relaxed = true)
        val interactor = BrowserInteractor(
            context,
            browserStore,
            mockk(),
            mockk(),
            readerModeController,
            session
        )

        every { context.metrics } returns metrics
        every { context.components.core.sessionManager.selectedSession } returns session
        every { session.readerMode } returns false
        every { metrics.track(Event.QuickActionSheetOpened) } just Runs

        interactor.onQuickActionSheetReadPressed()

        verify { metrics.track(Event.QuickActionSheetOpened) }
        verify { readerModeController.showReaderView() }
    }

    @Test
    fun onQuickActionSheetReadPressedWithActiveReaderMode() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val session: Session = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)
        val browserStore: BrowserStore = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            browserStore,
            mockk(),
            mockk(),
            readerModeController,
            session
        )

        every { context.metrics } returns metrics
        every { context.components.core.sessionManager.selectedSession } returns session
        every { session.readerMode } returns true
        every { metrics.track(Event.QuickActionSheetClosed) } just Runs

        interactor.onQuickActionSheetReadPressed()

        verify { metrics.track(Event.QuickActionSheetClosed) }
        verify { readerModeController.hideReaderView() }
    }

    @Test
    fun onQuickActionSheetOpenLinkPressed() {
        val context: Context = mockk()
        val session: Session = mockk()
        val quickActionSheetController: QuickActionSheetController = mockk(relaxed = true)

        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            quickActionSheetController,
            mockk(),
            session
        )

        interactor.onQuickActionSheetOpenLinkPressed()

        verify { quickActionSheetController.handleOpenLink() }
    }

    @Test
    fun onQuickActionSheetAppearancePressed() {
        val context: Context = mockk()
        val metrics: MetricController = mockk()
        val readerModeController: ReaderModeController = mockk(relaxed = true)

        every { context.metrics } returns metrics
        every { metrics.track(Event.ReaderModeAppearanceOpened) } just Runs

        val interactor = BrowserInteractor(
            context,
            mockk(),
            mockk(),
            mockk(),
            readerModeController,
            mockk()
        )

        interactor.onQuickActionSheetAppearancePressed()

        verify {
            metrics.track(Event.ReaderModeAppearanceOpened)
            readerModeController.showControls()
        }
    }
}

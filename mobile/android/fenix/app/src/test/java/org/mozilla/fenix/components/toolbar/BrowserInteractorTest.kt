package org.mozilla.fenix.components.toolbar

import android.content.Context
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.Session
import org.junit.Before
import org.junit.Test

import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.quickactionsheet.QuickActionSheetController

class BrowserInteractorTest {

    val metrics: MetricController = mockk()
    val context: Context = mockk()
    val browserStore: BrowserFragmentStore = mockk(relaxed = true)
    val browserToolbarController: BrowserToolbarController = mockk(relaxed = true)
    val quickActionSheetController: QuickActionSheetController = mockk(relaxed = true)
    val readerModeController: ReaderModeController = mockk(relaxed = true)
    val session: Session = mockk()

    val interactor = BrowserInteractor(
        context,
        browserStore,
        browserToolbarController,
        quickActionSheetController,
        readerModeController,
        session
    )

    @Before
    fun setup() {
        every { context.metrics } returns metrics
        every { context.components.core.sessionManager.selectedSession } returns session
    }

    @Test
    fun onBrowserToolbarPaste() {
        val pastedText = "Mozilla"
        interactor.onBrowserToolbarPaste(pastedText)
        verify { browserToolbarController.handleToolbarPaste(pastedText) }
    }

    @Test
    fun onBrowserToolbarPasteAndGo() {
        val pastedText = "Mozilla"

        interactor.onBrowserToolbarPasteAndGo(pastedText)
        verify { browserToolbarController.handleToolbarPasteAndGo(pastedText) }
    }

    @Test
    fun onBrowserToolbarClicked() {
        interactor.onBrowserToolbarClicked()

        verify { browserToolbarController.handleToolbarClick() }
    }

    @Test
    fun onBrowserToolbarMenuItemTapped() {
        val item: ToolbarMenu.Item = mockk()

        interactor.onBrowserToolbarMenuItemTapped(item)

        verify { browserToolbarController.handleToolbarItemInteraction(item) }
    }

    @Test
    fun onQuickActionSheetOpened() {
        every { metrics.track(Event.QuickActionSheetOpened) } just Runs

        interactor.onQuickActionSheetOpened()

        verify { metrics.track(Event.QuickActionSheetOpened) }
    }

    @Test
    fun onQuickActionSheetClosed() {
        every { metrics.track(Event.QuickActionSheetClosed) } just Runs

        interactor.onQuickActionSheetClosed()

        verify { metrics.track(Event.QuickActionSheetClosed) }
    }

    @Test
    fun onQuickActionSheetSharePressed() {
        interactor.onQuickActionSheetSharePressed()

        verify { quickActionSheetController.handleShare() }
    }

    @Test
    fun onQuickActionSheetDownloadPressed() {
        every { metrics.track(Event.QuickActionSheetDownloadTapped) } just Runs

        interactor.onQuickActionSheetDownloadPressed()

        verify { quickActionSheetController.handleDownload() }
    }

    @Test
    fun onQuickActionSheetBookmarkPressed() {
        interactor.onQuickActionSheetBookmarkPressed()

        verify { quickActionSheetController.handleBookmark() }
    }

    @Test
    fun onQuickActionSheetReadPressed() {
        every { session.readerMode } returns false
        every { metrics.track(Event.QuickActionSheetOpened) } just Runs

        interactor.onQuickActionSheetReadPressed()

        verify { metrics.track(Event.QuickActionSheetOpened) }
        verify { readerModeController.showReaderView() }
    }

    @Test
    fun onQuickActionSheetReadPressedWithActiveReaderMode() {
        every { session.readerMode } returns true
        every { metrics.track(Event.QuickActionSheetClosed) } just Runs

        interactor.onQuickActionSheetReadPressed()

        verify { metrics.track(Event.QuickActionSheetClosed) }
        verify { readerModeController.hideReaderView() }
    }

    @Test
    fun onQuickActionSheetOpenLinkPressed() {
        interactor.onQuickActionSheetOpenLinkPressed()

        verify { quickActionSheetController.handleOpenLink() }
    }

    @Test
    fun onQuickActionSheetAppearancePressed() {
        every { metrics.track(Event.ReaderModeAppearanceOpened) } just Runs

        interactor.onQuickActionSheetAppearancePressed()

        verify {
            metrics.track(Event.ReaderModeAppearanceOpened)
            readerModeController.showControls()
        }
    }
}

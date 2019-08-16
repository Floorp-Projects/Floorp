/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import mozilla.components.browser.session.Session
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.quickactionsheet.QuickActionSheetController
import org.mozilla.fenix.quickactionsheet.QuickActionSheetViewInteractor

class BrowserInteractor(
    private val context: Context,
    private val store: BrowserStore,
    private val browserToolbarController: BrowserToolbarController,
    private val quickActionSheetController: QuickActionSheetController,
    private val readerModeController: ReaderModeController,
    private val customTabSession: Session?
) : BrowserToolbarViewInteractor, QuickActionSheetViewInteractor {

    override fun onBrowserToolbarClicked() {
        browserToolbarController.handleToolbarClick()
    }

    override fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item) {
        browserToolbarController.handleToolbarItemInteraction(item)
    }

    override fun onQuickActionSheetOpened() {
        context.metrics.track(Event.QuickActionSheetOpened)
    }

    override fun onQuickActionSheetClosed() {
        context.metrics.track(Event.QuickActionSheetClosed)
    }

    override fun onQuickActionSheetSharePressed() {
        quickActionSheetController.handleShare()
    }

    override fun onQuickActionSheetDownloadPressed() {
        quickActionSheetController.handleDownload()
    }

    override fun onQuickActionSheetBookmarkPressed() {
        quickActionSheetController.handleBookmark()
    }

    override fun onQuickActionSheetReadPressed() {
        val enabled =
            customTabSession?.readerMode ?: context.components.core.sessionManager.selectedSession?.readerMode ?: false

        if (enabled) {
            context.metrics.track(Event.QuickActionSheetClosed)
            readerModeController.hideReaderView()
        } else {
            context.metrics.track(Event.QuickActionSheetOpened)
            readerModeController.showReaderView()
        }
        store.dispatch(QuickActionSheetAction.ReaderActiveStateChange(!enabled))
    }

    override fun onQuickActionSheetOpenLinkPressed() {
        quickActionSheetController.handleOpenLink()
    }

    override fun onQuickActionSheetAppearancePressed() {
        context.metrics.track(Event.ReaderModeAppearanceOpened)
        readerModeController.showControls()
    }
}

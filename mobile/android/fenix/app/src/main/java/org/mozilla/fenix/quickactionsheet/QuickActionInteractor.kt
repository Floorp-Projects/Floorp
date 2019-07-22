/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.content.Intent
import androidx.annotation.CallSuper
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinksUseCases
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.utils.ItsNotBrokenSnack

/**
 * Interactor for the QuickActionSheet
 */
class QuickActionInteractor(
    private val context: Context,
    private val readerModeController: ReaderModeController,
    private val quickActionStore: QuickActionSheetStore,
    private val shareUrl: (String) -> Unit,
    private val bookmarkTapped: (Session) -> Unit,
    private val appLinksUseCases: AppLinksUseCases
) : QuickActionSheetInteractor {

    private val selectedSession
        inline get() = context.components.core.sessionManager.selectedSession

    @CallSuper
    override fun onOpened() {
        context.metrics.track(Event.QuickActionSheetOpened)
    }

    @CallSuper
    override fun onClosed() {
        context.metrics.track(Event.QuickActionSheetClosed)
    }

    @CallSuper
    override fun onSharedPressed() {
        context.metrics.track(Event.QuickActionSheetShareTapped)
        selectedSession?.url?.let(shareUrl)
    }

    @CallSuper
    override fun onDownloadsPressed() {
        context.metrics.track(Event.QuickActionSheetDownloadTapped)
        ItsNotBrokenSnack(context).showSnackbar(issueNumber = "348")
    }

    @CallSuper
    override fun onBookmarkPressed() {
        context.metrics.track(Event.QuickActionSheetBookmarkTapped)
        selectedSession?.let(bookmarkTapped)
    }

    @CallSuper
    override fun onReadPressed() {
        context.metrics.track(Event.QuickActionSheetReadTapped)
        val enabled = selectedSession?.readerMode ?: false
        if (enabled) {
            readerModeController.hideReaderView()
        } else {
            readerModeController.showReaderView()
        }
        quickActionStore.dispatch(QuickActionSheetAction.ReaderActiveStateChange(!enabled))
    }

    @CallSuper
    override fun onOpenAppLinkPressed() {
        val getRedirect = appLinksUseCases.appLinkRedirect
        val redirect = selectedSession?.let {
            getRedirect.invoke(it.url)
        } ?: return

        redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        appLinksUseCases.openAppLink.invoke(redirect)
    }

    @CallSuper
    override fun onAppearancePressed() {
        // TODO telemetry: https://github.com/mozilla-mobile/fenix/issues/2267
        readerModeController.showControls()
    }
}

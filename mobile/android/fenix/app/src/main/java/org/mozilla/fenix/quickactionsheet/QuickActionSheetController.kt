/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.quickactionsheet

import android.content.Context
import android.content.Intent
import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinksUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.ItsNotBrokenSnack

/**
 * An interface that handles the view manipulation of the QuickActionSheet, triggered by the Interactor
 */
interface QuickActionSheetController {
    fun handleShare()
    fun handleDownload()
    fun handleBookmark()
    fun handleOpenLink()
}

class DefaultQuickActionSheetController(
    private val context: Context,
    private val navController: NavController,
    private val currentSession: Session,
    private val appLinksUseCases: AppLinksUseCases,
    private val bookmarkTapped: (Session) -> Unit
) : QuickActionSheetController {

    override fun handleShare() {
        context.metrics.track(Event.QuickActionSheetShareTapped)
        currentSession.url.let {
            val directions = BrowserFragmentDirections.actionBrowserFragmentToShareFragment(it)
            navController.nav(R.id.browserFragment, directions)
        }
    }

    override fun handleDownload() {
        context.metrics.track(Event.QuickActionSheetDownloadTapped)
        ItsNotBrokenSnack(context).showSnackbar(issueNumber = "348")
    }

    override fun handleBookmark() {
        context.metrics.track(Event.QuickActionSheetBookmarkTapped)
        bookmarkTapped(currentSession)
    }

    override fun handleOpenLink() {
        context.metrics.track(Event.QuickActionSheetOpenInAppTapped)

        val getRedirect = appLinksUseCases.appLinkRedirect
        val redirect = currentSession.let {
            getRedirect.invoke(it.url)
        }

        redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        appLinksUseCases.openAppLink.invoke(redirect)
    }
}

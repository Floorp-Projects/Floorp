/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.fenix.quickactionsheet

import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.session.Session
import mozilla.components.feature.app.links.AppLinksUseCases
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.ItsNotBrokenSnack

class DefaultQuickActionSheetControllerTest {
    private val context: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val currentSession: Session = mockk(relaxed = true)
    private val appLinksUseCases: AppLinksUseCases = mockk(relaxed = true)
    private val bookmarkTapped: (Session) -> Unit = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    private lateinit var controller: DefaultQuickActionSheetController

    @Before
    fun setUp() {
        controller = DefaultQuickActionSheetController(
            context,
            navController,
            currentSession,
            appLinksUseCases,
            bookmarkTapped
        )

        every { context.metrics } returns metrics
    }

    @Test
    fun handleShare() {
        controller.handleShare()

        verify { metrics.track(Event.QuickActionSheetShareTapped) }
        verify { navController.nav(R.id.browserFragment, BrowserFragmentDirections.actionBrowserFragmentToShareFragment(currentSession.url)) }
    }

    @Test
    fun handleDownload() {
        controller.handleDownload()

        verify { metrics.track(Event.QuickActionSheetDownloadTapped) }
        verify { ItsNotBrokenSnack(context).showSnackbar(issueNumber = "348") }
    }

    @Test
    fun handleBookmark() {
        controller.handleBookmark()

        verify { metrics.track(Event.QuickActionSheetBookmarkTapped) }
        verify { bookmarkTapped(currentSession) }
    }

    @Test
    fun handleOpenLink() {
        controller.handleOpenLink()

        verify { metrics.track(Event.QuickActionSheetOpenInAppTapped) }
        verify { appLinksUseCases.appLinkRedirect.invoke(currentSession.url) }
    }
}
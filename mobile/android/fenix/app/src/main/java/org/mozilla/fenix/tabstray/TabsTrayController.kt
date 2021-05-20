/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.navigateBlockingForAsyncNavGraph
import org.mozilla.fenix.tabtray.TabTrayDialogFragmentDirections

interface TabsTrayController {

    /**
     * Called to open a new tab.
     */
    fun handleOpeningNewTab(isPrivate: Boolean)
}

class DefaultTabsTrayController(
    private val browsingModeManager: BrowsingModeManager,
    private val navController: NavController,
    private val profiler: Profiler?,
    private val navigationInteractor: NavigationInteractor,
    private val metrics: MetricController,
) : TabsTrayController {

    override fun handleOpeningNewTab(isPrivate: Boolean) {
        val startTime = profiler?.getProfilerTime()
        browsingModeManager.mode = BrowsingMode.fromBoolean(isPrivate)
        navController.navigateBlockingForAsyncNavGraph(
            TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true))
        navigationInteractor.onTabTrayDismissed()
        profiler?.addMarker(
            "DefaultTabTrayController.onNewTabTapped",
            startTime
        )
        sendNewTabEvent(isPrivate)
    }

    private fun sendNewTabEvent(isPrivateModeSelected: Boolean) {
        val eventToSend = if (isPrivateModeSelected) {
            Event.NewPrivateTabTapped
        } else {
            Event.NewTabTapped
        }

        metrics.track(eventToSend)
    }
}

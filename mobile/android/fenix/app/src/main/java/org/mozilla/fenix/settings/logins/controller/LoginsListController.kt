/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.controller

import androidx.navigation.NavController
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.logins.LoginsAction
import org.mozilla.fenix.settings.logins.LoginsFragmentStore
import org.mozilla.fenix.settings.logins.SavedLogin
import org.mozilla.fenix.settings.logins.SortingStrategy
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsFragmentDirections
import org.mozilla.fenix.utils.Settings

/**
 * Controller for the saved logins list
 *
 * @param loginsFragmentStore Store used to hold in-memory collection state.
 * @param navController NavController manages app navigation within a NavHost.
 * @param browserNavigator Controller allowing browser navigation to any Uri.
 * @param settings SharedPreferences wrapper for easier usage.
 * @param metrics Controller that handles telemetry events.
 */
class LoginsListController(
    private val loginsFragmentStore: LoginsFragmentStore,
    private val navController: NavController,
    private val browserNavigator: (
        searchTermOrURL: String,
        newTab: Boolean,
        from: BrowserDirection
    ) -> Unit,
    private val settings: Settings,
    private val metrics: MetricController
) {

    fun handleItemClicked(item: SavedLogin) {
        loginsFragmentStore.dispatch(LoginsAction.LoginSelected(item))
        metrics.track(Event.OpenOneLogin)
        navController.navigate(
            SavedLoginsFragmentDirections.actionSavedLoginsFragmentToLoginDetailFragment(item.guid)
        )
    }

    fun handleLearnMoreClicked() {
        browserNavigator.invoke(
            SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SYNC_SETUP),
            true,
            BrowserDirection.FromSavedLoginsFragment
        )
    }

    fun handleSort(sortingStrategy: SortingStrategy) {
        loginsFragmentStore.dispatch(
            LoginsAction.SortLogins(
                sortingStrategy
            )
        )
        settings.savedLoginsSortingStrategy = sortingStrategy
    }
}

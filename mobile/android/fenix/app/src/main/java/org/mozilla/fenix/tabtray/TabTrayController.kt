/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.prompt.ShareData
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.home.HomeFragment

/**
 * [TabTrayDialogFragment] controller.
 *
 * Delegated by View Interactors, handles container business logic and operates changes on it.
 */
interface TabTrayController {
    fun onNewTabTapped(private: Boolean)
    fun onTabTrayDismissed()
    fun onShareTabsClicked(private: Boolean)
    fun onSaveToCollectionClicked()
    fun onCloseAllTabsClicked(private: Boolean)
}

@Suppress("TooManyFunctions")
class DefaultTabTrayController(
    private val activity: HomeActivity,
    private val navController: NavController,
    private val dismissTabTray: () -> Unit,
    private val dismissTabTrayAndNavigateHome: (String) -> Unit,
    private val registerCollectionStorageObserver: () -> Unit
) : TabTrayController {
    override fun onNewTabTapped(private: Boolean) {
        val startTime = activity.components.core.engine.profiler?.getProfilerTime()
        activity.browsingModeManager.mode = BrowsingMode.fromBoolean(private)
        navController.navigate(TabTrayDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true))
        dismissTabTray()
        activity.components.core.engine.profiler?.addMarker("DefaultTabTrayController.onNewTabTapped", startTime)
    }

    override fun onTabTrayDismissed() {
        dismissTabTray()
    }

    override fun onSaveToCollectionClicked() {
        val tabs = getListOfSessions(false)
        val tabIds = tabs.map { it.id }.toList().toTypedArray()
        val tabCollectionStorage = activity.components.core.tabCollectionStorage

        val step = when {
            // Show the SelectTabs fragment if there are multiple opened tabs to select which tabs
            // you want to save to a collection.
            tabs.size > 1 -> SaveCollectionStep.SelectTabs
            // If there is an existing tab collection, show the SelectCollection fragment to save
            // the selected tab to a collection of your choice.
            tabCollectionStorage.cachedTabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
            // Show the NameCollection fragment to create a new collection for the selected tab.
            else -> SaveCollectionStep.NameCollection
        }

        if (navController.currentDestination?.id == R.id.collectionCreationFragment) return

        // Only register the observer right before moving to collection creation
        registerCollectionStorageObserver()

        val directions = TabTrayDialogFragmentDirections.actionGlobalCollectionCreationFragment(
            tabIds = tabIds,
            saveCollectionStep = step,
            selectedTabIds = tabIds
        )
        navController.navigate(directions)
    }

    override fun onShareTabsClicked(private: Boolean) {
        val tabs = getListOfSessions(private)
        val data = tabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = TabTrayDialogFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        navController.navigate(directions)
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    override fun onCloseAllTabsClicked(private: Boolean) {
        val sessionsToClose = if (private) {
            HomeFragment.ALL_PRIVATE_TABS
        } else {
            HomeFragment.ALL_NORMAL_TABS
        }

        dismissTabTrayAndNavigateHome(sessionsToClose)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    private fun getListOfSessions(private: Boolean): List<Session> {
        return activity.components.core.sessionManager.sessionsOfType(private = private).toList()
    }
}

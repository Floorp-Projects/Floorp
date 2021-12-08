/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import mozilla.components.browser.state.action.DebugAction
import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.DelicateAction
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.ext.DEFAULT_ACTIVE_DAYS
import org.mozilla.fenix.ext.potentialInactiveTabs
import java.util.concurrent.TimeUnit

interface TabsTrayController {

    /**
     * Called to open a new tab.
     */
    fun handleOpeningNewTab(isPrivate: Boolean)

    /**
     * Set the current tray item to the clamped [position].
     *
     * @param position The position on the tray to focus.
     * @param smoothScroll If true, animate the scrolling from the current tab to [position].
     */
    fun handleTrayScrollingToPosition(position: Int, smoothScroll: Boolean)

    /**
     * Navigate from TabsTray to Browser.
     */
    fun handleNavigateToBrowser()

    /**
     * Deletes the [TabSessionState] with the specified [tabId].
     *
     * @param tabId The id of the [TabSessionState] to be removed from TabsTray.
     */
    fun handleTabDeletion(tabId: String)

    /**
     * Deletes a list of [tabs].
     *
     * @param tabs List of [TabSessionState]s (sessions) to be removed.
     */
    fun handleMultipleTabsDeletion(tabs: Collection<TabSessionState>)

    /**
     * Moves [tabId] next to before/after [targetId]
     *
     * @param tabId The tabs to be moved
     * @param targetId The id of the tab that the [tab] will be placed next to
     * @param placeAfter Place [tabs] before or after the target
     */
    fun handleTabsMove(tabId: String, targetId: String?, placeAfter: Boolean)

    /**
     * Navigate from TabsTray to Recently Closed section in the History fragment.
     */
    fun handleNavigateToRecentlyClosed()

    /**
     * Set the list of [tabs] into the inactive state.
     *
     * ⚠️ DO NOT USE THIS OUTSIDE OF DEBUGGING/TESTING.
     *
     * @param tabs List of [TabSessionState]s to be removed.
     */
    fun forceTabsAsInactive(
        tabs: Collection<TabSessionState>,
        numOfDays: Long = DEFAULT_ACTIVE_DAYS + 1
    )

    /**
     * Deletes all inactive tabs.
     */
    fun handleDeleteAllInactiveTabs()
}

@Suppress("TooManyFunctions")
class DefaultTabsTrayController(
    private val trayStore: TabsTrayStore,
    private val browserStore: BrowserStore,
    private val browsingModeManager: BrowsingModeManager,
    private val navController: NavController,
    private val navigateToHomeAndDeleteSession: (String) -> Unit,
    private val profiler: Profiler?,
    private val navigationInteractor: NavigationInteractor,
    private val metrics: MetricController,
    private val tabsUseCases: TabsUseCases,
    private val selectTabPosition: (Int, Boolean) -> Unit,
    private val dismissTray: () -> Unit,
    private val showUndoSnackbarForTab: (Boolean) -> Unit

) : TabsTrayController {

    override fun handleOpeningNewTab(isPrivate: Boolean) {
        val startTime = profiler?.getProfilerTime()
        browsingModeManager.mode = BrowsingMode.fromBoolean(isPrivate)
        navController.navigate(
            TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
        )
        navigationInteractor.onTabTrayDismissed()
        profiler?.addMarker(
            "DefaultTabTrayController.onNewTabTapped",
            startTime
        )
        sendNewTabEvent(isPrivate)
    }

    override fun handleTrayScrollingToPosition(position: Int, smoothScroll: Boolean) {
        selectTabPosition(position, smoothScroll)
        trayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(position)))
    }

    /**
     * Dismisses the tabs tray and navigates to the browser.
     */
    override fun handleNavigateToBrowser() {
        dismissTray()

        if (navController.currentDestination?.id == R.id.browserFragment) {
            return
        } else if (!navController.popBackStack(R.id.browserFragment, false)) {
            navController.navigate(R.id.browserFragment)
        }
    }

    /**
     * Deletes the [TabSessionState] with the specified [tabId].
     *
     * @param tabId The id of the [TabSessionState] to be removed from TabsTray.
     * This method has no effect if the tab does not exist.
     */
    override fun handleTabDeletion(tabId: String) {
        val tab = browserStore.state.findTab(tabId)

        tab?.let {
            if (browserStore.state.getNormalOrPrivateTabs(it.content.private).size != 1) {
                tabsUseCases.removeTab(tabId)
                showUndoSnackbarForTab(it.content.private)
            } else {
                dismissTabsTrayAndNavigateHome(tabId)
            }
        }
    }

    /**
     * Deletes a list of [tabs] offering an undo option.
     *
     * @param tabs List of [TabSessionState]s (sessions) to be removed.
     * This method has no effect for tabs that do not exist.
     */
    override fun handleMultipleTabsDeletion(tabs: Collection<TabSessionState>) {
        val isPrivate = tabs.any { it.content.private }

        // If user closes all the tabs from selected tabs page dismiss tray and navigate home.
        if (tabs.size == browserStore.state.getNormalOrPrivateTabs(isPrivate).size) {
            dismissTabsTrayAndNavigateHome(
                if (isPrivate) HomeFragment.ALL_PRIVATE_TABS else HomeFragment.ALL_NORMAL_TABS
            )
        } else {
            tabs.map { it.id }.let {
                tabsUseCases.removeTabs(it)
            }
        }
        showUndoSnackbarForTab(isPrivate)
    }

    /**
     * Moves [tabId] next to before/after [targetId]
     *
     * @param tabId The tabs to be moved
     * @param targetId The id of the tab that the [tab] will be placed next to
     * @param placeAfter Place [tabs] before or after the target
     */
    override fun handleTabsMove(
        tabId: String,
        targetId: String?,
        placeAfter: Boolean
    ) {
        if (targetId != null && tabId != targetId) {
            tabsUseCases.moveTabs(listOf(tabId), targetId, placeAfter)
        }
    }

    /**
     * Dismisses the tabs tray and navigates to the Recently Closed section in the History fragment.
     */
    override fun handleNavigateToRecentlyClosed() {
        dismissTray()

        navController.navigate(R.id.recentlyClosedFragment)

        metrics.track(Event.TabsTrayRecentlyClosedPressed)
    }

    /**
     * Marks all the [tabs] with the [TabSessionState.lastAccess] to 15 days; enough time to
     * have a tab considered as inactive.
     *
     * ⚠️ DO NOT USE THIS OUTSIDE OF DEBUGGING/TESTING.
     */
    @OptIn(DelicateAction::class)
    override fun forceTabsAsInactive(tabs: Collection<TabSessionState>, numOfDays: Long) {
        tabs.forEach { tab ->
            val daysSince = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(numOfDays)
            browserStore.apply {
                dispatch(LastAccessAction.UpdateLastAccessAction(tab.id, daysSince))
                dispatch(DebugAction.UpdateCreatedAtAction(tab.id, daysSince))
            }
        }
    }

    @VisibleForTesting
    internal fun sendNewTabEvent(isPrivateModeSelected: Boolean) {
        val eventToSend = if (isPrivateModeSelected) {
            Event.NewPrivateTabTapped
        } else {
            Event.NewTabTapped
        }

        metrics.track(eventToSend)
    }

    @VisibleForTesting
    internal fun dismissTabsTrayAndNavigateHome(sessionId: String) {
        dismissTray()
        navigateToHomeAndDeleteSession(sessionId)
    }

    override fun handleDeleteAllInactiveTabs() {
        metrics.track(Event.TabsTrayCloseAllInactiveTabs)
        browserStore.state.potentialInactiveTabs.map { it.id }.let {
            tabsUseCases.removeTabs(it)
        }
        showUndoSnackbarForTab(false)
    }
}

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
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.mediasession.MediaSession.PlaybackState
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.state.DelicateAction
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.DEFAULT_ACTIVE_DAYS
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.tabstray.ext.isActiveDownload
import java.util.concurrent.TimeUnit
import org.mozilla.fenix.GleanMetrics.Tab as GleanTab

/**
 * Controller for handling any actions in the tabs tray.
 */
interface TabsTrayController : SyncedTabsController {

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
     * Deletes the [TabSessionState] with the specified [tabId] or calls [DownloadCancelDialogFragment]
     * if user tries to close the last private tab while private downloads are active.
     * Tracks [Event.ClosedExistingTab] in case of deletion.
     *
     * @param tabId The id of the [TabSessionState] to be removed from TabsTray.
     * @param source app feature from which the tab with [tabId] was closed.
     */
    fun handleTabDeletion(tabId: String, source: String? = null)

    /**
     * Deletes the [TabSessionState] with the specified [tabId]
     * Tracks [Event.ClosedExistingTab] in case of deletion.
     *
     * @param tabId The id of the [TabSessionState] to be removed from TabsTray.
     * @param source app feature from which the tab with [tabId] was closed.
     */
    fun handleDeleteTabWarningAccepted(tabId: String, source: String? = null)

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
        numOfDays: Long = DEFAULT_ACTIVE_DAYS + 1,
    )

    /**
     * Handles when a tab item is click either to play/pause.
     */
    fun handleMediaClicked(tab: SessionState)
}

/**
 * Default implementation of [TabsTrayController].
 *
 * @property activity [HomeActivity] used to perform top-level app actions.
 * @property trayStore [TabsTrayStore] used to read/update the [TabsTrayState].
 * @property browserStore [BrowserStore] used to read/update the current [BrowserState].
 * @property browsingModeManager [BrowsingModeManager] used to read/update the current [BrowsingMode].
 * @property navController [NavController] used to navigate away from the tabs tray.
 * @property navigateToHomeAndDeleteSession Lambda used to return to the Homescreen and delete the current session.
 * @property navigationInteractor [NavigationInteractor] used to perform navigation actions with side effects.
 * @property tabsUseCases Use case wrapper for interacting with tabs.
 * @property selectTabPosition Lambda used to scroll the tabs tray to the desired position.
 * @property dismissTray Lambda used to dismiss/minimize the tabs tray.
 * @property showUndoSnackbarForTab Lambda used to display an UNDO Snackbar.
 * @property showCancelledDownloadWarning Lambda used to display a cancelled download warning.
 */
@Suppress("TooManyFunctions", "LongParameterList")
class DefaultTabsTrayController(
    private val activity: HomeActivity,
    private val trayStore: TabsTrayStore,
    private val browserStore: BrowserStore,
    private val browsingModeManager: BrowsingModeManager,
    private val navController: NavController,
    private val navigateToHomeAndDeleteSession: (String) -> Unit,
    private val profiler: Profiler?,
    private val navigationInteractor: NavigationInteractor,
    private val tabsUseCases: TabsUseCases,
    private val selectTabPosition: (Int, Boolean) -> Unit,
    private val dismissTray: () -> Unit,
    private val showUndoSnackbarForTab: (Boolean) -> Unit,
    internal val showCancelledDownloadWarning: (downloadCount: Int, tabId: String?, source: String?) -> Unit,
) : TabsTrayController {

    override fun handleOpeningNewTab(isPrivate: Boolean) {
        val startTime = profiler?.getProfilerTime()
        browsingModeManager.mode = BrowsingMode.fromBoolean(isPrivate)
        navController.navigate(
            TabsTrayFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
        )
        navigationInteractor.onTabTrayDismissed()
        profiler?.addMarker(
            "DefaultTabTrayController.onNewTabTapped",
            startTime,
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
     * @param source app feature from which the tab with [tabId] was closed.
     * This method has no effect if the tab does not exist.
     */
    override fun handleTabDeletion(tabId: String, source: String?) {
        deleteTab(tabId, source, isConfirmed = false)
    }

    override fun handleDeleteTabWarningAccepted(tabId: String, source: String?) {
        deleteTab(tabId, source, isConfirmed = true)
    }

    private fun deleteTab(tabId: String, source: String?, isConfirmed: Boolean) {
        val tab = browserStore.state.findTab(tabId)

        tab?.let {
            val isLastTab = browserStore.state.getNormalOrPrivateTabs(it.content.private).size == 1
            if (!isLastTab) {
                tabsUseCases.removeTab(tabId)
                showUndoSnackbarForTab(it.content.private)
            } else {
                val privateDownloads = browserStore.state.downloads.filter { map ->
                    map.value.private && map.value.isActiveDownload()
                }
                if (!isConfirmed && privateDownloads.isNotEmpty()) {
                    showCancelledDownloadWarning(privateDownloads.size, tabId, source)
                    return
                } else {
                    dismissTabsTrayAndNavigateHome(tabId)
                }
            }
            TabsTray.closedExistingTab.record(TabsTray.ClosedExistingTabExtra(source ?: "unknown"))
        }
    }

    /**
     * Deletes a list of [tabs] offering an undo option.
     *
     * @param tabs List of [TabSessionState]s (sessions) to be removed.
     * This method has no effect for tabs that do not exist.
     */
    override fun handleMultipleTabsDeletion(tabs: Collection<TabSessionState>) {
        TabsTray.closeSelectedTabs.record(TabsTray.CloseSelectedTabsExtra(tabCount = tabs.size))

        val isPrivate = tabs.any { it.content.private }

        // If user closes all the tabs from selected tabs page dismiss tray and navigate home.
        if (tabs.size == browserStore.state.getNormalOrPrivateTabs(isPrivate).size) {
            dismissTabsTrayAndNavigateHome(
                if (isPrivate) HomeFragment.ALL_PRIVATE_TABS else HomeFragment.ALL_NORMAL_TABS,
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
        placeAfter: Boolean,
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
    }

    /**
     * Marks all the [tabs] with the [TabSessionState.lastAccess] to 15 days; enough time to
     * have a tab considered as inactive.
     *
     * ⚠️ DO NOT USE THIS OUTSIDE OF DEBUGGING/TESTING.
     */
    @OptIn(DelicateAction::class)
    override fun forceTabsAsInactive(tabs: Collection<TabSessionState>, numOfDays: Long) {
        val currentTabId = browserStore.state.selectedTabId
        tabs
            .filterNot { it.id == currentTabId }
            .forEach { tab ->
                val daysSince = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(numOfDays)
                browserStore.apply {
                    dispatch(LastAccessAction.UpdateLastAccessAction(tab.id, daysSince))
                    dispatch(DebugAction.UpdateCreatedAtAction(tab.id, daysSince))
                }
            }
    }

    @VisibleForTesting
    internal fun sendNewTabEvent(isPrivateModeSelected: Boolean) {
        if (isPrivateModeSelected) {
            TabsTray.newPrivateTabTapped.record(NoExtras())
        } else {
            TabsTray.newTabTapped.record(NoExtras())
        }
    }

    @VisibleForTesting
    internal fun dismissTabsTrayAndNavigateHome(sessionId: String) {
        dismissTray()
        navigateToHomeAndDeleteSession(sessionId)
    }

    override fun handleMediaClicked(tab: SessionState) {
        when (tab.mediaSessionState?.playbackState) {
            PlaybackState.PLAYING -> {
                GleanTab.mediaPause.record(NoExtras())
                tab.mediaSessionState?.controller?.pause()
            }

            PlaybackState.PAUSED -> {
                GleanTab.mediaPlay.record(NoExtras())
                tab.mediaSessionState?.controller?.play()
            }
            else -> throw AssertionError(
                "Play/Pause button clicked without play/pause state.",
            )
        }
    }

    override fun handleSyncedTabClicked(tab: Tab) {
        Events.syncedTabOpened.record(NoExtras())

        dismissTray()
        activity.openToBrowserAndLoad(
            searchTermOrURL = tab.active().url,
            newTab = true,
            from = BrowserDirection.FromTabsTray,
        )
    }
}

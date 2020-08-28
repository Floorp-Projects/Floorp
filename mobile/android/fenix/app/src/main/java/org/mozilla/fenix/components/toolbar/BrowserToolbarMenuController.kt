/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.session.SessionFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Do
import org.mozilla.fenix.utils.Settings

/**
 * An interface that handles events from the BrowserToolbar menu, triggered by the Interactor
 */
interface BrowserToolbarMenuController {
    fun handleToolbarItemInteraction(item: ToolbarMenu.Item)
}

@Suppress("LargeClass")
class DefaultBrowserToolbarMenuController(
    private val activity: HomeActivity,
    private val navController: NavController,
    private val metrics: MetricController,
    private val settings: Settings,
    private val readerModeController: ReaderModeController,
    private val sessionFeature: ViewBoundFeatureWrapper<SessionFeature>,
    private val sessionManager: SessionManager,
    private val findInPageLauncher: () -> Unit,
    private val browserAnimator: BrowserAnimator,
    private val swipeRefresh: SwipeRefreshLayout,
    private val customTabSession: Session?,
    private val openInFenixIntent: Intent,
    private val bookmarkTapped: (Session) -> Unit,
    private val scope: CoroutineScope,
    private val tabCollectionStorage: TabCollectionStorage
) : BrowserToolbarMenuController {

    private val currentSession
        get() = customTabSession ?: sessionManager.selectedSession

    // We hold onto a reference of the inner scope so that we can override this with the
    // TestCoroutineScope to ensure sequential execution. If we didn't have this, our tests
    // would fail intermittently due to the async nature of coroutine scheduling.
    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    @Suppress("ComplexMethod", "LongMethod")
    override fun handleToolbarItemInteraction(item: ToolbarMenu.Item) {
        val sessionUseCases = activity.components.useCases.sessionUseCases
        trackToolbarItemInteraction(item)

        Do exhaustive when (item) {
            is ToolbarMenu.Item.Back -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment()
                    )
                } else {
                    sessionUseCases.goBack.invoke(currentSession)
                }
            }
            is ToolbarMenu.Item.Forward -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment()
                    )
                } else {
                    sessionUseCases.goForward.invoke(currentSession)
                }
            }
            is ToolbarMenu.Item.Reload -> {
                val flags = if (item.bypassCache) {
                    LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE)
                } else {
                    LoadUrlFlags.none()
                }

                sessionUseCases.reload.invoke(currentSession, flags = flags)
            }
            ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading.invoke(currentSession)
            ToolbarMenu.Item.Settings -> browserAnimator.captureEngineViewAndDrawStatically {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
                navController.nav(R.id.browserFragment, directions)
            }
            ToolbarMenu.Item.SyncedTabs -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionBrowserFragmentToSyncedTabsFragment()
                )
            }
            is ToolbarMenu.Item.RequestDesktop -> sessionUseCases.requestDesktopSite.invoke(
                item.isChecked,
                currentSession
            )
            ToolbarMenu.Item.AddToTopSites -> {
                scope.launch {
                    ioScope.launch {
                        currentSession?.let {
                            with(activity.components.useCases.topSitesUseCase) {
                                addPinnedSites(it.title, it.url)
                            }
                        }
                    }.join()

                    FenixSnackbar.make(
                        view = swipeRefresh,
                        duration = Snackbar.LENGTH_SHORT,
                        isDisplayedWithBrowserToolbar = true
                    )
                        .setText(
                            swipeRefresh.context.getString(R.string.snackbar_added_to_top_sites)
                        )
                        .show()
                }
            }
            ToolbarMenu.Item.AddToHomeScreen, ToolbarMenu.Item.InstallToHomeScreen -> {
                settings.installPwaOpened = true
                MainScope().launch {
                    with(activity.components.useCases.webAppUseCases) {
                        if (isInstallable()) {
                            addToHomescreen()
                        } else {
                            val directions =
                                BrowserFragmentDirections.actionBrowserFragmentToCreateShortcutFragment()
                            navController.navigateSafe(R.id.browserFragment, directions)
                        }
                    }
                }
            }
            ToolbarMenu.Item.Share -> {
                val directions = NavGraphDirections.actionGlobalShareFragment(
                    data = arrayOf(
                        ShareData(
                            url = currentSession?.url,
                            title = currentSession?.title
                        )
                    ),
                    showPage = true
                )
                navController.navigate(directions)
            }

            ToolbarMenu.Item.FindInPage -> {
                findInPageLauncher()
                metrics.track(Event.FindInPageOpened)
            }

            ToolbarMenu.Item.AddonsManager -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalAddonsManagementFragment()
                )
            }
            ToolbarMenu.Item.SaveToCollection -> {
                metrics
                    .track(Event.CollectionSaveButtonPressed(TELEMETRY_BROWSER_IDENTIFIER))

                currentSession?.let { currentSession ->
                    val directions =
                        BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
                            tabIds = arrayOf(currentSession.id),
                            selectedTabIds = arrayOf(currentSession.id),
                            saveCollectionStep = if (tabCollectionStorage.cachedTabCollections.isEmpty()) {
                                SaveCollectionStep.NameCollection
                            } else {
                                SaveCollectionStep.SelectCollection
                            }
                        )
                    navController.nav(R.id.browserFragment, directions)
                }
            }
            ToolbarMenu.Item.OpenInFenix -> {
                // Stop the SessionFeature from updating the EngineView and let it release the session
                // from the EngineView so that it can immediately be rendered by a different view once
                // we switch to the actual browser.
                sessionFeature.get()?.release()

                // Strip the CustomTabConfig to turn this Session into a regular tab and then select it
                customTabSession!!.customTabConfig = null
                sessionManager.select(customTabSession)

                // Switch to the actual browser which should now display our new selected session
                activity.startActivity(openInFenixIntent)

                // Close this activity since it is no longer displaying any session
                activity.finish()
            }
            ToolbarMenu.Item.Quit -> {
                // We need to show the snackbar while the browsing data is deleting (if "Delete
                // browsing data on quit" is activated). After the deletion is over, the snackbar
                // is dismissed.
                val snackbar: FenixSnackbar? = activity.getRootView()?.let { v ->
                    FenixSnackbar.make(
                        view = v,
                        duration = Snackbar.LENGTH_LONG,
                        isDisplayedWithBrowserToolbar = true
                    )
                        .setText(v.context.getString(R.string.deleting_browsing_data_in_progress))
                }

                deleteAndQuit(activity, scope, snackbar)
            }
            ToolbarMenu.Item.ReaderModeAppearance -> {
                readerModeController.showControls()
                metrics.track(Event.ReaderModeAppearanceOpened)
            }
            ToolbarMenu.Item.OpenInApp -> {
                settings.openInAppOpened = true

                val appLinksUseCases = activity.components.useCases.appLinksUseCases
                val getRedirect = appLinksUseCases.appLinkRedirect
                currentSession?.let {
                    val redirect = getRedirect.invoke(it.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }
            ToolbarMenu.Item.Bookmark -> {
                sessionManager.selectedSession?.let {
                    bookmarkTapped(it)
                }
            }
            ToolbarMenu.Item.Bookmarks -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
                )
            }
            ToolbarMenu.Item.History -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalHistoryFragment()
                )
            }

            ToolbarMenu.Item.Downloads -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalDownloadsFragment()
                )
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun trackToolbarItemInteraction(item: ToolbarMenu.Item) {
        val eventItem = when (item) {
            is ToolbarMenu.Item.Back -> Event.BrowserMenuItemTapped.Item.BACK
            is ToolbarMenu.Item.Forward -> Event.BrowserMenuItemTapped.Item.FORWARD
            is ToolbarMenu.Item.Reload -> Event.BrowserMenuItemTapped.Item.RELOAD
            ToolbarMenu.Item.Stop -> Event.BrowserMenuItemTapped.Item.STOP
            ToolbarMenu.Item.Settings -> Event.BrowserMenuItemTapped.Item.SETTINGS
            is ToolbarMenu.Item.RequestDesktop ->
                if (item.isChecked) {
                    Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_ON
                } else {
                    Event.BrowserMenuItemTapped.Item.DESKTOP_VIEW_OFF
                }

            ToolbarMenu.Item.FindInPage -> Event.BrowserMenuItemTapped.Item.FIND_IN_PAGE
            ToolbarMenu.Item.OpenInFenix -> Event.BrowserMenuItemTapped.Item.OPEN_IN_FENIX
            ToolbarMenu.Item.Share -> Event.BrowserMenuItemTapped.Item.SHARE
            ToolbarMenu.Item.SaveToCollection -> Event.BrowserMenuItemTapped.Item.SAVE_TO_COLLECTION
            ToolbarMenu.Item.AddToTopSites -> Event.BrowserMenuItemTapped.Item.ADD_TO_TOP_SITES
            ToolbarMenu.Item.AddToHomeScreen -> Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN
            ToolbarMenu.Item.SyncedTabs -> Event.BrowserMenuItemTapped.Item.SYNC_TABS
            ToolbarMenu.Item.InstallToHomeScreen -> Event.BrowserMenuItemTapped.Item.ADD_TO_HOMESCREEN
            ToolbarMenu.Item.Quit -> Event.BrowserMenuItemTapped.Item.QUIT
            ToolbarMenu.Item.ReaderModeAppearance ->
                Event.BrowserMenuItemTapped.Item.READER_MODE_APPEARANCE
            ToolbarMenu.Item.OpenInApp -> Event.BrowserMenuItemTapped.Item.OPEN_IN_APP
            ToolbarMenu.Item.Bookmark -> Event.BrowserMenuItemTapped.Item.BOOKMARK
            ToolbarMenu.Item.AddonsManager -> Event.BrowserMenuItemTapped.Item.ADDONS_MANAGER
            ToolbarMenu.Item.Bookmarks -> Event.BrowserMenuItemTapped.Item.BOOKMARKS
            ToolbarMenu.Item.History -> Event.BrowserMenuItemTapped.Item.HISTORY
            ToolbarMenu.Item.Downloads -> Event.BrowserMenuItemTapped.Item.DOWNLOADS
        }

        metrics.track(Event.BrowserMenuItemTapped(eventItem))
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}

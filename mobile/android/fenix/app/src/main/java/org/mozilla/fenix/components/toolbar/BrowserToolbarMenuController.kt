/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.navigation.NavController
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.top.sites.DefaultTopSitesStorage
import mozilla.components.feature.top.sites.PinnedSiteStorage
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.ReaderMode
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.utils.Do
import org.mozilla.fenix.utils.Settings

/**
 * An interface that handles events from the BrowserToolbar menu, triggered by the Interactor
 */
interface BrowserToolbarMenuController {
    fun handleToolbarItemInteraction(item: ToolbarMenu.Item)
}

@Suppress("LargeClass", "ForbiddenComment", "LongParameterList")
class DefaultBrowserToolbarMenuController(
    private val store: BrowserStore,
    private val activity: HomeActivity,
    private val navController: NavController,
    private val settings: Settings,
    private val readerModeController: ReaderModeController,
    private val sessionFeature: ViewBoundFeatureWrapper<SessionFeature>,
    private val findInPageLauncher: () -> Unit,
    private val browserAnimator: BrowserAnimator,
    private val snackbarParent: ViewGroup,
    private val customTabSessionId: String?,
    private val openInFenixIntent: Intent,
    private val bookmarkTapped: (String, String) -> Unit,
    private val scope: CoroutineScope,
    private val tabCollectionStorage: TabCollectionStorage,
    private val topSitesStorage: DefaultTopSitesStorage,
    private val pinnedSiteStorage: PinnedSiteStorage,
    private val browserStore: BrowserStore,
) : BrowserToolbarMenuController {

    private val currentSession
        get() = store.state.findCustomTabOrSelectedTab(customTabSessionId)

    // We hold onto a reference of the inner scope so that we can override this with the
    // TestCoroutineScope to ensure sequential execution. If we didn't have this, our tests
    // would fail intermittently due to the async nature of coroutine scheduling.
    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    @Suppress("ComplexMethod", "LongMethod")
    override fun handleToolbarItemInteraction(item: ToolbarMenu.Item) {
        val sessionUseCases = activity.components.useCases.sessionUseCases
        val customTabUseCases = activity.components.useCases.customTabsUseCases
        trackToolbarItemInteraction(item)

        Do exhaustive when (item) {
            // TODO: These can be removed for https://github.com/mozilla-mobile/fenix/issues/17870
            // todo === Start ===
            is ToolbarMenu.Item.InstallPwaToHomeScreen -> {
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
            is ToolbarMenu.Item.OpenInFenix -> {
                customTabSessionId?.let {
                    // Stop the SessionFeature from updating the EngineView and let it release the session
                    // from the EngineView so that it can immediately be rendered by a different view once
                    // we switch to the actual browser.
                    sessionFeature.get()?.release()

                    // Turn this Session into a regular tab and then select it
                    customTabUseCases.migrate(customTabSessionId, select = true)

                    // Switch to the actual browser which should now display our new selected session
                    activity.startActivity(
                        openInFenixIntent.apply {
                            // We never want to launch the browser in the same task as the external app
                            // activity. So we force a new task here. IntentReceiverActivity will do the
                            // right thing and take care of routing to an already existing browser and avoid
                            // cloning a new one.
                            flags = flags or Intent.FLAG_ACTIVITY_NEW_TASK
                        },
                    )

                    // Close this activity (and the task) since it is no longer displaying any session
                    activity.finishAndRemoveTask()
                }
            }
            // todo === End ===
            is ToolbarMenu.Item.OpenInApp -> {
                settings.openInAppOpened = true

                val appLinksUseCases = activity.components.useCases.appLinksUseCases
                val getRedirect = appLinksUseCases.appLinkRedirect
                currentSession?.let {
                    val redirect = getRedirect.invoke(it.content.url)
                    redirect.appIntent?.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                    appLinksUseCases.openAppLink.invoke(redirect.appIntent)
                }
            }
            is ToolbarMenu.Item.Quit -> {
                // We need to show the snackbar while the browsing data is deleting (if "Delete
                // browsing data on quit" is activated). After the deletion is over, the snackbar
                // is dismissed.
                val snackbar: FenixSnackbar? = activity.getRootView()?.let { v ->
                    FenixSnackbar.make(
                        view = v,
                        duration = Snackbar.LENGTH_LONG,
                        isDisplayedWithBrowserToolbar = true,
                    )
                        .setText(v.context.getString(R.string.deleting_browsing_data_in_progress))
                }

                deleteAndQuit(activity, scope, snackbar)
            }
            is ToolbarMenu.Item.CustomizeReaderView -> {
                readerModeController.showControls()
                ReaderMode.appearance.record(NoExtras())
            }
            is ToolbarMenu.Item.Back -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(
                            activeSessionId = customTabSessionId,
                        ),
                    )
                } else {
                    currentSession?.let {
                        sessionUseCases.goBack.invoke(it.id)
                    }
                }
            }
            is ToolbarMenu.Item.Forward -> {
                if (item.viewHistory) {
                    navController.navigate(
                        BrowserFragmentDirections.actionGlobalTabHistoryDialogFragment(
                            activeSessionId = customTabSessionId,
                        ),
                    )
                } else {
                    currentSession?.let {
                        sessionUseCases.goForward.invoke(it.id)
                    }
                }
            }
            is ToolbarMenu.Item.Reload -> {
                val flags = if (item.bypassCache) {
                    LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE)
                } else {
                    LoadUrlFlags.none()
                }

                currentSession?.let {
                    sessionUseCases.reload.invoke(it.id, flags = flags)
                }
            }
            is ToolbarMenu.Item.Stop -> {
                currentSession?.let {
                    sessionUseCases.stopLoading.invoke(it.id)
                }
            }
            is ToolbarMenu.Item.Share -> {
                val directions = NavGraphDirections.actionGlobalShareFragment(
                    sessionId = currentSession?.id,
                    data = arrayOf(
                        ShareData(
                            url = getProperUrl(currentSession),
                            title = currentSession?.content?.title,
                        ),
                    ),
                    showPage = true,
                )
                navController.navigate(directions)
            }
            is ToolbarMenu.Item.Settings -> browserAnimator.captureEngineViewAndDrawStatically {
                val directions = BrowserFragmentDirections.actionBrowserFragmentToSettingsFragment()
                navController.nav(R.id.browserFragment, directions)
            }
            is ToolbarMenu.Item.SyncAccount -> {
                val directions = when (item.accountState) {
                    AccountState.AUTHENTICATED ->
                        BrowserFragmentDirections.actionGlobalAccountSettingsFragment()
                    AccountState.NEEDS_REAUTHENTICATION ->
                        BrowserFragmentDirections.actionGlobalAccountProblemFragment(
                            entrypoint = FenixFxAEntryPoint.BrowserToolbar,
                        )
                    AccountState.NO_ACCOUNT ->
                        BrowserFragmentDirections.actionGlobalTurnOnSync(entrypoint = FenixFxAEntryPoint.BrowserToolbar)
                }
                browserAnimator.captureEngineViewAndDrawStatically {
                    navController.nav(
                        R.id.browserFragment,
                        directions,
                    )
                }
            }
            is ToolbarMenu.Item.RequestDesktop -> {
                currentSession?.let {
                    sessionUseCases.requestDesktopSite.invoke(
                        item.isChecked,
                        it.id,
                    )
                }
            }
            is ToolbarMenu.Item.AddToTopSites -> {
                scope.launch {
                    val context = snackbarParent.context
                    val numPinnedSites = topSitesStorage.cachedTopSites
                        .filter { it is TopSite.Default || it is TopSite.Pinned }.size

                    if (numPinnedSites >= settings.topSitesMaxLimit) {
                        AlertDialog.Builder(snackbarParent.context).apply {
                            setTitle(R.string.shortcut_max_limit_title)
                            setMessage(R.string.shortcut_max_limit_content)
                            setPositiveButton(R.string.top_sites_max_limit_confirmation_button) { dialog, _ ->
                                dialog.dismiss()
                            }
                            create().withCenterAlignedButtons()
                        }.show()
                    } else {
                        ioScope.launch {
                            currentSession?.let {
                                with(activity.components.useCases.topSitesUseCase) {
                                    addPinnedSites(it.content.title, it.content.url)
                                }
                            }
                        }.join()

                        FenixSnackbar.make(
                            view = snackbarParent,
                            duration = Snackbar.LENGTH_SHORT,
                            isDisplayedWithBrowserToolbar = true,
                        )
                            .setText(
                                context.getString(R.string.snackbar_added_to_shortcuts),
                            )
                            .show()
                    }
                }
            }
            is ToolbarMenu.Item.AddToHomeScreen -> {
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
            is ToolbarMenu.Item.FindInPage -> {
                findInPageLauncher()
            }
            is ToolbarMenu.Item.AddonsManager -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalAddonsManagementFragment(),
                )
            }
            is ToolbarMenu.Item.SaveToCollection -> {
                Collections.saveButton.record(
                    Collections.SaveButtonExtra(
                        TELEMETRY_BROWSER_IDENTIFIER,
                    ),
                )

                currentSession?.let { currentSession ->
                    val directions =
                        BrowserFragmentDirections.actionGlobalCollectionCreationFragment(
                            tabIds = arrayOf(currentSession.id),
                            selectedTabIds = arrayOf(currentSession.id),
                            saveCollectionStep = if (tabCollectionStorage.cachedTabCollections.isEmpty()) {
                                SaveCollectionStep.NameCollection
                            } else {
                                SaveCollectionStep.SelectCollection
                            },
                        )
                    navController.nav(R.id.browserFragment, directions)
                }
            }
            is ToolbarMenu.Item.Bookmark -> {
                store.state.selectedTab?.let {
                    getProperUrl(it)?.let { url -> bookmarkTapped(url, it.content.title) }
                }
            }
            is ToolbarMenu.Item.Bookmarks -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                )
            }
            is ToolbarMenu.Item.History -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalHistoryFragment(),
                )
            }

            is ToolbarMenu.Item.Downloads -> browserAnimator.captureEngineViewAndDrawStatically {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalDownloadsFragment(),
                )
            }
            is ToolbarMenu.Item.NewTab -> {
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
                )
            }
            is ToolbarMenu.Item.SetDefaultBrowser -> {
                activity.openSetDefaultBrowserOption()
            }
            is ToolbarMenu.Item.RemoveFromTopSites -> {
                scope.launch {
                    val removedTopSite: TopSite? =
                        pinnedSiteStorage
                            .getPinnedSites()
                            .find { it.url == currentSession?.content?.url }
                    if (removedTopSite != null) {
                        ioScope.launch {
                            currentSession?.let {
                                with(activity.components.useCases.topSitesUseCase) {
                                    removeTopSites(removedTopSite)
                                }
                            }
                        }.join()
                    }

                    FenixSnackbar.make(
                        view = snackbarParent,
                        duration = Snackbar.LENGTH_SHORT,
                        isDisplayedWithBrowserToolbar = true,
                    )
                        .setText(
                            snackbarParent.context.getString(R.string.snackbar_top_site_removed),
                        )
                        .show()
                }
            }
        }
    }

    private fun getProperUrl(currentSession: SessionState?): String? {
        return currentSession?.id?.let {
            val currentTab = browserStore.state.findTab(it)
            if (currentTab?.readerState?.active == true) {
                currentTab.readerState.activeUrl
            } else {
                currentSession.content.url
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun trackToolbarItemInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.OpenInFenix ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("open_in_fenix"))
            is ToolbarMenu.Item.InstallPwaToHomeScreen ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_homescreen"))
            is ToolbarMenu.Item.Quit ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("quit"))
            is ToolbarMenu.Item.OpenInApp ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("open_in_app"))
            is ToolbarMenu.Item.CustomizeReaderView ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("reader_mode_appearance"))
            is ToolbarMenu.Item.Back ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("back"))
            is ToolbarMenu.Item.Forward ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("forward"))
            is ToolbarMenu.Item.Reload ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("reload"))
            is ToolbarMenu.Item.Stop ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("stop"))
            is ToolbarMenu.Item.Share ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("share"))
            is ToolbarMenu.Item.Settings ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("settings"))
            is ToolbarMenu.Item.RequestDesktop ->
                if (item.isChecked) {
                    Events.browserMenuAction.record(Events.BrowserMenuActionExtra("desktop_view_on"))
                } else {
                    Events.browserMenuAction.record(Events.BrowserMenuActionExtra("desktop_view_off"))
                }
            is ToolbarMenu.Item.FindInPage ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("find_in_page"))
            is ToolbarMenu.Item.SaveToCollection ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("save_to_collection"))
            is ToolbarMenu.Item.AddToTopSites ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_top_sites"))
            is ToolbarMenu.Item.AddToHomeScreen ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("add_to_homescreen"))
            is ToolbarMenu.Item.SyncAccount ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("sync_account"))
            is ToolbarMenu.Item.Bookmark ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("bookmark"))
            is ToolbarMenu.Item.AddonsManager ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("addons_manager"))
            is ToolbarMenu.Item.Bookmarks ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("bookmarks"))
            is ToolbarMenu.Item.History ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("history"))
            is ToolbarMenu.Item.Downloads ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("downloads"))
            is ToolbarMenu.Item.NewTab ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("new_tab"))
            is ToolbarMenu.Item.SetDefaultBrowser ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("set_default_browser"))
            is ToolbarMenu.Item.RemoveFromTopSites ->
                Events.browserMenuAction.record(Events.BrowserMenuActionExtra("remove_from_top_sites"))
        }
    }

    companion object {
        internal const val TELEMETRY_BROWSER_IDENTIFIER = "browserMenu"
    }
}

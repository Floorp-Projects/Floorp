/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.widget.EditText
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.state.availableSearchEngines
import mozilla.components.browser.state.state.searchEngines
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.ext.invoke
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.support.ktx.kotlin.isUrl
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.GleanMetrics.RecentBookmarks
import org.mozilla.fenix.GleanMetrics.RecentTabs
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.metrics.MetricsUtils
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.gleanplumb.Message
import org.mozilla.fenix.gleanplumb.MessageController
import org.mozilla.fenix.home.HomeFragment
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.onboarding.WallpaperOnboardingDialogFragment.Companion.THUMBNAILS_SELECTION_COUNT
import org.mozilla.fenix.search.toolbar.SearchSelectorInteractor
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpaperState
import mozilla.components.feature.tab.collections.Tab as ComponentTab

/**
 * [HomeFragment] controller. An interface that handles the view manipulation of the Tabs triggered
 * by the Interactor.
 */
@Suppress("TooManyFunctions")
interface SessionControlController {
    /**
     * @see [CollectionInteractor.onCollectionAddTabTapped]
     */
    fun handleCollectionAddTabTapped(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onCollectionOpenTabClicked]
     */
    fun handleCollectionOpenTabClicked(tab: ComponentTab)

    /**
     * @see [CollectionInteractor.onCollectionOpenTabsTapped]
     */
    fun handleCollectionOpenTabsTapped(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onCollectionRemoveTab]
     */
    fun handleCollectionRemoveTab(collection: TabCollection, tab: ComponentTab, wasSwiped: Boolean)

    /**
     * @see [CollectionInteractor.onCollectionShareTabsClicked]
     */
    fun handleCollectionShareTabsClicked(collection: TabCollection)

    /**
     * @see [CollectionInteractor.onDeleteCollectionTapped]
     */
    fun handleDeleteCollectionTapped(collection: TabCollection)

    /**
     * @see [TopSiteInteractor.onOpenInPrivateTabClicked]
     */
    fun handleOpenInPrivateTabClicked(topSite: TopSite)

    /**
     * @see [TabSessionInteractor.onPrivateBrowsingLearnMoreClicked]
     */
    fun handlePrivateBrowsingLearnMoreClicked()

    /**
     * @see [TopSiteInteractor.onRenameTopSiteClicked]
     */
    fun handleRenameTopSiteClicked(topSite: TopSite)

    /**
     * @see [TopSiteInteractor.onRemoveTopSiteClicked]
     */
    fun handleRemoveTopSiteClicked(topSite: TopSite)

    /**
     * @see [CollectionInteractor.onRenameCollectionTapped]
     */
    fun handleRenameCollectionTapped(collection: TabCollection)

    /**
     * @see [TopSiteInteractor.onSelectTopSite]
     */
    fun handleSelectTopSite(topSite: TopSite, position: Int)

    /**
     * @see [TopSiteInteractor.onSettingsClicked]
     */
    fun handleTopSiteSettingsClicked()

    /**
     * @see [TopSiteInteractor.onSponsorPrivacyClicked]
     */
    fun handleSponsorPrivacyClicked()

    /**
     * @see [CollectionInteractor.onToggleCollectionExpanded]
     */
    fun handleToggleCollectionExpanded(collection: TabCollection, expand: Boolean)

    /**
     * @see [ToolbarInteractor.onPasteAndGo]
     */
    fun handlePasteAndGo(clipboardText: String)

    /**
     * @see [ToolbarInteractor.onPaste]
     */
    fun handlePaste(clipboardText: String)

    /**
     * @see [ToolbarInteractor.onNavigateSearch]
     */
    fun handleNavigateSearch()

    /**
     * @see [CollectionInteractor.onAddTabsToCollectionTapped]
     */
    fun handleCreateCollection()

    /**
     * @see [CollectionInteractor.onRemoveCollectionsPlaceholder]
     */
    fun handleRemoveCollectionsPlaceholder()

    /**
     * @see [MessageCardInteractor.onMessageClicked]
     */
    fun handleMessageClicked(message: Message)

    /**
     * @see [MessageCardInteractor.onMessageClosedClicked]
     */
    fun handleMessageClosed(message: Message)

    /**
     * @see [TabSessionInteractor.onPrivateModeButtonClicked]
     */
    fun handlePrivateModeButtonClicked(newMode: BrowsingMode, userHasBeenOnboarded: Boolean)

    /**
     * @see [CustomizeHomeIteractor.openCustomizeHomePage]
     */
    fun handleCustomizeHomeTapped()

    /**
     * @see [WallpaperInteractor.showWallpapersOnboardingDialog]
     */
    fun handleShowWallpapersOnboardingDialog(state: WallpaperState): Boolean

    /**
     * @see [SessionControlInteractor.reportSessionMetrics]
     */
    fun handleReportSessionMetrics(state: AppState)

    /**
     * @see [SearchSelectorInteractor.onMenuItemTapped]
     */
    fun handleMenuItemTapped(item: SearchSelectorMenu.Item)
}

@Suppress("TooManyFunctions", "LargeClass", "LongParameterList")
class DefaultSessionControlController(
    private val activity: HomeActivity,
    private val settings: Settings,
    private val engine: Engine,
    private val messageController: MessageController,
    private val store: BrowserStore,
    private val tabCollectionStorage: TabCollectionStorage,
    private val addTabUseCase: TabsUseCases.AddNewTabUseCase,
    private val restoreUseCase: TabsUseCases.RestoreUseCase,
    private val reloadUrlUseCase: SessionUseCases.ReloadUrlUseCase,
    private val selectTabUseCase: TabsUseCases.SelectTabUseCase,
    private val appStore: AppStore,
    private val navController: NavController,
    private val viewLifecycleScope: CoroutineScope,
    private val registerCollectionStorageObserver: () -> Unit,
    private val removeCollectionWithUndo: (tabCollection: TabCollection) -> Unit,
    private val showTabTray: () -> Unit,
) : SessionControlController {

    override fun handleCollectionAddTabTapped(collection: TabCollection) {
        Collections.addTabButton.record(NoExtras())
        showCollectionCreationFragment(
            step = SaveCollectionStep.SelectTabs,
            selectedTabCollectionId = collection.id,
        )
    }

    override fun handleCollectionOpenTabClicked(tab: ComponentTab) {
        restoreUseCase.invoke(
            activity,
            engine,
            tab,
            onTabRestored = {
                activity.openToBrowser(BrowserDirection.FromHome)
                selectTabUseCase.invoke(it)
                reloadUrlUseCase.invoke(it)
            },
            onFailure = {
                activity.openToBrowserAndLoad(
                    searchTermOrURL = tab.url,
                    newTab = true,
                    from = BrowserDirection.FromHome,
                )
            },
        )

        Collections.tabRestored.record(NoExtras())
    }

    override fun handleCollectionOpenTabsTapped(collection: TabCollection) {
        restoreUseCase.invoke(
            activity,
            engine,
            collection,
            onFailure = { url ->
                addTabUseCase.invoke(url)
            },
        )

        showTabTray()
        Collections.allTabsRestored.record(NoExtras())
    }

    override fun handleCollectionRemoveTab(
        collection: TabCollection,
        tab: ComponentTab,
        wasSwiped: Boolean,
    ) {
        Collections.tabRemoved.record(NoExtras())

        if (collection.tabs.size == 1) {
            removeCollectionWithUndo(collection)
        } else {
            viewLifecycleScope.launch {
                tabCollectionStorage.removeTabFromCollection(collection, tab)
            }
        }
    }

    override fun handleCollectionShareTabsClicked(collection: TabCollection) {
        showShareFragment(
            collection.title,
            collection.tabs.map { ShareData(url = it.url, title = it.title) },
        )
        Collections.shared.record(NoExtras())
    }

    override fun handleDeleteCollectionTapped(collection: TabCollection) {
        removeCollectionWithUndo(collection)
        Collections.removed.record(NoExtras())
    }

    override fun handleOpenInPrivateTabClicked(topSite: TopSite) {
        if (topSite is TopSite.Provided) {
            TopSites.openContileInPrivateTab.record(NoExtras())
        } else {
            TopSites.openInPrivateTab.record(NoExtras())
        }
        with(activity) {
            browsingModeManager.mode = BrowsingMode.Private
            openToBrowserAndLoad(
                searchTermOrURL = topSite.url,
                newTab = true,
                from = BrowserDirection.FromHome,
            )
        }
    }

    override fun handlePrivateBrowsingLearnMoreClicked() {
        activity.openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(PRIVATE_BROWSING_MYTHS),
            newTab = true,
            from = BrowserDirection.FromHome,
        )
    }

    @SuppressLint("InflateParams")
    override fun handleRenameTopSiteClicked(topSite: TopSite) {
        activity.let {
            val customLayout =
                LayoutInflater.from(it).inflate(R.layout.top_sites_rename_dialog, null)
            val topSiteLabelEditText: EditText =
                customLayout.findViewById(R.id.top_site_title)
            topSiteLabelEditText.setText(topSite.title)

            AlertDialog.Builder(it).apply {
                setTitle(R.string.rename_top_site)
                setView(customLayout)
                setPositiveButton(R.string.top_sites_rename_dialog_ok) { dialog, _ ->
                    viewLifecycleScope.launch(Dispatchers.IO) {
                        with(activity.components.useCases.topSitesUseCase) {
                            updateTopSites(
                                topSite,
                                topSiteLabelEditText.text.toString(),
                                topSite.url,
                            )
                        }
                    }
                    dialog.dismiss()
                }
                setNegativeButton(R.string.top_sites_rename_dialog_cancel) { dialog, _ ->
                    dialog.cancel()
                }
            }.show().also {
                topSiteLabelEditText.setSelection(0, topSiteLabelEditText.text.length)
                topSiteLabelEditText.showKeyboard()
            }
        }
    }

    override fun handleRemoveTopSiteClicked(topSite: TopSite) {
        TopSites.remove.record(NoExtras())
        when (topSite.url) {
            SupportUtils.POCKET_TRENDING_URL -> Pocket.pocketTopSiteRemoved.record(NoExtras())
            SupportUtils.GOOGLE_URL -> TopSites.googleTopSiteRemoved.record(NoExtras())
            SupportUtils.BAIDU_URL -> TopSites.baiduTopSiteRemoved.record(NoExtras())
        }

        viewLifecycleScope.launch(Dispatchers.IO) {
            with(activity.components.useCases.topSitesUseCase) {
                removeTopSites(topSite)
            }
        }
    }

    override fun handleRenameCollectionTapped(collection: TabCollection) {
        showCollectionCreationFragment(
            step = SaveCollectionStep.RenameCollection,
            selectedTabCollectionId = collection.id,
        )
        Collections.renameButton.record(NoExtras())
    }

    override fun handleSelectTopSite(topSite: TopSite, position: Int) {
        TopSites.openInNewTab.record(NoExtras())

        when (topSite) {
            is TopSite.Default -> TopSites.openDefault.record(NoExtras())
            is TopSite.Frecent -> TopSites.openFrecency.record(NoExtras())
            is TopSite.Pinned -> TopSites.openPinned.record(NoExtras())
            is TopSite.Provided -> TopSites.openContileTopSite.record(NoExtras()).also {
                submitTopSitesImpressionPing(topSite, position)
            }
        }

        when (topSite.url) {
            SupportUtils.GOOGLE_URL -> TopSites.openGoogleSearchAttribution.record(NoExtras())
            SupportUtils.BAIDU_URL -> TopSites.openBaiduSearchAttribution.record(NoExtras())
            SupportUtils.POCKET_TRENDING_URL -> Pocket.pocketTopSiteClicked.record(NoExtras())
        }

        val availableEngines: List<SearchEngine> = getAvailableSearchEngines()
        val searchAccessPoint = MetricsUtils.Source.TOPSITE

        availableEngines.firstOrNull { engine ->
            engine.resultUrls.firstOrNull { it.contains(topSite.url) } != null
        }?.let { searchEngine ->
            MetricsUtils.recordSearchMetrics(
                searchEngine,
                searchEngine == store.state.search.selectedOrDefaultSearchEngine,
                searchAccessPoint,
            )
        }

        val tabId = addTabUseCase.invoke(
            url = appendSearchAttributionToUrlIfNeeded(topSite.url),
            selectTab = true,
            startLoading = true,
        )

        if (settings.openNextTabInDesktopMode) {
            activity.handleRequestDesktopMode(tabId)
        }
        activity.openToBrowser(BrowserDirection.FromHome)
    }

    @VisibleForTesting
    internal fun submitTopSitesImpressionPing(topSite: TopSite.Provided, position: Int) {
        TopSites.contileClick.record(
            TopSites.ContileClickExtra(
                position = position + 1,
                source = "newtab",
            ),
        )

        topSite.id?.let { TopSites.contileTileId.set(it) }
        topSite.title?.let { TopSites.contileAdvertiser.set(it.lowercase()) }
        TopSites.contileReportingUrl.set(topSite.clickUrl)
        Pings.topsitesImpression.submit()
    }

    override fun handleTopSiteSettingsClicked() {
        TopSites.contileSettings.record(NoExtras())
        navController.nav(
            R.id.homeFragment,
            HomeFragmentDirections.actionGlobalHomeSettingsFragment(),
        )
    }

    override fun handleSponsorPrivacyClicked() {
        TopSites.contileSponsorsAndPrivacy.record(NoExtras())
        activity.openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(SupportUtils.SumoTopic.SPONSOR_PRIVACY),
            newTab = true,
            from = BrowserDirection.FromHome,
        )
    }

    @VisibleForTesting
    internal fun getAvailableSearchEngines() =
        activity.components.core.store.state.search.searchEngines +
            activity.components.core.store.state.search.availableSearchEngines

    /**
     * Append a search attribution query to any provided search engine URL based on the
     * user's current region.
     */
    private fun appendSearchAttributionToUrlIfNeeded(url: String): String {
        if (url == SupportUtils.GOOGLE_URL) {
            store.state.search.region?.let { region ->
                return when (region.current) {
                    "US" -> SupportUtils.GOOGLE_US_URL
                    else -> SupportUtils.GOOGLE_XX_URL
                }
            }
        }

        return url
    }

    override fun handleCustomizeHomeTapped() {
        val directions = HomeFragmentDirections.actionGlobalHomeSettingsFragment()
        navController.nav(navController.currentDestination?.id, directions)
        HomeScreen.customizeHomeClicked.record(NoExtras())
    }

    override fun handleShowWallpapersOnboardingDialog(state: WallpaperState): Boolean {
        return if (activity.browsingModeManager.mode.isPrivate) {
            false
        } else {
            state.availableWallpapers.filter { wallpaper ->
                wallpaper.thumbnailFileState == Wallpaper.ImageFileState.Downloaded
            }.size.let { downloadedCount ->
                // We only display the dialog if enough thumbnails have been downloaded for it.
                downloadedCount >= THUMBNAILS_SELECTION_COUNT
            }.also { showOnboarding ->
                if (showOnboarding) {
                    navController.nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionGlobalWallpaperOnboardingDialog(),
                    )
                }
            }
        }
    }
    override fun handleToggleCollectionExpanded(collection: TabCollection, expand: Boolean) {
        appStore.dispatch(AppAction.CollectionExpanded(collection, expand))
    }

    private fun showTabTrayCollectionCreation() {
        val directions = HomeFragmentDirections.actionGlobalTabsTrayFragment(
            enterMultiselect = true,
        )
        navController.nav(R.id.homeFragment, directions)
    }

    private fun showCollectionCreationFragment(
        step: SaveCollectionStep,
        selectedTabIds: Array<String>? = null,
        selectedTabCollectionId: Long? = null,
    ) {
        if (navController.currentDestination?.id == R.id.collectionCreationFragment) return

        // Only register the observer right before moving to collection creation
        registerCollectionStorageObserver()

        val tabIds = store.state
            .getNormalOrPrivateTabs(private = activity.browsingModeManager.mode.isPrivate)
            .map { session -> session.id }
            .toList()
            .toTypedArray()
        val directions = HomeFragmentDirections.actionGlobalCollectionCreationFragment(
            tabIds = tabIds,
            saveCollectionStep = step,
            selectedTabIds = selectedTabIds,
            selectedTabCollectionId = selectedTabCollectionId ?: -1,
        )
        navController.nav(R.id.homeFragment, directions)
    }

    override fun handleCreateCollection() {
        showTabTrayCollectionCreation()
    }

    override fun handleRemoveCollectionsPlaceholder() {
        settings.showCollectionsPlaceholderOnHome = false
        Collections.placeholderCancel.record()
        appStore.dispatch(AppAction.RemoveCollectionsPlaceholder)
    }

    private fun showShareFragment(shareSubject: String, data: List<ShareData>) {
        val directions = HomeFragmentDirections.actionGlobalShareFragment(
            sessionId = store.state.selectedTabId,
            shareSubject = shareSubject,
            data = data.toTypedArray(),
        )
        navController.nav(R.id.homeFragment, directions)
    }

    override fun handlePasteAndGo(clipboardText: String) {
        val searchEngine = store.state.search.selectedOrDefaultSearchEngine

        activity.openToBrowserAndLoad(
            searchTermOrURL = clipboardText,
            newTab = true,
            from = BrowserDirection.FromHome,
            engine = searchEngine,
        )

        if (clipboardText.isUrl() || searchEngine == null) {
            Events.enteredUrl.record(Events.EnteredUrlExtra(autocomplete = false))
        } else {
            val searchAccessPoint = MetricsUtils.Source.ACTION
            MetricsUtils.recordSearchMetrics(
                searchEngine,
                searchEngine == store.state.search.selectedOrDefaultSearchEngine,
                searchAccessPoint,
            )
        }
    }

    override fun handlePaste(clipboardText: String) {
        val directions = HomeFragmentDirections.actionGlobalSearchDialog(
            sessionId = null,
            pastedText = clipboardText,
        )
        navController.nav(R.id.homeFragment, directions)
    }

    override fun handleNavigateSearch() {
        val directions =
            HomeFragmentDirections.actionGlobalSearchDialog(
                sessionId = null,
            )

        navController.nav(
            R.id.homeFragment,
            directions,
            BrowserAnimator.getToolbarNavOptions(activity),
        )

        Events.searchBarTapped.record(Events.SearchBarTappedExtra("HOME"))
    }

    override fun handleMessageClicked(message: Message) {
        messageController.onMessagePressed(message)
    }

    override fun handleMessageClosed(message: Message) {
        messageController.onMessageDismissed(message)
    }

    override fun handlePrivateModeButtonClicked(
        newMode: BrowsingMode,
        userHasBeenOnboarded: Boolean,
    ) {
        if (newMode == BrowsingMode.Private) {
            activity.settings().incrementNumTimesPrivateModeOpened()
        }

        if (userHasBeenOnboarded) {
            appStore.dispatch(
                AppAction.ModeChange(Mode.fromBrowsingMode(newMode)),
            )

            if (navController.currentDestination?.id == R.id.searchDialogFragment) {
                navController.navigate(
                    BrowserFragmentDirections.actionGlobalSearchDialog(
                        sessionId = null,
                    ),
                )
            }
        }
    }

    override fun handleReportSessionMetrics(state: AppState) {
        if (state.recentTabs.isEmpty()) {
            RecentTabs.sectionVisible.set(false)
        } else {
            RecentTabs.sectionVisible.set(true)
        }

        RecentBookmarks.recentBookmarksCount.set(state.recentBookmarks.size.toLong())
    }

    override fun handleMenuItemTapped(item: SearchSelectorMenu.Item) {
        when (item) {
            SearchSelectorMenu.Item.SearchSettings -> {
                navController.nav(
                    R.id.homeFragment,
                    HomeFragmentDirections.actionGlobalSearchEngineFragment(),
                )
            }
            is SearchSelectorMenu.Item.SearchEngine -> {
                val directions = HomeFragmentDirections.actionGlobalSearchDialog(
                    sessionId = null,
                    searchEngine = item.searchEngine.id,
                )
                navController.nav(
                    R.id.homeFragment,
                    directions,
                    BrowserAnimator.getToolbarNavOptions(activity),
                )
            }
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.annotation.SuppressLint
import android.content.res.ColorStateList
import android.content.res.Configuration
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ButtonDefaults
import androidx.compose.material.Text
import androidx.compose.material.TextButton
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.content.ContextCompat.getColor
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.LinearSmoothScroller
import androidx.recyclerview.widget.RecyclerView.SmoothScroller
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.button.MaterialButton
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopupProperties
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesFeature
import mozilla.components.feature.top.sites.TopSitesFrecencyConfig
import mozilla.components.feature.top.sites.TopSitesProviderConfig
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.Homepage
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingShortcutCfr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.showSnackBar
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.tabstrip.TabStrip
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.toolbar.BottomToolbarContainerView
import org.mozilla.fenix.components.toolbar.IncompleteRedesignToolbarFeature
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.containsQueryParameters
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.scaleToBottomOfView
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.pocket.DefaultPocketStoriesController
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.privatebrowsing.controller.DefaultPrivateBrowsingController
import org.mozilla.fenix.home.recentbookmarks.RecentBookmarksFeature
import org.mozilla.fenix.home.recentbookmarks.controller.DefaultRecentBookmarksController
import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTabFeature
import org.mozilla.fenix.home.recentsyncedtabs.controller.DefaultRecentSyncedTabController
import org.mozilla.fenix.home.recenttabs.RecentTabsListFeature
import org.mozilla.fenix.home.recenttabs.controller.DefaultRecentTabsController
import org.mozilla.fenix.home.recentvisits.RecentVisitsFeature
import org.mozilla.fenix.home.recentvisits.controller.DefaultRecentVisitsController
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.SessionControlView
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionHeaderViewHolder
import org.mozilla.fenix.home.toolbar.DefaultToolbarController
import org.mozilla.fenix.home.toolbar.SearchSelectorBinding
import org.mozilla.fenix.home.toolbar.SearchSelectorMenuBinding
import org.mozilla.fenix.home.topsites.DefaultTopSitesView
import org.mozilla.fenix.messaging.DefaultMessageController
import org.mozilla.fenix.messaging.MessagingFeature
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.perf.MarkersFragmentLifecycleCallbacks
import org.mozilla.fenix.search.toolbar.DefaultSearchSelectorController
import org.mozilla.fenix.search.toolbar.SearchSelectorMenu
import org.mozilla.fenix.tabstray.TabsTrayAccessPoint
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings.Companion.TOP_SITES_PROVIDER_MAX_THRESHOLD
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.wallpapers.Wallpaper
import java.lang.ref.WeakReference

@Suppress("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
    private val args by navArgs<HomeFragmentArgs>()

    @VisibleForTesting
    internal lateinit var bundleArgs: Bundle

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeScreenViewModel by activityViewModels()

    private val snackbarAnchorView: View?
        get() = when (requireContext().settings().toolbarPosition) {
            ToolbarPosition.BOTTOM -> binding.toolbarLayout
            ToolbarPosition.TOP -> null
        }

    private val searchSelectorMenu by lazy {
        SearchSelectorMenu(
            context = requireContext(),
            interactor = sessionControlInteractor,
        )
    }

    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        @SuppressLint("NotifyDataSetChanged")
        override fun onCollectionRenamed(tabCollection: TabCollection, title: String) {
            lifecycleScope.launch(Main) {
                binding.sessionControlRecyclerView.adapter?.notifyDataSetChanged()
            }
            showRenamedSnackbar()
        }

        @SuppressLint("NotifyDataSetChanged")
        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<TabSessionState>) {
            view?.let {
                val message = if (sessions.size == 1) {
                    R.string.create_collection_tab_saved
                } else {
                    R.string.create_collection_tabs_saved
                }

                lifecycleScope.launch(Main) {
                    binding.sessionControlRecyclerView.adapter?.notifyDataSetChanged()
                }

                FenixSnackbar.make(
                    view = it,
                    duration = Snackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = false,
                )
                    .setText(it.context.getString(message))
                    .setAnchorView(snackbarAnchorView)
                    .show()
            }
        }
    }

    private val store: BrowserStore
        get() = requireComponents.core.store

    private var _sessionControlInteractor: SessionControlInteractor? = null
    private val sessionControlInteractor: SessionControlInteractor
        get() = _sessionControlInteractor!!

    private var sessionControlView: SessionControlView? = null
    private var tabCounterView: TabCounterView? = null
    private var toolbarView: ToolbarView? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var homeMenuView: HomeMenuView? = null

    private var lastAppliedWallpaperName: String = Wallpaper.defaultName

    private val topSitesFeature = ViewBoundFeatureWrapper<TopSitesFeature>()
    private val messagingFeature = ViewBoundFeatureWrapper<MessagingFeature>()
    private val recentTabsListFeature = ViewBoundFeatureWrapper<RecentTabsListFeature>()
    private val recentSyncedTabFeature = ViewBoundFeatureWrapper<RecentSyncedTabFeature>()
    private val recentBookmarksFeature = ViewBoundFeatureWrapper<RecentBookmarksFeature>()
    private val historyMetadataFeature = ViewBoundFeatureWrapper<RecentVisitsFeature>()
    private val searchSelectorBinding = ViewBoundFeatureWrapper<SearchSelectorBinding>()
    private val searchSelectorMenuBinding = ViewBoundFeatureWrapper<SearchSelectorMenuBinding>()

    override fun onCreate(savedInstanceState: Bundle?) {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        super.onCreate(savedInstanceState)

        bundleArgs = args.toBundle()

        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME,
            profilerStartTime,
            "HomeFragment.onCreate",
        )
    }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        _binding = FragmentHomeBinding.inflate(inflater, container, false)
        val activity = activity as HomeActivity
        val components = requireComponents

        val currentWallpaperName = requireContext().settings().currentWallpaperName
        applyWallpaper(
            wallpaperName = currentWallpaperName,
            orientationChange = false,
            orientation = requireContext().resources.configuration.orientation,
        )

        components.appStore.dispatch(AppAction.ModeChange(browsingModeManager.mode))

        lifecycleScope.launch(IO) {
            if (requireContext().settings().showPocketRecommendationsFeature) {
                val categories = components.core.pocketStoriesService.getStories()
                    .groupBy { story -> story.category }
                    .map { (category, stories) -> PocketRecommendedStoriesCategory(category, stories) }

                components.appStore.dispatch(AppAction.PocketStoriesCategoriesChange(categories))

                if (requireContext().settings().showPocketSponsoredStories) {
                    components.appStore.dispatch(
                        AppAction.PocketSponsoredStoriesChange(
                            components.core.pocketStoriesService.getSponsoredStories(),
                        ),
                    )
                }
            } else {
                components.appStore.dispatch(AppAction.PocketStoriesClean)
            }
        }

        if (requireContext().settings().isExperimentationEnabled) {
            messagingFeature.set(
                feature = MessagingFeature(
                    appStore = requireComponents.appStore,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }

        if (requireContext().settings().showTopSitesFeature) {
            topSitesFeature.set(
                feature = TopSitesFeature(
                    view = DefaultTopSitesView(
                        appStore = components.appStore,
                        settings = components.settings,
                    ),
                    storage = components.core.topSitesStorage,
                    config = ::getTopSitesConfig,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }

        if (requireContext().settings().showRecentTabsFeature) {
            recentTabsListFeature.set(
                feature = RecentTabsListFeature(
                    browserStore = components.core.store,
                    appStore = components.appStore,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )

            recentSyncedTabFeature.set(
                feature = RecentSyncedTabFeature(
                    context = requireContext(),
                    appStore = requireComponents.appStore,
                    syncStore = requireComponents.backgroundServices.syncStore,
                    storage = requireComponents.backgroundServices.syncedTabsStorage,
                    accountManager = requireComponents.backgroundServices.accountManager,
                    historyStorage = requireComponents.core.historyStorage,
                    coroutineScope = viewLifecycleOwner.lifecycleScope,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }

        if (requireContext().settings().showRecentBookmarksFeature) {
            recentBookmarksFeature.set(
                feature = RecentBookmarksFeature(
                    appStore = components.appStore,
                    bookmarksUseCase = run {
                        requireContext().components.useCases.bookmarksUseCases
                    },
                    scope = viewLifecycleOwner.lifecycleScope,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }

        if (requireContext().settings().historyMetadataUIFeature) {
            historyMetadataFeature.set(
                feature = RecentVisitsFeature(
                    appStore = components.appStore,
                    historyMetadataStorage = components.core.historyStorage,
                    historyHighlightsStorage = components.core.lazyHistoryStorage,
                    scope = viewLifecycleOwner.lifecycleScope,
                ),
                owner = viewLifecycleOwner,
                view = binding.root,
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            controller = DefaultSessionControlController(
                activity = activity,
                settings = components.settings,
                engine = components.core.engine,
                messageController = DefaultMessageController(
                    appStore = components.appStore,
                    messagingController = components.nimbus.messaging,
                    homeActivity = activity,
                ),
                store = store,
                tabCollectionStorage = components.core.tabCollectionStorage,
                addTabUseCase = components.useCases.tabsUseCases.addTab,
                restoreUseCase = components.useCases.tabsUseCases.restore,
                reloadUrlUseCase = components.useCases.sessionUseCases.reload,
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                appStore = components.appStore,
                navController = findNavController(),
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                removeCollectionWithUndo = ::removeCollectionWithUndo,
                showUndoSnackbarForTopSite = ::showUndoSnackbarForTopSite,
                showTabTray = ::openTabsTray,
            ),
            recentTabController = DefaultRecentTabsController(
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                navController = findNavController(),
                appStore = components.appStore,
            ),
            recentSyncedTabController = DefaultRecentSyncedTabController(
                tabsUseCase = requireComponents.useCases.tabsUseCases,
                navController = findNavController(),
                accessPoint = TabsTrayAccessPoint.HomeRecentSyncedTab,
                appStore = components.appStore,
            ),
            recentBookmarksController = DefaultRecentBookmarksController(
                activity = activity,
                navController = findNavController(),
                appStore = components.appStore,
                browserStore = components.core.store,
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
            ),
            recentVisitsController = DefaultRecentVisitsController(
                navController = findNavController(),
                appStore = components.appStore,
                selectOrAddTabUseCase = components.useCases.tabsUseCases.selectOrAddTab,
                storage = components.core.historyStorage,
                scope = viewLifecycleOwner.lifecycleScope,
                store = components.core.store,
            ),
            pocketStoriesController = DefaultPocketStoriesController(
                homeActivity = activity,
                appStore = components.appStore,
            ),
            privateBrowsingController = DefaultPrivateBrowsingController(
                activity = activity,
                appStore = components.appStore,
                navController = findNavController(),
            ),
            searchSelectorController = DefaultSearchSelectorController(
                activity = activity,
                navController = findNavController(),
            ),
            toolbarController = DefaultToolbarController(
                activity = activity,
                store = components.core.store,
                navController = findNavController(),
            ),
        )

        toolbarView = ToolbarView(
            binding = binding,
            context = requireContext(),
            interactor = sessionControlInteractor,
        )

        if (IncompleteRedesignToolbarFeature(requireContext().settings()).isEnabled) {
            val toolbarView = if (requireContext().components.settings.toolbarPosition == ToolbarPosition.BOTTOM) {
                val toolbar = binding.toolbarLayout
                // Should refactor this so there is no added view to remove to begin with
                // https://bugzilla.mozilla.org/show_bug.cgi?id=1870976
                binding.root.removeView(toolbar)
                toolbar
            } else {
                null
            }

            BottomToolbarContainerView(
                context = requireContext(),
                container = binding.homeLayout,
                androidToolbarView = toolbarView,
            )
        }

        sessionControlView = SessionControlView(
            containerView = binding.sessionControlRecyclerView,
            viewLifecycleOwner = viewLifecycleOwner,
            interactor = sessionControlInteractor,
        )

        updateSessionControlView()

        disableAppBarDragging()

        activity.themeManager.applyStatusBarTheme(activity)

        FxNimbus.features.homescreen.recordExposure()

        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME,
            profilerStartTime,
            "HomeFragment.onCreateView",
        )
        return binding.root
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        homeMenuView?.dismissMenu()

        val currentWallpaperName = requireContext().settings().currentWallpaperName
        applyWallpaper(
            wallpaperName = currentWallpaperName,
            orientationChange = true,
            orientation = newConfig.orientation,
        )
    }

    /**
     * Returns a [TopSitesConfig] which specifies how many top sites to display and whether or
     * not frequently visited sites should be displayed.
     */
    @VisibleForTesting
    internal fun getTopSitesConfig(): TopSitesConfig {
        val settings = requireContext().settings()
        return TopSitesConfig(
            totalSites = settings.topSitesMaxLimit,
            frecencyConfig = TopSitesFrecencyConfig(
                FrecencyThresholdOption.SKIP_ONE_TIME_PAGES,
            ) { !Uri.parse(it.url).containsQueryParameters(settings.frecencyFilterQuery) },
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = settings.showContileFeature,
                maxThreshold = TOP_SITES_PROVIDER_MAX_THRESHOLD,
                providerFilter = { topSite ->
                    when (store.state.search.selectedOrDefaultSearchEngine?.name) {
                        AMAZON_SEARCH_ENGINE_NAME -> topSite.title != AMAZON_SPONSORED_TITLE
                        EBAY_SPONSORED_TITLE -> topSite.title != EBAY_SPONSORED_TITLE
                        else -> true
                    }
                },
            ),
        )
    }

    @VisibleForTesting
    internal fun showUndoSnackbarForTopSite(topSite: TopSite) {
        lifecycleScope.allowUndo(
            view = requireView(),
            message = getString(R.string.snackbar_top_site_removed),
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = {
                requireComponents.useCases.topSitesUseCase.addPinnedSites(
                    topSite.title.toString(),
                    topSite.url,
                )
            },
            operation = { },
            elevation = TOAST_ELEVATION,
            paddedForBottomToolbar = true,
        )
    }

    /**
     * The [SessionControlView] is forced to update with our current state when we call
     * [HomeFragment.onCreateView] in order to be able to draw everything at once with the current
     * data in our store. The [View.consumeFrom] coroutine dispatch
     * doesn't get run right away which means that we won't draw on the first layout pass.
     */
    private fun updateSessionControlView() {
        if (browsingModeManager.mode == BrowsingMode.Private) {
            binding.root.consumeFrom(requireContext().components.appStore, viewLifecycleOwner) {
                sessionControlView?.update(it)
            }
        } else {
            sessionControlView?.update(requireContext().components.appStore.state)

            binding.root.consumeFrom(requireContext().components.appStore, viewLifecycleOwner) {
                sessionControlView?.update(it, shouldReportMetrics = true)
            }
        }
    }

    private fun disableAppBarDragging() {
        if (binding.homeAppBar.layoutParams != null) {
            val appBarLayoutParams = binding.homeAppBar.layoutParams as CoordinatorLayout.LayoutParams
            val appBarBehavior = AppBarLayout.Behavior()
            appBarBehavior.setDragCallback(
                object : AppBarLayout.Behavior.DragCallback() {
                    override fun canDrag(appBarLayout: AppBarLayout): Boolean {
                        return false
                    }
                },
            )
            appBarLayoutParams.behavior = appBarBehavior
        }
        binding.homeAppBar.setExpanded(true)
    }

    @Suppress("LongMethod", "ComplexMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        super.onViewCreated(view, savedInstanceState)
        HomeScreen.homeScreenDisplayed.record(NoExtras())
        HomeScreen.homeScreenViewCount.add()
        if (!browsingModeManager.mode.isPrivate) {
            HomeScreen.standardHomepageViewCount.add()
        }

        observeSearchEngineNameChanges()
        observeWallpaperUpdates()

        homeMenuView = HomeMenuView(
            view = view,
            context = view.context,
            lifecycleOwner = viewLifecycleOwner,
            homeActivity = activity as HomeActivity,
            navController = findNavController(),
            menuButton = WeakReference(binding.menuButton),
        ).also { it.build() }

        tabCounterView = TabCounterView(
            context = requireContext(),
            browsingModeManager = browsingModeManager,
            navController = findNavController(),
            tabCounter = binding.tabButton,
        )

        toolbarView?.build()
        if (requireContext().settings().isTabletAndTabStripEnabled) {
            initTabStrip()
        }

        PrivateBrowsingButtonView(binding.privateBrowsingButton, browsingModeManager) { newMode ->
            sessionControlInteractor.onPrivateModeButtonClicked(newMode)
            Homepage.privateModeIconTapped.record(mozilla.telemetry.glean.private.NoExtras())
        }

        consumeFrom(requireComponents.core.store) {
            tabCounterView?.update(it)
            showCollectionsPlaceholder(it)
        }

        homeViewModel.sessionToDelete?.also {
            if (it == ALL_NORMAL_TABS || it == ALL_PRIVATE_TABS) {
                removeAllTabsAndShowSnackbar(it)
            } else {
                removeTabAndShowSnackbar(it)
            }
        }

        homeViewModel.sessionToDelete = null

        tabCounterView?.update(requireComponents.core.store.state)

        if (bundleArgs.getBoolean(FOCUS_ON_ADDRESS_BAR)) {
            sessionControlInteractor.onNavigateSearch()
        } else if (bundleArgs.getBoolean(SCROLL_TO_COLLECTION)) {
            MainScope().launch {
                delay(ANIM_SCROLL_DELAY)
                val smoothScroller: SmoothScroller =
                    object : LinearSmoothScroller(sessionControlView!!.view.context) {
                        override fun getVerticalSnapPreference(): Int {
                            return SNAP_TO_START
                        }
                    }
                val recyclerView = sessionControlView!!.view
                val adapter = recyclerView.adapter!!
                val collectionPosition = IntRange(0, adapter.itemCount - 1).firstOrNull {
                    adapter.getItemViewType(it) == CollectionHeaderViewHolder.LAYOUT_ID
                }
                collectionPosition?.run {
                    val linearLayoutManager = recyclerView.layoutManager as LinearLayoutManager
                    smoothScroller.targetPosition = this
                    linearLayoutManager.startSmoothScroll(smoothScroller)
                }
            }
        }

        searchSelectorBinding.set(
            feature = SearchSelectorBinding(
                context = view.context,
                binding = binding,
                browserStore = requireComponents.core.store,
                searchSelectorMenu = searchSelectorMenu,
            ),
            owner = viewLifecycleOwner,
            view = binding.root,
        )

        searchSelectorMenuBinding.set(
            feature = SearchSelectorMenuBinding(
                context = view.context,
                interactor = sessionControlInteractor,
                searchSelectorMenu = searchSelectorMenu,
                browserStore = requireComponents.core.store,
            ),
            owner = viewLifecycleOwner,
            view = view,
        )

        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME,
            profilerStartTime,
            "HomeFragment.onViewCreated",
        )
    }

    private fun initTabStrip() {
        binding.tabStripView.isVisible = true
        binding.tabStripView.apply {
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
            setContent {
                FirefoxTheme {
                    TabStrip(
                        onHome = true,
                        onAddTabClick = {
                            sessionControlInteractor.onNavigateSearch()
                        },
                        onSelectedTabClick = {
                            (requireActivity() as HomeActivity).openToBrowser(BrowserDirection.FromHome)
                        },
                        onLastTabClose = {},
                    )
                }
            }
        }
    }

    /**
     * Method used to listen to search engine name changes and trigger a top sites update accordingly
     */
    private fun observeSearchEngineNameChanges() {
        consumeFlow(store) { flow ->
            flow.map { state ->
                when (state.search.selectedOrDefaultSearchEngine?.name) {
                    AMAZON_SEARCH_ENGINE_NAME -> AMAZON_SPONSORED_TITLE
                    EBAY_SPONSORED_TITLE -> EBAY_SPONSORED_TITLE
                    else -> null
                }
            }
                .distinctUntilChanged()
                .collect {
                    topSitesFeature.withFeature {
                        it.storage.notifyObservers { onStorageUpdated() }
                    }
                }
        }
    }

    private fun removeAllTabsAndShowSnackbar(sessionCode: String) {
        if (sessionCode == ALL_PRIVATE_TABS) {
            requireComponents.useCases.tabsUseCases.removePrivateTabs()
        } else {
            requireComponents.useCases.tabsUseCases.removeNormalTabs()
        }

        val snackbarMessage = if (sessionCode == ALL_PRIVATE_TABS) {
            if (requireContext().settings().feltPrivateBrowsingEnabled) {
                getString(R.string.snackbar_private_data_deleted)
            } else {
                getString(R.string.snackbar_private_tabs_closed)
            }
        } else {
            getString(R.string.snackbar_tabs_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            requireContext().getString(R.string.snackbar_deleted_undo),
            {
                requireComponents.useCases.tabsUseCases.undo.invoke()
            },
            operation = { },
            anchorView = snackbarAnchorView,
        )
    }

    private fun removeTabAndShowSnackbar(sessionId: String) {
        val tab = store.state.findTab(sessionId) ?: return

        requireComponents.useCases.tabsUseCases.removeTab(sessionId)

        val snackbarMessage = if (tab.content.private) {
            requireContext().getString(R.string.snackbar_private_tab_closed)
        } else {
            requireContext().getString(R.string.snackbar_tab_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            requireContext().getString(R.string.snackbar_deleted_undo),
            {
                requireComponents.useCases.tabsUseCases.undo.invoke()
                findNavController().navigate(
                    HomeFragmentDirections.actionGlobalBrowser(null),
                )
            },
            operation = { },
            anchorView = snackbarAnchorView,
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _sessionControlInteractor = null
        homeMenuView = null
        sessionControlView = null
        tabCounterView = null
        toolbarView = null
        _binding = null

        bundleArgs.clear()
        lastAppliedWallpaperName = Wallpaper.defaultName
    }

    override fun onStop() {
        dismissRecommendPrivateBrowsingShortcut()
        super.onStop()
    }

    override fun onStart() {
        super.onStart()

        subscribeToTabCollections()

        val context = requireContext()

        requireComponents.backgroundServices.accountManagerAvailableQueue.runIfReadyOrQueue {
            // By the time this code runs, we may not be attached to a context or have a view lifecycle owner.
            if ((this@HomeFragment).view?.context == null) {
                return@runIfReadyOrQueue
            }

            requireComponents.backgroundServices.accountManager.register(
                object : AccountObserver {
                    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                        if (authType != AuthType.Existing) {
                            view?.let {
                                FenixSnackbar.make(
                                    view = it,
                                    duration = Snackbar.LENGTH_SHORT,
                                    isDisplayedWithBrowserToolbar = false,
                                )
                                    .setText(it.context.getString(R.string.onboarding_firefox_account_sync_is_on))
                                    .setAnchorView(binding.toolbarLayout)
                                    .show()
                            }
                        }
                    }
                },
                owner = this@HomeFragment.viewLifecycleOwner,
            )
        }

        if (browsingModeManager.mode.isPrivate &&
            // We will be showing the search dialog and don't want to show the CFR while the dialog shows
            !bundleArgs.getBoolean(FOCUS_ON_ADDRESS_BAR) &&
            context.settings().shouldShowPrivateModeCfr
        ) {
            recommendPrivateBrowsingShortcut()
        }

        // We only want this observer live just before we navigate away to the collection creation screen
        requireComponents.core.tabCollectionStorage.unregister(collectionStorageObserver)

        lifecycleScope.launch(IO) {
            requireComponents.reviewPromptController.promptReview(requireActivity())
        }
    }

    @VisibleForTesting
    internal fun removeCollectionWithUndo(tabCollection: TabCollection) {
        val snackbarMessage = getString(R.string.snackbar_collection_deleted)

        lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo),
            {
                requireComponents.core.tabCollectionStorage.createCollection(tabCollection)
            },
            operation = { },
            elevation = TOAST_ELEVATION,
            anchorView = null,
        )

        lifecycleScope.launch(IO) {
            requireComponents.core.tabCollectionStorage.removeCollection(tabCollection)
        }
    }

    override fun onResume() {
        super.onResume()
        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawableResource(R.drawable.private_home_background_gradient)
        }

        hideToolbar()

        // Whenever a tab is selected its last access timestamp is automatically updated by A-C.
        // However, in the case of resuming the app to the home fragment, we already have an
        // existing selected tab, but its last access timestamp is outdated. No action is
        // triggered to cause an automatic update on warm start (no tab selection occurs). So we
        // update it manually here.
        requireComponents.useCases.sessionUseCases.updateLastAccess()
    }

    override fun onPause() {
        super.onPause()
        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawable(
                ColorDrawable(
                    ContextCompat.getColor(
                        requireContext(),
                        R.color.fx_mobile_private_layer_color_1,
                    ),
                ),
            )
        }

        // Counterpart to the update in onResume to keep the last access timestamp of the selected
        // tab up-to-date.
        requireComponents.useCases.sessionUseCases.updateLastAccess()
    }

    private var recommendPrivateBrowsingCFR: CFRPopup? = null

    @OptIn(ExperimentalComposeUiApi::class)
    @Suppress("LongMethod")
    private fun recommendPrivateBrowsingShortcut() {
        context?.let { context ->
            CFRPopup(
                anchor = binding.privateBrowsingButton,
                properties = CFRPopupProperties(
                    popupWidth = 256.dp,
                    popupAlignment = CFRPopup.PopupAlignment.INDICATOR_CENTERED_IN_ANCHOR,
                    popupBodyColors = listOf(
                        getColor(context, R.color.fx_mobile_layer_color_gradient_end),
                        getColor(context, R.color.fx_mobile_layer_color_gradient_start),
                    ),
                    showDismissButton = false,
                    dismissButtonColor = getColor(context, R.color.fx_mobile_icon_color_oncolor),
                    indicatorDirection = CFRPopup.IndicatorDirection.UP,
                ),
                onDismiss = {
                    PrivateBrowsingShortcutCfr.cancel.record()
                    context.settings().showedPrivateModeContextualFeatureRecommender = true
                    context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                    dismissRecommendPrivateBrowsingShortcut()
                },
                text = {
                    FirefoxTheme {
                        Text(
                            text = context.getString(R.string.private_mode_cfr_message_2),
                            color = FirefoxTheme.colors.textOnColorPrimary,
                            style = FirefoxTheme.typography.headline7,
                            modifier = Modifier
                                .semantics {
                                    testTagsAsResourceId = true
                                    testTag = "private.message"
                                },
                        )
                    }
                },
                action = {
                    FirefoxTheme {
                        TextButton(
                            onClick = {
                                PrivateBrowsingShortcutCfr.addShortcut.record(NoExtras())
                                PrivateShortcutCreateManager.createPrivateShortcut(context)
                                context.settings().showedPrivateModeContextualFeatureRecommender = true
                                context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                                dismissRecommendPrivateBrowsingShortcut()
                            },
                            colors = ButtonDefaults.buttonColors(backgroundColor = PhotonColors.LightGrey30),
                            shape = RoundedCornerShape(8.dp),
                            modifier = Modifier
                                .padding(top = 16.dp)
                                .heightIn(36.dp)
                                .fillMaxWidth()
                                .semantics {
                                    testTagsAsResourceId = true
                                    testTag = "private.add"
                                },
                        ) {
                            Text(
                                text = context.getString(R.string.private_mode_cfr_pos_button_text),
                                color = PhotonColors.DarkGrey50,
                                style = FirefoxTheme.typography.headline7,
                                textAlign = TextAlign.Center,
                            )
                        }
                        TextButton(
                            onClick = {
                                PrivateBrowsingShortcutCfr.cancel.record()
                                context.settings().showedPrivateModeContextualFeatureRecommender = true
                                context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                                dismissRecommendPrivateBrowsingShortcut()
                            },
                            modifier = Modifier
                                .heightIn(36.dp)
                                .fillMaxWidth()
                                .semantics {
                                    testTagsAsResourceId = true
                                    testTag = "private.cancel"
                                },
                        ) {
                            Text(
                                text = context.getString(R.string.cfr_neg_button_text),
                                textAlign = TextAlign.Center,
                                color = FirefoxTheme.colors.textOnColorPrimary,
                                style = FirefoxTheme.typography.headline7,
                            )
                        }
                    }
                },
            ).run {
                recommendPrivateBrowsingCFR = this
                show()
            }
        }
    }

    private fun dismissRecommendPrivateBrowsingShortcut() {
        recommendPrivateBrowsingCFR?.dismiss()
        recommendPrivateBrowsingCFR = null
    }

    private fun subscribeToTabCollections(): Observer<List<TabCollection>> {
        return Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
            requireComponents.appStore.dispatch(AppAction.CollectionsChange(it))
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections().observe(this, observer)
        }
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun showRenamedSnackbar() {
        view?.let { view ->
            val string = view.context.getString(R.string.snackbar_collection_renamed)
            FenixSnackbar.make(
                view = view,
                duration = Snackbar.LENGTH_LONG,
                isDisplayedWithBrowserToolbar = false,
            )
                .setText(string)
                .setAnchorView(snackbarAnchorView)
                .show()
        }
    }

    private fun openTabsTray() {
        findNavController().nav(
            R.id.homeFragment,
            HomeFragmentDirections.actionGlobalTabsTrayFragment(),
        )
    }

    private fun showCollectionsPlaceholder(browserState: BrowserState) {
        val tabCount = if (browsingModeManager.mode.isPrivate) {
            browserState.privateTabs.size
        } else {
            browserState.normalTabs.size
        }

        // The add_tabs_to_collections_button is added at runtime. We need to search for it in the same way.
        sessionControlView?.view?.findViewById<MaterialButton>(R.id.add_tabs_to_collections_button)
            ?.isVisible = tabCount > 0
    }

    @VisibleForTesting
    internal fun shouldEnableWallpaper() =
        (activity as? HomeActivity)?.themeManager?.currentTheme?.isPrivate?.not() ?: false

    private fun applyWallpaper(wallpaperName: String, orientationChange: Boolean, orientation: Int) {
        when {
            !shouldEnableWallpaper() ||
                (wallpaperName == lastAppliedWallpaperName && !orientationChange) -> return
            Wallpaper.nameIsDefault(wallpaperName) -> {
                binding.wallpaperImageView.isVisible = false
                lastAppliedWallpaperName = wallpaperName
            }
            else -> {
                viewLifecycleOwner.lifecycleScope.launch {
                    // loadBitmap does file lookups based on name, so we don't need a fully
                    // qualified type to load the image
                    val wallpaper = Wallpaper.Default.copy(name = wallpaperName)
                    val wallpaperImage = requireComponents.useCases.wallpaperUseCases.loadBitmap(wallpaper, orientation)
                    wallpaperImage?.let {
                        it.scaleToBottomOfView(binding.wallpaperImageView)
                        binding.wallpaperImageView.isVisible = true
                        lastAppliedWallpaperName = wallpaperName
                    } ?: run {
                        if (!isActive) return@run
                        with(binding.wallpaperImageView) {
                            isVisible = false
                            showSnackBar(
                                view = this,
                                text = resources.getString(R.string.wallpaper_select_error_snackbar_message),
                            )
                        }
                        // If setting a wallpaper failed reset also the contrasting text color.
                        requireContext().settings().currentWallpaperTextColor = 0L
                        lastAppliedWallpaperName = Wallpaper.defaultName
                    }
                }
            }
        }
        // Logo color should be updated in all cases.
        applyWallpaperTextColor()
    }

    /**
     * Apply a color better contrasting with the current wallpaper to the Fenix logo and private mode switcher.
     */
    @VisibleForTesting
    internal fun applyWallpaperTextColor() {
        val tintColor = when (val color = requireContext().settings().currentWallpaperTextColor.toInt()) {
            0 -> null // a null ColorStateList will clear the current tint
            else -> ColorStateList.valueOf(color)
        }

        binding.wordmarkText.imageTintList = tintColor
        binding.privateBrowsingButton.imageTintList = tintColor
    }

    private fun observeWallpaperUpdates() {
        consumeFlow(requireComponents.appStore, viewLifecycleOwner) { flow ->
            flow.filter { it.mode == BrowsingMode.Normal }
                .map { it.wallpaperState.currentWallpaper }
                .distinctUntilChanged()
                .collect {
                    if (it.name != lastAppliedWallpaperName) {
                        applyWallpaper(
                            wallpaperName = it.name,
                            orientationChange = false,
                            orientation = requireContext().resources.configuration.orientation,
                        )
                    }
                }
        }
    }

    companion object {
        const val ALL_NORMAL_TABS = "all_normal"
        const val ALL_PRIVATE_TABS = "all_private"

        private const val FOCUS_ON_ADDRESS_BAR = "focusOnAddressBar"

        private const val SCROLL_TO_COLLECTION = "scrollToCollection"
        private const val ANIM_SCROLL_DELAY = 100L

        // Sponsored top sites titles and search engine names used for filtering
        const val AMAZON_SPONSORED_TITLE = "Amazon"
        const val AMAZON_SEARCH_ENGINE_NAME = "Amazon.com"
        const val EBAY_SPONSORED_TITLE = "eBay"

        // Elevation for undo toasts
        internal const val TOAST_ELEVATION = 80f
    }
}

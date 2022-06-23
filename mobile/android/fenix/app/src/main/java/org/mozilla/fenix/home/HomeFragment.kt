/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.annotation.SuppressLint
import android.content.res.Configuration
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.os.StrictMode
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewTreeObserver
import android.widget.Button
import android.widget.LinearLayout
import android.widget.PopupWindow
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.constraintlayout.widget.ConstraintSet.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.TOP
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.core.view.doOnPreDraw
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.button.MaterialButton
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.FrecencyThresholdOption
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSitesConfig
import mozilla.components.feature.top.sites.TopSitesFeature
import mozilla.components.feature.top.sites.TopSitesProviderConfig
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.HomeScreen
import org.mozilla.fenix.GleanMetrics.StartOnHome
import org.mozilla.fenix.GleanMetrics.Wallpapers
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.toolbar.FenixTabCounterMenu
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.gleanplumb.DefaultMessageController
import org.mozilla.fenix.gleanplumb.MessagingFeature
import org.mozilla.fenix.home.mozonline.showPrivacyPopWindow
import org.mozilla.fenix.home.pocket.DefaultPocketStoriesController
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
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
import org.mozilla.fenix.home.topsites.DefaultTopSitesView
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.perf.MarkersFragmentLifecycleCallbacks
import org.mozilla.fenix.tabstray.TabsTrayAccessPoint
import org.mozilla.fenix.utils.Settings.Companion.TOP_SITES_PROVIDER_MAX_THRESHOLD
import org.mozilla.fenix.utils.ToolbarPopupWindow
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.wallpapers.WallpaperManager
import java.lang.ref.WeakReference
import kotlin.math.min

@Suppress("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
    private val args by navArgs<HomeFragmentArgs>()
    private lateinit var bundleArgs: Bundle

    private var _binding: FragmentHomeBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeScreenViewModel by activityViewModels()

    private val snackbarAnchorView: View?
        get() = when (requireContext().settings().toolbarPosition) {
            ToolbarPosition.BOTTOM -> binding.toolbarLayout
            ToolbarPosition.TOP -> null
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
    }

    private val store: BrowserStore
        get() = requireComponents.core.store

    private val onboarding by lazy {
        requireComponents.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            FenixOnboarding(requireContext())
        }
    }

    private val syncedTabFeature by lazy {
        RecentSyncedTabFeature(
            store = requireComponents.appStore,
            context = requireContext(),
            storage = requireComponents.backgroundServices.syncedTabsStorage,
            accountManager = requireComponents.backgroundServices.accountManager,
            lifecycleOwner = viewLifecycleOwner,
        )
    }

    private var _sessionControlInteractor: SessionControlInteractor? = null
    private val sessionControlInteractor: SessionControlInteractor
        get() = _sessionControlInteractor!!

    private var sessionControlView: SessionControlView? = null
    private var appBarLayout: AppBarLayout? = null
    private lateinit var currentMode: CurrentMode

    private val topSitesFeature = ViewBoundFeatureWrapper<TopSitesFeature>()
    private val messagingFeature = ViewBoundFeatureWrapper<MessagingFeature>()
    private val recentTabsListFeature = ViewBoundFeatureWrapper<RecentTabsListFeature>()
    private val recentSyncedTabFeature = ViewBoundFeatureWrapper<RecentSyncedTabFeature>()
    private val recentBookmarksFeature = ViewBoundFeatureWrapper<RecentBookmarksFeature>()
    private val historyMetadataFeature = ViewBoundFeatureWrapper<RecentVisitsFeature>()

    @VisibleForTesting
    internal var getMenuButton: () -> MenuButton? = { binding.menuButton }

    override fun onCreate(savedInstanceState: Bundle?) {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        super.onCreate(savedInstanceState)

        bundleArgs = args.toBundle()

        if (!onboarding.userHasBeenOnboarded() &&
            requireContext().settings().shouldShowPrivacyPopWindow &&
            Config.channel.isMozillaOnline
        ) {
            showPrivacyPopWindow(requireContext(), requireActivity())
        }

        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME, profilerStartTime, "HomeFragment.onCreate",
        )
    }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        _binding = FragmentHomeBinding.inflate(inflater, container, false)
        val activity = activity as HomeActivity
        val components = requireComponents

        currentMode = CurrentMode(
            requireContext(),
            onboarding,
            browsingModeManager,
            ::dispatchModeChanges
        )

        components.appStore.dispatch(AppAction.ModeChange(currentMode.getCurrentMode()))

        lifecycleScope.launch(IO) {
            if (requireContext().settings().showPocketRecommendationsFeature) {
                val categories = components.core.pocketStoriesService.getStories()
                    .groupBy { story -> story.category }
                    .map { (category, stories) -> PocketRecommendedStoriesCategory(category, stories) }

                components.appStore.dispatch(AppAction.PocketStoriesCategoriesChange(categories))

                if (requireContext().settings().showPocketSponsoredStories) {
                    components.appStore.dispatch(
                        AppAction.PocketSponsoredStoriesChange(
                            components.core.pocketStoriesService.getSponsoredStories()
                        )
                    )
                }
            } else {
                components.appStore.dispatch(AppAction.PocketStoriesClean)
            }
        }

        if (requireContext().settings().isExperimentationEnabled) {
            messagingFeature.set(
                feature = MessagingFeature(
                    store = requireComponents.appStore,
                ),
                owner = viewLifecycleOwner,
                view = binding.root
            )
        }

        if (requireContext().settings().showTopSitesFeature) {
            topSitesFeature.set(
                feature = TopSitesFeature(
                    view = DefaultTopSitesView(
                        store = components.appStore,
                        settings = components.settings
                    ),
                    storage = components.core.topSitesStorage,
                    config = ::getTopSitesConfig
                ),
                owner = viewLifecycleOwner,
                view = binding.root
            )
        }

        if (requireContext().settings().showRecentTabsFeature) {
            recentTabsListFeature.set(
                feature = RecentTabsListFeature(
                    browserStore = components.core.store,
                    appStore = components.appStore
                ),
                owner = viewLifecycleOwner,
                view = binding.root
            )

            if (requireContext().settings().enableTaskContinuityEnhancements) {
                recentSyncedTabFeature.set(
                    feature = syncedTabFeature,
                    owner = viewLifecycleOwner,
                    view = binding.root
                )
            }
        }

        if (requireContext().settings().showRecentBookmarksFeature) {
            recentBookmarksFeature.set(
                feature = RecentBookmarksFeature(
                    appStore = components.appStore,
                    bookmarksUseCase = run {
                        requireContext().components.useCases.bookmarksUseCases
                    },
                    scope = viewLifecycleOwner.lifecycleScope
                ),
                owner = viewLifecycleOwner,
                view = binding.root
            )
        }

        if (requireContext().settings().historyMetadataUIFeature) {
            historyMetadataFeature.set(
                feature = RecentVisitsFeature(
                    appStore = components.appStore,
                    historyMetadataStorage = components.core.historyStorage,
                    historyHighlightsStorage = components.core.lazyHistoryStorage,
                    scope = viewLifecycleOwner.lifecycleScope
                ),
                owner = viewLifecycleOwner,
                view = binding.root
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            controller = DefaultSessionControlController(
                activity = activity,
                settings = components.settings,
                engine = components.core.engine,
                messageController = DefaultMessageController(
                    appStore = components.appStore,
                    messagingStorage = components.analytics.messagingStorage,
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
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                removeCollectionWithUndo = ::removeCollectionWithUndo,
                showTabTray = ::openTabsTray
            ),
            recentTabController = DefaultRecentTabsController(
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                navController = findNavController(),
                store = components.core.store,
                appStore = components.appStore,
            ),
            recentSyncedTabController = DefaultRecentSyncedTabController(
                addNewTabUseCase = requireComponents.useCases.tabsUseCases.addTab,
                navController = findNavController(),
                accessPoint = TabsTrayAccessPoint.HomeRecentSyncedTab,
            ),
            recentBookmarksController = DefaultRecentBookmarksController(
                activity = activity,
                navController = findNavController(),
                appStore = components.appStore,
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
                navController = findNavController(),
            )
        )

        updateLayout(binding.root)
        sessionControlView = SessionControlView(
            binding.sessionControlRecyclerView,
            viewLifecycleOwner,
            sessionControlInteractor
        )

        updateSessionControlView()

        appBarLayout = binding.homeAppBar

        activity.themeManager.applyStatusBarTheme(activity)

        FxNimbus.features.homescreen.recordExposure()

        displayWallpaperIfEnabled()

        binding.root.doOnPreDraw {
            requireComponents.appStore.dispatch(AppAction.UpdateFirstFrameDrawn(drawn = true))
        }
        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME, profilerStartTime, "HomeFragment.onCreateView",
        )
        return binding.root
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        getMenuButton()?.dismissMenu()
        displayWallpaperIfEnabled()
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
            frecencyConfig = FrecencyThresholdOption.SKIP_ONE_TIME_PAGES,
            providerConfig = TopSitesProviderConfig(
                showProviderTopSites = settings.showContileFeature,
                maxThreshold = TOP_SITES_PROVIDER_MAX_THRESHOLD,
                providerFilter = { topSite ->
                    when (store.state.search.selectedOrDefaultSearchEngine?.name) {
                        AMAZON_SEARCH_ENGINE_NAME -> topSite.title != AMAZON_SPONSORED_TITLE
                        EBAY_SPONSORED_TITLE -> topSite.title != EBAY_SPONSORED_TITLE
                        else -> true
                    }
                }
            )
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

    private fun updateLayout(view: View) {
        when (requireContext().settings().toolbarPosition) {
            ToolbarPosition.TOP -> {
                binding.toolbarLayout.layoutParams = CoordinatorLayout.LayoutParams(
                    ConstraintLayout.LayoutParams.MATCH_PARENT,
                    ConstraintLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    gravity = Gravity.TOP
                }

                ConstraintSet().apply {
                    clone(binding.toolbarLayout)
                    clear(binding.bottomBar.id, BOTTOM)
                    clear(binding.bottomBarShadow.id, BOTTOM)
                    connect(binding.bottomBar.id, TOP, PARENT_ID, TOP)
                    connect(binding.bottomBarShadow.id, TOP, binding.bottomBar.id, BOTTOM)
                    connect(binding.bottomBarShadow.id, BOTTOM, PARENT_ID, BOTTOM)
                    applyTo(binding.toolbarLayout)
                }

                binding.bottomBar.background = AppCompatResources.getDrawable(
                    view.context,
                    view.context.theme.resolveAttribute(R.attr.bottomBarBackgroundTop)
                )

                binding.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    topMargin =
                        resources.getDimensionPixelSize(R.dimen.home_fragment_top_toolbar_header_margin)
                }
            }
            ToolbarPosition.BOTTOM -> {
            }
        }
    }

    @Suppress("LongMethod", "ComplexMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // DO NOT ADD ANYTHING ABOVE THIS getProfilerTime CALL!
        val profilerStartTime = requireComponents.core.engine.profiler?.getProfilerTime()

        super.onViewCreated(view, savedInstanceState)
        HomeScreen.homeScreenDisplayed.record(NoExtras())
        HomeScreen.homeScreenViewCount.add()

        observeSearchEngineChanges()
        observeSearchEngineNameChanges()

        HomeMenuBuilder(
            view = view,
            context = view.context,
            lifecycleOwner = viewLifecycleOwner,
            homeActivity = activity as HomeActivity,
            navController = findNavController(),
            menuButton = WeakReference(binding.menuButton),
            hideOnboardingIfNeeded = ::hideOnboardingIfNeeded,
        ).build()

        createTabCounterMenu()

        binding.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        binding.toolbarWrapper.setOnClickListener {
            navigateToSearch()
        }

        binding.toolbarWrapper.setOnLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(it),
                handlePasteAndGo = sessionControlInteractor::onPasteAndGo,
                handlePaste = sessionControlInteractor::onPaste,
                copyVisible = false
            )
            true
        }

        binding.tabButton.setOnClickListener {
            StartOnHome.openTabsTray.record(NoExtras())
            openTabsTray()
        }

        PrivateBrowsingButtonView(binding.privateBrowsingButton, browsingModeManager) { newMode ->
            sessionControlInteractor.onPrivateModeButtonClicked(
                newMode,
                onboarding.userHasBeenOnboarded()
            )
        }

        consumeFrom(requireComponents.core.store) {
            updateTabCounter(it)
        }

        homeViewModel.sessionToDelete?.also {
            if (it == ALL_NORMAL_TABS || it == ALL_PRIVATE_TABS) {
                removeAllTabsAndShowSnackbar(it)
            } else {
                removeTabAndShowSnackbar(it)
            }
        }

        homeViewModel.sessionToDelete = null

        updateTabCounter(requireComponents.core.store.state)

        if (bundleArgs.getBoolean(FOCUS_ON_ADDRESS_BAR)) {
            navigateToSearch()
        }

        // DO NOT MOVE ANYTHING BELOW THIS addMarker CALL!
        requireComponents.core.engine.profiler?.addMarker(
            MarkersFragmentLifecycleCallbacks.MARKER_NAME, profilerStartTime, "HomeFragment.onViewCreated",
        )
    }

    private fun observeSearchEngineChanges() {
        consumeFlow(store) { flow ->
            flow.map { state -> state.search.selectedOrDefaultSearchEngine }
                .ifChanged()
                .collect { searchEngine ->
                    if (searchEngine != null) {
                        val iconSize =
                            requireContext().resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)
                        val searchIcon =
                            BitmapDrawable(requireContext().resources, searchEngine.icon)
                        searchIcon.setBounds(0, 0, iconSize, iconSize)
                        binding.searchEngineIcon.setImageDrawable(searchIcon)
                    } else {
                        binding.searchEngineIcon.setImageDrawable(null)
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
                .ifChanged()
                .collect {
                    topSitesFeature.withFeature {
                        it.storage.notifyObservers { onStorageUpdated() }
                    }
                }
        }
    }

    private fun createTabCounterMenu() {
        val browsingModeManager = (activity as HomeActivity).browsingModeManager
        val mode = browsingModeManager.mode

        val onItemTapped: (TabCounterMenu.Item) -> Unit = {
            if (it is TabCounterMenu.Item.NewTab) {
                browsingModeManager.mode = BrowsingMode.Normal
            } else if (it is TabCounterMenu.Item.NewPrivateTab) {
                browsingModeManager.mode = BrowsingMode.Private
            }
        }

        val tabCounterMenu = FenixTabCounterMenu(
            requireContext(),
            onItemTapped,
            iconColor = if (mode == BrowsingMode.Private) {
                ContextCompat.getColor(requireContext(), R.color.fx_mobile_private_text_color_primary)
            } else {
                null
            }
        )

        val inverseBrowsingMode = when (mode) {
            BrowsingMode.Normal -> BrowsingMode.Private
            BrowsingMode.Private -> BrowsingMode.Normal
        }

        tabCounterMenu.updateMenu(showOnly = inverseBrowsingMode)
        binding.tabButton.setOnLongClickListener {
            tabCounterMenu.menuController.show(anchor = it)
            true
        }
    }

    private fun removeAllTabsAndShowSnackbar(sessionCode: String) {
        if (sessionCode == ALL_PRIVATE_TABS) {
            requireComponents.useCases.tabsUseCases.removePrivateTabs()
        } else {
            requireComponents.useCases.tabsUseCases.removeNormalTabs()
        }

        val snackbarMessage = if (sessionCode == ALL_PRIVATE_TABS) {
            getString(R.string.snackbar_private_tabs_closed)
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
            anchorView = snackbarAnchorView
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
                    HomeFragmentDirections.actionGlobalBrowser(null)
                )
            },
            operation = { },
            anchorView = snackbarAnchorView
        )
    }

    override fun onDestroyView() {
        super.onDestroyView()

        _sessionControlInteractor = null
        sessionControlView = null
        appBarLayout = null
        _binding = null
        bundleArgs.clear()
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
                currentMode,
                owner = this@HomeFragment.viewLifecycleOwner
            )
            requireComponents.backgroundServices.accountManager.register(
                object : AccountObserver {
                    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                        if (authType != AuthType.Existing) {
                            view?.let {
                                FenixSnackbar.make(
                                    view = it,
                                    duration = Snackbar.LENGTH_SHORT,
                                    isDisplayedWithBrowserToolbar = false
                                )
                                    .setText(it.context.getString(R.string.onboarding_firefox_account_sync_is_on))
                                    .setAnchorView(binding.toolbarLayout)
                                    .show()
                            }
                        }
                    }
                },
                owner = this@HomeFragment.viewLifecycleOwner
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

        if (shouldEnableWallpaper() && context.settings().wallpapersSwitchedByLogoTap) {
            binding.wordmark.contentDescription =
                context.getString(R.string.wallpaper_logo_content_description)
            binding.wordmark.setOnClickListener {
                val manager = requireComponents.wallpaperManager
                val newWallpaper = manager.switchToNextWallpaper()
                Wallpapers.wallpaperSwitched.record(
                    Wallpapers.WallpaperSwitchedExtra(
                        name = newWallpaper.name,
                        themeCollection = newWallpaper::class.simpleName
                    )
                )
                manager.updateWallpaper(
                    wallpaperContainer = binding.wallpaperImageView,
                    newWallpaper = newWallpaper
                )
            }
        }
    }

    private fun dispatchModeChanges(mode: Mode) {
        if (mode != Mode.fromBrowsingMode(browsingModeManager.mode)) {
            requireContext().components.appStore.dispatch(AppAction.ModeChange(mode))
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
            anchorView = null
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
        if (shouldAnimateLogoForWallpaper()) {
            _binding?.sessionControlRecyclerView?.viewTreeObserver?.addOnGlobalLayoutListener(
                homeLayoutListenerForLogoAnimation
            )
        }
    }

    // To try to find a good time to show the logo animation, we are waiting until all
    // the sub-recyclerviews (recentBookmarks, collections, recentTabs,recentVisits
    // and pocketStories) on the home screen have been layout.
    private val homeLayoutListenerForLogoAnimation = object : ViewTreeObserver.OnGlobalLayoutListener {
        override fun onGlobalLayout() {
            _binding?.let { safeBindings ->
                requireComponents.wallpaperManager.animateLogoIfNeeded(safeBindings.wordmark)
                safeBindings.sessionControlRecyclerView.viewTreeObserver.removeOnGlobalLayoutListener(
                    this
                )
            }
        }
    }

    override fun onPause() {
        super.onPause()
        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawable(
                ColorDrawable(
                    ContextCompat.getColor(
                        requireContext(),
                        R.color.fx_mobile_private_layer_color_1
                    )
                )
            )
        }

        // Counterpart to the update in onResume to keep the last access timestamp of the selected
        // tab up-to-date.
        requireComponents.useCases.sessionUseCases.updateLastAccess()
    }

    @SuppressLint("InflateParams")
    private fun recommendPrivateBrowsingShortcut() {
        context?.let { context ->
            val layout = LayoutInflater.from(context)
                .inflate(R.layout.pbm_shortcut_popup, null)
            val privateBrowsingRecommend =
                PopupWindow(
                    layout,
                    min(
                        (resources.displayMetrics.widthPixels / CFR_WIDTH_DIVIDER).toInt(),
                        (resources.displayMetrics.heightPixels / CFR_WIDTH_DIVIDER).toInt()
                    ),
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    true
                )
            layout.findViewById<Button>(R.id.cfr_pos_button).apply {
                setOnClickListener {
                    PrivateShortcutCreateManager.createPrivateShortcut(context)
                    privateBrowsingRecommend.dismiss()
                }
            }
            layout.findViewById<Button>(R.id.cfr_neg_button).apply {
                setOnClickListener {
                    privateBrowsingRecommend.dismiss()
                }
            }
            // We want to show the popup only after privateBrowsingButton is available.
            // Otherwise, we will encounter an activity token error.
            binding.privateBrowsingButton.post {
                runIfFragmentIsAttached {
                    context.settings().showedPrivateModeContextualFeatureRecommender = true
                    context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                    privateBrowsingRecommend.showAsDropDown(
                        binding.privateBrowsingButton, 0, CFR_Y_OFFSET, Gravity.TOP or Gravity.END
                    )
                }
            }
        }
    }

    private fun hideOnboardingIfNeeded() {
        if (!onboarding.userHasBeenOnboarded()) {
            onboarding.finish()
            requireContext().components.appStore.dispatch(
                AppAction.ModeChange(
                    mode = currentMode.getCurrentMode()
                )
            )
        }
    }

    private fun hideOnboardingAndOpenSearch() {
        hideOnboardingIfNeeded()
        appBarLayout?.setExpanded(true, true)
        navigateToSearch()
    }

    @VisibleForTesting
    internal fun navigateToSearch() {
        val directions =
            HomeFragmentDirections.actionGlobalSearchDialog(
                sessionId = null
            )

        nav(R.id.homeFragment, directions, getToolbarNavOptions(requireContext()))

        Events.searchBarTapped.record(Events.SearchBarTappedExtra("HOME"))
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
                isDisplayedWithBrowserToolbar = false
            )
                .setText(string)
                .setAnchorView(snackbarAnchorView)
                .show()
        }
    }

    private fun openTabsTray() {
        findNavController().nav(
            R.id.homeFragment,
            HomeFragmentDirections.actionGlobalTabsTrayFragment()
        )
    }

    // TODO use [FenixTabCounterToolbarButton] instead of [TabCounter]:
    // https://github.com/mozilla-mobile/fenix/issues/16792
    private fun updateTabCounter(browserState: BrowserState) {
        val tabCount = if (browsingModeManager.mode.isPrivate) {
            browserState.privateTabs.size
        } else {
            browserState.normalTabs.size
        }

        binding.tabButton.setCountWithAnimation(tabCount)
        // The add_tabs_to_collections_button is added at runtime. We need to search for it in the same way.
        sessionControlView?.view?.findViewById<MaterialButton>(R.id.add_tabs_to_collections_button)
            ?.isVisible = tabCount > 0
    }

    private fun displayWallpaperIfEnabled() {
        if (shouldEnableWallpaper()) {
            val wallpaperManger = requireComponents.wallpaperManager
            // We only want to update the wallpaper when it's different from the default one
            // as the default is applied already on xml by default.
            if (wallpaperManger.currentWallpaper != WallpaperManager.defaultWallpaper) {
                wallpaperManger.updateWallpaper(binding.wallpaperImageView, wallpaperManger.currentWallpaper)
            }
        }
    }

    // We want to show the animation in a time when the user less distracted
    // The Heuristics are:
    // 1) The animation hasn't shown before.
    // 2) The user has onboarded.
    // 3) It's the third time the user enters the app.
    // 4) The user is part of the right audience.
    @Suppress("MagicNumber")
    private fun shouldAnimateLogoForWallpaper(): Boolean {
        val localContext = context ?: return false
        val settings = localContext.settings()

        return shouldEnableWallpaper() && settings.shouldAnimateFirefoxLogo &&
            onboarding.userHasBeenOnboarded() &&
            settings.numberOfAppLaunches >= 3
    }

    private fun shouldEnableWallpaper() =
        (activity as? HomeActivity)?.themeManager?.currentTheme?.isPrivate?.not() ?: false

    companion object {
        const val ALL_NORMAL_TABS = "all_normal"
        const val ALL_PRIVATE_TABS = "all_private"

        private const val FOCUS_ON_ADDRESS_BAR = "focusOnAddressBar"

        private const val CFR_WIDTH_DIVIDER = 1.7
        private const val CFR_Y_OFFSET = -20

        // Sponsored top sites titles and search engine names used for filtering
        const val AMAZON_SPONSORED_TITLE = "Amazon"
        const val AMAZON_SEARCH_ENGINE_NAME = "Amazon.com"
        const val EBAY_SPONSORED_TITLE = "eBay"

        // Elevation for undo toasts
        internal const val TOAST_ELEVATION = 80f
    }
}

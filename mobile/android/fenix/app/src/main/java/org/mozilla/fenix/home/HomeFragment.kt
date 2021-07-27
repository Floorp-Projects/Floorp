/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.animation.Animator
import android.content.Context
import android.content.res.Configuration
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.os.StrictMode
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.View.AccessibilityDelegate
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
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
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.android.synthetic.main.no_collections_message.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
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
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.Config
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.accounts.AccountState
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.tips.FenixTipManager
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.providers.MasterPasswordTipProvider
import org.mozilla.fenix.components.toolbar.FenixTabCounterMenu
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.historymetadata.HistoryMetadataFeature
import org.mozilla.fenix.historymetadata.controller.DefaultHistoryMetadataController
import org.mozilla.fenix.home.mozonline.showPrivacyPopWindow
import org.mozilla.fenix.home.recentbookmarks.RecentBookmarksFeature
import org.mozilla.fenix.home.recentbookmarks.controller.DefaultRecentBookmarksController
import org.mozilla.fenix.home.recenttabs.RecentTabsListFeature
import org.mozilla.fenix.home.recenttabs.controller.DefaultRecentTabsController
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.SessionControlView
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.topsites.DefaultTopSitesView
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.HELP
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.ToolbarPopupWindow
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.whatsnew.WhatsNew
import java.lang.ref.WeakReference
import kotlin.math.min

@ExperimentalCoroutinesApi
@Suppress("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
    private val args by navArgs<HomeFragmentArgs>()
    private lateinit var bundleArgs: Bundle

    private val homeViewModel: HomeScreenViewModel by activityViewModels()

    private val snackbarAnchorView: View?
        get() = when (requireContext().settings().toolbarPosition) {
            ToolbarPosition.BOTTOM -> toolbarLayout
            ToolbarPosition.TOP -> null
        }

    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionRenamed(tabCollection: TabCollection, title: String) {
            lifecycleScope.launch(Main) {
                view?.sessionControlRecyclerView?.adapter?.notifyDataSetChanged()
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

    private lateinit var homeFragmentStore: HomeFragmentStore
    private var _sessionControlInteractor: SessionControlInteractor? = null
    protected val sessionControlInteractor: SessionControlInteractor
        get() = _sessionControlInteractor!!

    private var sessionControlView: SessionControlView? = null
    private var appBarLayout: AppBarLayout? = null
    private lateinit var currentMode: CurrentMode

    private val topSitesFeature = ViewBoundFeatureWrapper<TopSitesFeature>()
    private val recentTabsListFeature = ViewBoundFeatureWrapper<RecentTabsListFeature>()
    private val recentBookmarksFeature = ViewBoundFeatureWrapper<RecentBookmarksFeature>()
    private val historyMetadataFeature = ViewBoundFeatureWrapper<HistoryMetadataFeature>()

    @VisibleForTesting
    internal var getMenuButton: () -> MenuButton? = { menuButton }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        bundleArgs = args.toBundle()
        lifecycleScope.launch(IO) {
            if (!onboarding.userHasBeenOnboarded()) {
                requireComponents.analytics.metrics.track(Event.OpenedAppFirstRun)
            }
        }

        if (!onboarding.userHasBeenOnboarded() &&
            requireContext().settings().shouldShowPrivacyPopWindow &&
            Config.channel.isMozillaOnline
        ) {
            showPrivacyPopWindow(requireContext(), requireActivity())
        }
    }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_home, container, false)
        val activity = activity as HomeActivity
        val components = requireComponents

        currentMode = CurrentMode(
            view.context,
            onboarding,
            browsingModeManager,
            ::dispatchModeChanges
        )

        homeFragmentStore = StoreProvider.get(this) {
            HomeFragmentStore(
                HomeFragmentState(
                    collections = components.core.tabCollectionStorage.cachedTabCollections,
                    expandedCollections = emptySet(),
                    mode = currentMode.getCurrentMode(),
                    topSites = components.core.topSitesStorage.cachedTopSites,
                    tip = components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                        FenixTipManager(
                            listOf(
                                MasterPasswordTipProvider(
                                    requireContext(),
                                    ::navToSavedLogins,
                                    ::dismissTip
                                )
                            )
                        ).getTip()
                    },
                    recentBookmarks = emptyList(),
                    showCollectionPlaceholder = components.settings.showCollectionsPlaceholderOnHome,
                    showSetAsDefaultBrowserCard = components.settings.shouldShowSetAsDefaultBrowserCard(),
                    recentTabs = emptyList(),
                    historyMetadata = emptyList()
                )
            )
        }

        topSitesFeature.set(
            feature = TopSitesFeature(
                view = DefaultTopSitesView(homeFragmentStore),
                storage = components.core.topSitesStorage,
                config = ::getTopSitesConfig
            ),
            owner = viewLifecycleOwner,
            view = view
        )

        if (FeatureFlags.showRecentTabsFeature) {
            recentTabsListFeature.set(
                feature = RecentTabsListFeature(
                    browserStore = components.core.store,
                    homeStore = homeFragmentStore
                ),
                owner = viewLifecycleOwner,
                view = view
            )
        }

        if (FeatureFlags.recentBookmarksFeature) {
            recentBookmarksFeature.set(
                feature = RecentBookmarksFeature(
                    homeStore = homeFragmentStore,
                    bookmarksUseCase = run {
                        requireContext().components.useCases.bookmarksUseCases
                    },
                    scope = viewLifecycleOwner.lifecycleScope
                ),
                owner = viewLifecycleOwner,
                view = view
            )
        }

        if (requireContext().settings().historyMetadataFeature) {
            historyMetadataFeature.set(
                feature = HistoryMetadataFeature(
                    homeStore = homeFragmentStore,
                    historyMetadataStorage = components.core.historyStorage,
                    scope = viewLifecycleOwner.lifecycleScope
                ),
                owner = viewLifecycleOwner,
                view = view
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            controller = DefaultSessionControlController(
                activity = activity,
                settings = components.settings,
                engine = components.core.engine,
                metrics = components.analytics.metrics,
                store = store,
                tabCollectionStorage = components.core.tabCollectionStorage,
                addTabUseCase = components.useCases.tabsUseCases.addTab,
                restoreUseCase = components.useCases.tabsUseCases.restore,
                reloadUrlUseCase = components.useCases.sessionUseCases.reload,
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                fragmentStore = homeFragmentStore,
                navController = findNavController(),
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                removeCollectionWithUndo = ::removeCollectionWithUndo,
                showTabTray = ::openTabsTray,
                handleSwipedItemDeletionCancel = ::handleSwipedItemDeletionCancel
            ),
            recentTabController = DefaultRecentTabsController(
                selectTabUseCase = components.useCases.tabsUseCases.selectTab,
                navController = findNavController(),
                metrics = requireComponents.analytics.metrics,
                store = components.core.store
            ),
            recentBookmarksController = DefaultRecentBookmarksController(
                activity = activity,
                navController = findNavController()
            ),
            historyMetadataController = DefaultHistoryMetadataController(
                activity = activity,
                settings = components.settings,
                homeFragmentStore = homeFragmentStore,
                selectOrAddUseCase = components.useCases.tabsUseCases.selectOrAddTab,
                navController = findNavController()
            )
        )

        updateLayout(view)
        sessionControlView = SessionControlView(
            view.sessionControlRecyclerView,
            viewLifecycleOwner,
            sessionControlInteractor,
            homeViewModel
        )

        updateSessionControlView(view)

        appBarLayout = view.homeAppBar

        activity.themeManager.applyStatusBarTheme(activity)
        return view
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        getMenuButton()?.dismissMenu()
    }

    private fun dismissTip(tip: Tip) {
        sessionControlInteractor.onCloseTip(tip)
    }

    /**
     * Returns a [TopSitesConfig] which specifies how many top sites to display and whether or
     * not frequently visited sites should be displayed.
     */
    @VisibleForTesting
    internal fun getTopSitesConfig(): TopSitesConfig {
        val settings = requireContext().settings()
        return TopSitesConfig(
            settings.topSitesMaxLimit,
            if (settings.showTopFrecentSites) FrecencyThresholdOption.SKIP_ONE_TIME_PAGES else null
        )
    }

    /**
     * The [SessionControlView] is forced to update with our current state when we call
     * [HomeFragment.onCreateView] in order to be able to draw everything at once with the current
     * data in our store. The [View.consumeFrom] coroutine dispatch
     * doesn't get run right away which means that we won't draw on the first layout pass.
     */
    private fun updateSessionControlView(view: View) {
        if (browsingModeManager.mode == BrowsingMode.Private) {
            view.consumeFrom(homeFragmentStore, viewLifecycleOwner) {
                sessionControlView?.update(it)
            }
        } else {
            sessionControlView?.update(homeFragmentStore.state)

            view.consumeFrom(homeFragmentStore, viewLifecycleOwner) {
                sessionControlView?.update(it)
            }
        }
    }

    private fun updateLayout(view: View) {
        when (view.context.settings().toolbarPosition) {
            ToolbarPosition.TOP -> {
                view.toolbarLayout.layoutParams = CoordinatorLayout.LayoutParams(
                    ConstraintLayout.LayoutParams.MATCH_PARENT,
                    ConstraintLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    gravity = Gravity.TOP
                }

                ConstraintSet().apply {
                    clone(view.toolbarLayout)
                    clear(view.bottom_bar.id, BOTTOM)
                    clear(view.bottomBarShadow.id, BOTTOM)
                    connect(view.bottom_bar.id, TOP, PARENT_ID, TOP)
                    connect(view.bottomBarShadow.id, TOP, view.bottom_bar.id, BOTTOM)
                    connect(view.bottomBarShadow.id, BOTTOM, PARENT_ID, BOTTOM)
                    applyTo(view.toolbarLayout)
                }

                view.bottom_bar.background = AppCompatResources.getDrawable(
                    view.context,
                    view.context.theme.resolveAttribute(R.attr.bottomBarBackgroundTop)
                )

                view.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
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
        super.onViewCreated(view, savedInstanceState)
        context?.metrics?.track(Event.HomeScreenDisplayed)

        observeSearchEngineChanges()
        createHomeMenu(requireContext(), WeakReference(view.menuButton))
        createTabCounterMenu(view)

        view.menuButton.setColorFilter(
            ContextCompat.getColor(
                requireContext(),
                ThemeManager.resolveAttribute(R.attr.primaryText, requireContext())
            )
        )

        view.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        view.toolbar_wrapper.setOnClickListener {
            navigateToSearch()
            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        view.toolbar_wrapper.setOnLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(it),
                handlePasteAndGo = sessionControlInteractor::onPasteAndGo,
                handlePaste = sessionControlInteractor::onPaste,
                copyVisible = false
            )
            true
        }

        view.tab_button.setOnClickListener {
            if (FeatureFlags.showStartOnHomeSettings) {
                requireComponents.analytics.metrics.track(Event.StartOnHomeOpenTabsTray)
            }
            openTabsTray()
        }

        PrivateBrowsingButtonView(
            privateBrowsingButton,
            browsingModeManager
        ) { newMode ->
            if (newMode == BrowsingMode.Private) {
                requireContext().settings().incrementNumTimesPrivateModeOpened()
            }

            if (onboarding.userHasBeenOnboarded()) {
                homeFragmentStore.dispatch(
                    HomeFragmentAction.ModeChange(Mode.fromBrowsingMode(newMode))
                )
            }
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
        } else if (bundleArgs.getLong(FOCUS_ON_COLLECTION, -1) >= 0) {
            // No need to scroll to async'd loaded TopSites if we want to scroll to collections.
            homeViewModel.shouldScrollToTopSites = false
            /* Triggered when the user has added a tab to a collection and has tapped
            * the View action on the [TabsTrayDialogFragment] snackbar.*/
            scrollAndAnimateCollection(bundleArgs.getLong(FOCUS_ON_COLLECTION, -1))
        }
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
                        search_engine_icon?.setImageDrawable(searchIcon)
                    } else {
                        search_engine_icon.setImageDrawable(null)
                    }
                }
        }
    }

    private fun createTabCounterMenu(view: View) {
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
            view.context,
            onItemTapped,
            iconColor = if (mode == BrowsingMode.Private) {
                ContextCompat.getColor(requireContext(), R.color.primary_text_private_theme)
            } else {
                null
            }
        )

        val inverseBrowsingMode = when (mode) {
            BrowsingMode.Normal -> BrowsingMode.Private
            BrowsingMode.Private -> BrowsingMode.Normal
        }

        tabCounterMenu.updateMenu(showOnly = inverseBrowsingMode)
        view.tab_button.setOnLongClickListener {
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
        bundleArgs.clear()
    }

    override fun onStart() {
        super.onStart()

        subscribeToTabCollections()

        val context = requireContext()
        val components = context.components

        homeFragmentStore.dispatch(
            HomeFragmentAction.Change(
                collections = components.core.tabCollectionStorage.cachedTabCollections,
                mode = currentMode.getCurrentMode(),
                topSites = components.core.topSitesStorage.cachedTopSites,
                tip = components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
                    FenixTipManager(
                        listOf(
                            MasterPasswordTipProvider(
                                requireContext(),
                                ::navToSavedLogins,
                                ::dismissTip
                            )
                        )
                    ).getTip()
                },
                showCollectionPlaceholder = components.settings.showCollectionsPlaceholderOnHome,
                recentTabs = emptyList(),
                recentBookmarks = emptyList(),
                historyMetadata = emptyList()
            )
        )

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
                                    .setAnchorView(toolbarLayout)
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
    }

    private fun navToSavedLogins() {
        findNavController().navigate(
            HomeFragmentDirections.actionGlobalSavedLoginsAuthFragment()
        )
    }

    private fun dispatchModeChanges(mode: Mode) {
        if (mode != Mode.fromBrowsingMode(browsingModeManager.mode)) {
            homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(mode))
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
    }

    override fun onPause() {
        super.onPause()
        if (browsingModeManager.mode == BrowsingMode.Private) {
            activity?.window?.setBackgroundDrawable(
                ColorDrawable(
                    ContextCompat.getColor(
                        requireContext(),
                        R.color.foundation_private_theme
                    )
                )
            )
        }
    }

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
                    context.metrics.track(Event.PrivateBrowsingAddShortcutCFR)
                    PrivateShortcutCreateManager.createPrivateShortcut(context)
                    privateBrowsingRecommend.dismiss()
                }
            }
            layout.findViewById<Button>(R.id.cfr_neg_button).apply {
                setOnClickListener {
                    context.metrics.track(Event.PrivateBrowsingCancelCFR)
                    privateBrowsingRecommend.dismiss()
                }
            }
            // We want to show the popup only after privateBrowsingButton is available.
            // Otherwise, we will encounter an activity token error.
            privateBrowsingButton.post {
                runIfFragmentIsAttached {
                    context.settings().showedPrivateModeContextualFeatureRecommender = true
                    context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                    privateBrowsingRecommend.showAsDropDown(
                        privateBrowsingButton, 0, CFR_Y_OFFSET, Gravity.TOP or Gravity.END
                    )
                }
            }
        }
    }

    private fun hideOnboardingIfNeeded() {
        if (!onboarding.userHasBeenOnboarded()) {
            onboarding.finish()
            homeFragmentStore.dispatch(
                HomeFragmentAction.ModeChange(
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

    private fun navigateToSearch() {
        // Dismisses the search dialog when the home content is scrolled
        val recyclerView = sessionControlView!!.view
        val listener = object : RecyclerView.OnScrollListener() {
            override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
                super.onScrollStateChanged(recyclerView, newState)
                if (newState == RecyclerView.SCROLL_STATE_DRAGGING || newState == RecyclerView.SCROLL_STATE_SETTLING) {
                    findNavController().navigateUp()
                    recyclerView.removeOnScrollListener(this)
                }
            }
        }

        recyclerView.addOnScrollListener(listener)

        val directions =
            HomeFragmentDirections.actionGlobalSearchDialog(
                sessionId = null
            )

        nav(R.id.homeFragment, directions, getToolbarNavOptions(requireContext()))
    }

    @SuppressWarnings("ComplexMethod", "LongMethod")
    private fun createHomeMenu(context: Context, menuButtonView: WeakReference<MenuButton>) =
        HomeMenu(
            this.viewLifecycleOwner,
            context,
            onItemTapped = {
                when (it) {
                    HomeMenu.Item.Settings -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalSettingsFragment()
                        )
                        requireComponents.analytics.metrics.track(Event.HomeMenuSettingsItemClicked)
                    }
                    is HomeMenu.Item.SyncAccount -> {
                        hideOnboardingIfNeeded()
                        val directions = when (it.accountState) {
                            AccountState.AUTHENTICATED ->
                                BrowserFragmentDirections.actionGlobalAccountSettingsFragment()
                            AccountState.NEEDS_REAUTHENTICATION ->
                                BrowserFragmentDirections.actionGlobalAccountProblemFragment()
                            AccountState.NO_ACCOUNT ->
                                BrowserFragmentDirections.actionGlobalTurnOnSync()
                        }
                        nav(
                            R.id.homeFragment,
                            directions
                        )
                    }
                    HomeMenu.Item.Bookmarks -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
                        )
                    }
                    HomeMenu.Item.History -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalHistoryFragment()
                        )
                    }

                    HomeMenu.Item.Downloads -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalDownloadsFragment()
                        )
                    }

                    HomeMenu.Item.Help -> {
                        hideOnboardingIfNeeded()
                        (activity as HomeActivity).openToBrowserAndLoad(
                            searchTermOrURL = SupportUtils.getSumoURLForTopic(context, HELP),
                            newTab = true,
                            from = BrowserDirection.FromHome
                        )
                    }
                    HomeMenu.Item.WhatsNew -> {
                        hideOnboardingIfNeeded()
                        WhatsNew.userViewedWhatsNew(context)
                        context.metrics.track(Event.WhatsNewTapped)
                        (activity as HomeActivity).openToBrowserAndLoad(
                            searchTermOrURL = SupportUtils.getWhatsNewUrl(context),
                            newTab = true,
                            from = BrowserDirection.FromHome
                        )
                    }
                    // We need to show the snackbar while the browsing data is deleting(if "Delete
                    // browsing data on quit" is activated). After the deletion is over, the snackbar
                    // is dismissed.
                    HomeMenu.Item.Quit -> activity?.let { activity ->
                        deleteAndQuit(
                            activity,
                            viewLifecycleOwner.lifecycleScope,
                            view?.let { view ->
                                FenixSnackbar.make(
                                    view = view,
                                    isDisplayedWithBrowserToolbar = false
                                )
                            }
                        )
                    }
                    HomeMenu.Item.ReconnectSync -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalAccountProblemFragment()
                        )
                    }
                    HomeMenu.Item.Extensions -> {
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalAddonsManagementFragment()
                        )
                    }
                    is HomeMenu.Item.DesktopMode -> {
                        context.settings().openNextTabInDesktopMode = it.checked
                    }
                }
            },
            onHighlightPresent = { menuButtonView.get()?.setHighlight(it) },
            onMenuBuilderChanged = { menuButtonView.get()?.menuBuilder = it }
        )

    private fun subscribeToTabCollections(): Observer<List<TabCollection>> {
        return Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
            homeFragmentStore.dispatch(HomeFragmentAction.CollectionsChange(it))
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections().observe(this, observer)
        }
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    /**
     * This method will find and scroll to the row of the specified collection Id.
     * */
    private fun scrollAndAnimateCollection(
        collectionIdToSelect: Long = -1
    ) {
        if (view != null && collectionIdToSelect >= 0) {
            viewLifecycleOwner.lifecycleScope.launch {
                val recyclerView = sessionControlView!!.view
                delay(ANIM_SCROLL_DELAY)
                val indexOfCollection =
                    NON_COLLECTION_ITEM_NUM + findIndexOfSpecificCollection(collectionIdToSelect)

                val lastVisiblePosition =
                    (recyclerView.layoutManager as? LinearLayoutManager)?.findLastCompletelyVisibleItemPosition()
                        ?: 0

                if (lastVisiblePosition < indexOfCollection) {
                    val onScrollListener = object : RecyclerView.OnScrollListener() {
                        override fun onScrollStateChanged(
                            recyclerView: RecyclerView,
                            newState: Int
                        ) {
                            super.onScrollStateChanged(recyclerView, newState)
                            if (newState == SCROLL_STATE_IDLE) {
                                appBarLayout?.setExpanded(false)
                                animateCollection(indexOfCollection)
                                recyclerView.removeOnScrollListener(this)
                            }
                        }
                    }
                    recyclerView.addOnScrollListener(onScrollListener)
                    recyclerView.smoothScrollToPosition(indexOfCollection)
                } else {
                    appBarLayout?.setExpanded(false)
                    animateCollection(indexOfCollection)
                }
            }
        }
    }

    /**
     * Returns index of the collection with the specified id.
     * */
    private fun findIndexOfSpecificCollection(
        changedCollectionId: Long
    ): Int {
        var result = 0
        requireComponents.core.tabCollectionStorage.cachedTabCollections
            .filterIndexed { index, tabCollection ->
                if (tabCollection.id == changedCollectionId) {
                    result = index
                    return@filterIndexed true
                }
                false
            }
        return result
    }

    /**
     * Will highlight the border of the collection with the specified index.
     * */
    private fun animateCollection(indexOfCollection: Int) {
        viewLifecycleOwner.lifecycleScope.launch {
            val viewHolder =
                sessionControlView!!.view.findViewHolderForAdapterPosition(indexOfCollection)
            val border =
                (viewHolder as? CollectionViewHolder)?.itemView?.findViewById<View>(R.id.selected_border)
            val listener = object : Animator.AnimatorListener {
                override fun onAnimationCancel(animation: Animator?) {
                    border?.visibility = View.GONE
                }

                override fun onAnimationStart(animation: Animator?) { /* noop */
                }

                override fun onAnimationRepeat(animation: Animator?) { /* noop */
                }

                override fun onAnimationEnd(animation: Animator?) {
                    border?.animate()?.alpha(0.0F)?.setStartDelay(ANIM_ON_SCREEN_DELAY)
                        ?.setDuration(FADE_ANIM_DURATION)
                        ?.start()
                }
            }
            border?.animate()?.alpha(1.0F)?.setStartDelay(ANIM_ON_SCREEN_DELAY)
                ?.setDuration(FADE_ANIM_DURATION)
                ?.setListener(listener)?.start()
        }.invokeOnCompletion {
            val a11yEnabled = context?.settings()?.accessibilityServicesEnabled ?: false
            if (a11yEnabled) {
                focusCollectionForTalkBack(indexOfCollection)
            }
        }
    }

    /**
     * Will focus the collection with [indexOfCollection] for accessibility services.
     * */
    private fun focusCollectionForTalkBack(indexOfCollection: Int) {
        viewLifecycleOwner.lifecycleScope.launch {
            var focusedForAccessibility = false
            view?.let { mainView ->
                mainView.accessibilityDelegate = object : AccessibilityDelegate() {
                    override fun onRequestSendAccessibilityEvent(
                        host: ViewGroup,
                        child: View,
                        event: AccessibilityEvent
                    ): Boolean {
                        if (!focusedForAccessibility &&
                            event.eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED
                        ) {
                            sessionControlView?.view?.findViewHolderForAdapterPosition(
                                indexOfCollection
                            )?.itemView?.let { viewToFocus ->
                                focusedForAccessibility = true
                                viewToFocus.requestFocus()
                                viewToFocus.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
                                return false
                            }
                        }
                        return super.onRequestSendAccessibilityEvent(host, child, event)
                    }
                }
            }
        }
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

        view?.tab_button?.setCountWithAnimation(tabCount)
        view?.add_tabs_to_collections_button?.isVisible = tabCount > 0
    }

    private fun handleSwipedItemDeletionCancel() {
        view?.sessionControlRecyclerView?.adapter?.notifyDataSetChanged()
    }

    companion object {
        const val ALL_NORMAL_TABS = "all_normal"
        const val ALL_PRIVATE_TABS = "all_private"

        private const val FOCUS_ON_ADDRESS_BAR = "focusOnAddressBar"
        private const val FOCUS_ON_COLLECTION = "focusOnCollection"

        /**
         * Represents the number of items in [sessionControlView] that are NOT part of
         * the list of collections. At the moment these are topSites pager, collections header.
         * */
        private const val NON_COLLECTION_ITEM_NUM = 2

        private const val ANIM_SCROLL_DELAY = 100L
        private const val ANIM_ON_SCREEN_DELAY = 200L
        private const val FADE_ANIM_DURATION = 150L
        private const val CFR_WIDTH_DIVIDER = 1.7
        private const val CFR_Y_OFFSET = -20

        // Elevation for undo toasts
        internal const val TOAST_ELEVATION = 80f
    }
}

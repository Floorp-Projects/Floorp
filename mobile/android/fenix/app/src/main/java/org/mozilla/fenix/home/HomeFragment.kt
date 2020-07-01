/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.animation.Animator
import android.content.Context
import android.content.DialogInterface
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.os.StrictMode
import android.view.Display.FLAG_SECURE
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.widget.Button
import android.widget.LinearLayout
import android.widget.PopupWindow
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.constraintlayout.widget.ConstraintSet.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.TOP
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.doOnLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.android.synthetic.main.no_collections_message.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.util.dpToPx
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.cfr.SearchWidgetCFR
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.tips.FenixTipManager
import org.mozilla.fenix.components.tips.providers.MigrationTipProvider
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.resetPoliciesAfter
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.SessionControlView
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.HELP
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.FragmentPreDrawManager
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

    private val homeViewModel: HomeScreenViewModel by viewModels {
        ViewModelProvider.AndroidViewModelFactory(requireActivity().application)
    }

    private val snackbarAnchorView: View?
        get() = when (requireContext().settings().toolbarPosition) {
            ToolbarPosition.BOTTOM -> toolbarLayout
            ToolbarPosition.TOP -> null
        }

    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            scrollAndAnimateCollection()
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            scrollAndAnimateCollection(tabCollection)
        }

        override fun onCollectionRenamed(tabCollection: TabCollection, title: String) {
            showRenamedSnackbar()
        }
    }

    private val sessionManager: SessionManager
        get() = requireComponents.core.sessionManager
    private val store: BrowserStore
        get() = requireComponents.core.store

    private val onboarding by lazy { StrictMode.allowThreadDiskReads().resetPoliciesAfter {
        FenixOnboarding(requireContext()) } }

    private lateinit var homeFragmentStore: HomeFragmentStore
    private var _sessionControlInteractor: SessionControlInteractor? = null
    protected val sessionControlInteractor: SessionControlInteractor
        get() = _sessionControlInteractor!!

    private var sessionControlView: SessionControlView? = null
    private lateinit var currentMode: CurrentMode

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
        bundleArgs = args.toBundle()
        lifecycleScope.launch(IO) {
            if (!onboarding.userHasBeenOnboarded()) {
                requireComponents.analytics.metrics.track(Event.OpenedAppFirstRun)
            }
        }
    }

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
                    topSites = StrictMode.allowThreadDiskReads().resetPoliciesAfter {
                        components.core.topSiteStorage.cachedTopSites
                    },
                    tip = FenixTipManager(listOf(MigrationTipProvider(requireContext()))).getTip()
                )
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            DefaultSessionControlController(
                activity = activity,
                engine = components.core.engine,
                metrics = components.analytics.metrics,
                sessionManager = sessionManager,
                tabCollectionStorage = components.core.tabCollectionStorage,
                topSiteStorage = components.core.topSiteStorage,
                addTabUseCase = components.useCases.tabsUseCases.addTab,
                fragmentStore = homeFragmentStore,
                navController = findNavController(),
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                showDeleteCollectionPrompt = ::showDeleteCollectionPrompt,
                showTabTray = ::openTabTray,
                handleSwipedItemDeletionCancel = ::handleSwipedItemDeletionCancel
            )
        )
        updateLayout(view)
        sessionControlView = SessionControlView(
            view.sessionControlRecyclerView,
            sessionControlInteractor,
            homeViewModel,
            requireComponents.core.store.state.normalTabs.isNotEmpty()
        )

        updateSessionControlView(view)

        activity.themeManager.applyStatusBarTheme(activity)
        return view
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

                view.bottom_bar.background = resources.getDrawable(
                    ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, requireContext()),
                    null
                )

                view.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    topMargin = HEADER_MARGIN.dpToPx(resources.displayMetrics)
                }
            }
            ToolbarPosition.BOTTOM -> {
            }
        }
    }

    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        FragmentPreDrawManager(this).execute {
            val homeViewModel: HomeScreenViewModel by activityViewModels {
                ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
            }
            homeViewModel.layoutManagerState?.also { parcelable ->
                sessionControlView!!.view.layoutManager?.onRestoreInstanceState(parcelable)
            }
            homeViewModel.layoutManagerState = null

            // We have to delay so that the keyboard collapses and the view is resized before the
            // animation from SearchFragment happens
            delay(ANIMATION_DELAY)
        }

        viewLifecycleOwner.lifecycleScope.launch(IO) {
            // This is necessary due to a bug in viewLifecycleOwner. See:
            // https://github.com/mozilla-mobile/android-components/blob/master/components/lib/state/src/main/java/mozilla/components/lib/state/ext/Fragment.kt#L32-L56
            // TODO remove when viewLifecycleOwner is fixed
            val context = context ?: return@launch

            val iconSize =
                context.resources.getDimensionPixelSize(R.dimen.preference_icon_drawable_size)

            val searchEngine = context.components.search.provider.getDefaultEngine(context)
            val searchIcon = BitmapDrawable(context.resources, searchEngine.icon)
            searchIcon.setBounds(0, 0, iconSize, iconSize)

            withContext(Main) {
                search_engine_icon?.setImageDrawable(searchIcon)
            }
        }

        createHomeMenu(requireContext(), WeakReference(view.menuButton))
        view.tab_button.setOnLongClickListener {
            createTabCounterMenu(requireContext()).show(view.tab_button)
            true
        }

        view.menuButton.setColorFilter(
            ContextCompat.getColor(
                requireContext(),
                ThemeManager.resolveAttribute(R.attr.primaryText, requireContext())
            )
        )

        view.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        view.toolbar_wrapper.setOnClickListener {
            hideOnboardingIfNeeded()
            navigateToSearch()
            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        view.toolbar_wrapper.setOnLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(view),
                handlePasteAndGo = sessionControlInteractor::onPasteAndGo,
                handlePaste = sessionControlInteractor::onPaste,
                copyVisible = false
            )
            true
        }

        view.tab_button.setOnClickListener {
            openTabTray()
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

        // We call this onLayout so that the bottom bar width is correctly set for us to center
        // the CFR in.
        view.toolbar_wrapper.doOnLayout {
            if (!browsingModeManager.mode.isPrivate) {
                SearchWidgetCFR(
                    context = view.context,
                    settings = view.context.settings(),
                    metrics = view.context.components.analytics.metrics
                ) {
                    view.toolbar_wrapper
                }.displayIfNecessary()
            }
        }

        val args by navArgs<HomeFragmentArgs>()

        if (view.context.settings().accessibilityServicesEnabled &&
            args.focusOnAddressBar
        ) {
            // We cannot put this in the fragment_home.xml file as it breaks tests
            view.toolbar_wrapper.isFocusableInTouchMode = true
            viewLifecycleOwner.lifecycleScope.launch {
                view.toolbar_wrapper?.requestFocus()
                view.toolbar_wrapper?.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED)
            }
        }

        if (browsingModeManager.mode.isPrivate) {
            requireActivity().window.addFlags(FLAG_SECURE)
        } else {
            requireActivity().window.clearFlags(FLAG_SECURE)
        }

        consumeFrom(requireComponents.core.store) {
            updateTabCounter(it)
        }

        bundleArgs.getString(SESSION_TO_DELETE)?.also {
            if (it == ALL_NORMAL_TABS || it == ALL_PRIVATE_TABS) {
                removeAllTabsAndShowSnackbar(it)
            } else {
                removeTabAndShowSnackbar(it)
            }
        }

        updateTabCounter(requireComponents.core.store.state)

        if (args.focusOnAddressBar && requireContext().settings().useNewSearchExperience) {
            navigateToSearch()
        }
    }

    private fun removeAllTabsAndShowSnackbar(sessionCode: String) {
        val tabs = sessionManager.sessionsOfType(private = sessionCode == ALL_PRIVATE_TABS).toList()
        val selectedIndex = sessionManager
            .selectedSession?.let { sessionManager.sessions.indexOf(it) } ?: 0

        val snapshot = tabs
            .map(sessionManager::createSessionSnapshot)
            .map {
                it.copy(
                    engineSession = null,
                    engineSessionState = it.engineSession?.saveState()
                )
            }
            .let { SessionManager.Snapshot(it, selectedIndex) }

        tabs.forEach {
            sessionManager.remove(it)
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
                sessionManager.restore(snapshot)
            },
            operation = { },
            anchorView = snackbarAnchorView
        )
    }

    private fun removeTabAndShowSnackbar(sessionId: String) {
        sessionManager.findSessionById(sessionId)?.let { session ->
            val snapshot = sessionManager.createSessionSnapshot(session)
            val state = snapshot.engineSession?.saveState()
            val isSelected =
                session.id == requireComponents.core.store.state.selectedTabId ?: false

            sessionManager.remove(session)

            val snackbarMessage = if (snapshot.session.private) {
                requireContext().getString(R.string.snackbar_private_tab_closed)
            } else {
                requireContext().getString(R.string.snackbar_tab_closed)
            }

            viewLifecycleOwner.lifecycleScope.allowUndo(
                requireView(),
                snackbarMessage,
                requireContext().getString(R.string.snackbar_deleted_undo),
                {
                    sessionManager.add(
                        snapshot.session,
                        isSelected,
                        engineSessionState = state
                    )
                    findNavController().navigate(
                        HomeFragmentDirections.actionGlobalBrowser(null)
                    )
                },
                operation = { },
                anchorView = snackbarAnchorView
            )
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _sessionControlInteractor = null
        sessionControlView = null
        bundleArgs.clear()
        requireActivity().window.clearFlags(FLAG_SECURE)
    }

    override fun onStart() {
        super.onStart()
        subscribeToTabCollections()
        subscribeToTopSites()

        val context = requireContext()
        val components = context.components

        homeFragmentStore.dispatch(
            HomeFragmentAction.Change(
                collections = components.core.tabCollectionStorage.cachedTabCollections,
                mode = currentMode.getCurrentMode(),
                topSites = components.core.topSiteStorage.cachedTopSites,
                tip = FenixTipManager(listOf(MigrationTipProvider(requireContext()))).getTip()
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
            requireComponents.backgroundServices.accountManager.register(object : AccountObserver {
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
            }, owner = this@HomeFragment.viewLifecycleOwner)
        }

        if (context.settings().showPrivateModeContextualFeatureRecommender &&
            browsingModeManager.mode.isPrivate
        ) {
            recommendPrivateBrowsingShortcut()
        }

        // We only want this observer live just before we navigate away to the collection creation screen
        requireComponents.core.tabCollectionStorage.unregister(collectionStorageObserver)
    }

    private fun dispatchModeChanges(mode: Mode) {
        if (mode != Mode.fromBrowsingMode(browsingModeManager.mode)) {
            homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(mode))
        }
    }

    private fun showDeleteCollectionPrompt(
        tabCollection: TabCollection,
        title: String?,
        message: String,
        wasSwiped: Boolean,
        handleSwipedItemDeletionCancel: () -> Unit
    ) {
        val context = context ?: return
        AlertDialog.Builder(context).apply {
            setTitle(title)
            setMessage(message)
            setNegativeButton(R.string.tab_collection_dialog_negative) { dialog: DialogInterface, _ ->
                if (wasSwiped) {
                    handleSwipedItemDeletionCancel()
                }
                dialog.cancel()
            }
            setPositiveButton(R.string.tab_collection_dialog_positive) { dialog: DialogInterface, _ ->
                viewLifecycleOwner.lifecycleScope.launch(IO) {
                    context.components.core.tabCollectionStorage.removeCollection(tabCollection)
                    context.components.analytics.metrics.track(Event.CollectionRemoved)
                }.invokeOnCompletion {
                    dialog.dismiss()
                }
            }
            create()
        }.show()
    }

    override fun onStop() {
        super.onStop()
        val homeViewModel: HomeScreenViewModel by activityViewModels {
            ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
        }
        homeViewModel.layoutManagerState =
            sessionControlView!!.view.layoutManager?.onSaveInstanceState()
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
        context?.let {
            val layout = LayoutInflater.from(it)
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
                privateBrowsingRecommend.showAsDropDown(
                    privateBrowsingButton, 0, CFR_Y_OFFSET, Gravity.TOP or Gravity.END
                )
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
        navigateToSearch()
    }

    private fun navigateToSearch() {
        val directions = if (requireContext().settings().useNewSearchExperience) {
            HomeFragmentDirections.actionGlobalSearchDialog(
                sessionId = null
            )
        } else {
            HomeFragmentDirections.actionGlobalSearch(
                sessionId = null
            )
        }

        nav(R.id.homeFragment, directions, getToolbarNavOptions(requireContext()))
    }

    private fun openInNormalTab(url: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromHome
        )
    }

    private fun createTabCounterMenu(context: Context): BrowserMenu {
        val primaryTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context)
        val isPrivate = (activity as HomeActivity).browsingModeManager.mode == BrowsingMode.Private
        val menuItems = listOf(
            BrowserMenuImageText(
                label = context.getString(
                    if (isPrivate) {
                        R.string.browser_menu_new_tab
                    } else {
                        R.string.home_screen_shortcut_open_new_private_tab_2
                    }
                ),
                imageResource = if (isPrivate) {
                    R.drawable.ic_new
                } else {
                    R.drawable.ic_private_browsing
                },
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                requireComponents.analytics.metrics.track(
                    Event.TabCounterMenuItemTapped(
                        if (isPrivate) {
                            Event.TabCounterMenuItemTapped.Item.NEW_TAB
                        } else {
                            Event.TabCounterMenuItemTapped.Item.NEW_PRIVATE_TAB
                        }
                    )
                )
                (activity as HomeActivity).browsingModeManager.mode =
                    BrowsingMode.fromBoolean(!isPrivate)
            }
        )
        return BrowserMenuBuilder(menuItems).build(context)
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
                    }
                    HomeMenu.Item.SyncedTabs -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalSyncedTabsFragment()
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
                    HomeMenu.Item.Sync -> {
                        hideOnboardingIfNeeded()
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalAccountProblemFragment()
                        )
                    }
                    HomeMenu.Item.AddonsManager -> {
                        nav(
                            R.id.homeFragment,
                            HomeFragmentDirections.actionGlobalAddonsManagementFragment()
                        )
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

    private fun subscribeToTopSites(): Observer<List<TopSite>> {
        return Observer<List<TopSite>> { topSites ->
            requireComponents.core.topSiteStorage.cachedTopSites = topSites
            context?.settings()?.preferences?.edit()
                ?.putInt(getString(R.string.pref_key_top_sites_size), topSites.size)?.apply()
            homeFragmentStore.dispatch(HomeFragmentAction.TopSitesChange(topSites))
        }.also { observer ->
            requireComponents.core.topSiteStorage.getTopSites().observe(this, observer)
        }
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun scrollAndAnimateCollection(
        changedCollection: TabCollection? = null
    ) {
        if (view != null) {
            viewLifecycleOwner.lifecycleScope.launch {
                val recyclerView = sessionControlView!!.view
                delay(ANIM_SCROLL_DELAY)
                val tabsSize = store.state
                    .getNormalOrPrivateTabs(browsingModeManager.mode.isPrivate)
                    .size

                var indexOfCollection = tabsSize + NON_TAB_ITEM_NUM
                changedCollection?.let { changedCollection ->
                    requireComponents.core.tabCollectionStorage.cachedTabCollections
                        .filterIndexed { index, tabCollection ->
                            if (tabCollection.id == changedCollection.id) {
                                indexOfCollection = tabsSize + NON_TAB_ITEM_NUM + index
                                return@filterIndexed true
                            }
                            false
                        }
                }
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
                                animateCollection(indexOfCollection)
                                recyclerView.removeOnScrollListener(this)
                            }
                        }
                    }
                    recyclerView.addOnScrollListener(onScrollListener)
                    recyclerView.smoothScrollToPosition(indexOfCollection)
                } else {
                    animateCollection(indexOfCollection)
                }
            }
        }
    }

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
            showSavedSnackbar()
        }
    }

    private fun showSavedSnackbar() {
        viewLifecycleOwner.lifecycleScope.launch {
            delay(ANIM_SNACKBAR_DELAY)
            view?.let { view ->
                FenixSnackbar.make(
                    view = view,
                    duration = Snackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = false
                )
                    .setText(view.context.getString(R.string.create_collection_tabs_saved_new_collection))
                    .setAnchorView(snackbarAnchorView)
                    .show()
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

    private fun openTabTray() {
        findNavController().nav(
            R.id.homeFragment,
            HomeFragmentDirections.actionGlobalTabTrayDialogFragment()
        )
    }

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
        private const val SESSION_TO_DELETE = "session_to_delete"
        private const val ANIMATION_DELAY = 100L

        private const val NON_TAB_ITEM_NUM = 3
        private const val ANIM_SCROLL_DELAY = 100L
        private const val ANIM_ON_SCREEN_DELAY = 200L
        private const val FADE_ANIM_DURATION = 150L
        private const val ANIM_SNACKBAR_DELAY = 100L
        private const val CFR_WIDTH_DIVIDER = 1.7
        private const val CFR_Y_OFFSET = -20

        // Layout
        private const val HEADER_MARGIN = 60
    }
}

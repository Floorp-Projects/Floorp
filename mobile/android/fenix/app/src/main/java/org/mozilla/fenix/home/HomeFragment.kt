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
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.PopupWindow
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.constraintlayout.widget.ConstraintSet.BOTTOM
import androidx.constraintlayout.widget.ConstraintSet.PARENT_ID
import androidx.constraintlayout.widget.ConstraintSet.TOP
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.doOnLayout
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.support.ktx.android.os.resetAfter
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
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.hideToolbar
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.home.sessioncontrol.SessionControlView
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.MozillaPage.PRIVATE_NOTICE
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.HELP
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.tabtray.TabTrayDialogFragment
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.FragmentPreDrawManager
import org.mozilla.fenix.whatsnew.WhatsNew
import java.lang.ref.WeakReference
import kotlin.math.abs
import kotlin.math.min

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
    private val homeViewModel: HomeScreenViewModel by viewModels {
        ViewModelProvider.AndroidViewModelFactory(requireActivity().application)
    }

    private val snackbarAnchorView: View?
        get() {
            return if (requireContext().settings().shouldUseBottomToolbar) {
                toolbarLayout
            } else {
                null
            }
        }

    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager
    private var homeAppBarOffset = 0

    private val collectionStorageObserver = object : TabCollectionStorage.Observer {
        override fun onCollectionCreated(title: String, sessions: List<Session>) {
            scrollAndAnimateCollection(sessions.size)
        }

        override fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) {
            scrollAndAnimateCollection(sessions.size, tabCollection)
        }

        override fun onCollectionRenamed(tabCollection: TabCollection, title: String) {
            showRenamedSnackbar()
        }
    }

    private val sessionManager: SessionManager
        get() = requireComponents.core.sessionManager

    private lateinit var homeAppBarOffSetListener: AppBarLayout.OnOffsetChangedListener
    private val onboarding by lazy { FenixOnboarding(requireContext()) }
    private lateinit var homeFragmentStore: HomeFragmentStore
    private var _sessionControlInteractor: SessionControlInteractor? = null
    protected val sessionControlInteractor: SessionControlInteractor
        get() = _sessionControlInteractor!!

    private var sessionControlView: SessionControlView? = null
    private lateinit var currentMode: CurrentMode

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
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

        currentMode = CurrentMode(
            view.context,
            onboarding,
            browsingModeManager,
            ::dispatchModeChanges
        )

        homeFragmentStore = StoreProvider.get(this) {
            HomeFragmentStore(
                HomeFragmentState(
                    collections = requireComponents.core.tabCollectionStorage.cachedTabCollections,
                    expandedCollections = emptySet(),
                    mode = currentMode.getCurrentMode(),
                    topSites = StrictMode.allowThreadDiskReads().resetAfter {
                        requireComponents.core.topSiteStorage.cachedTopSites
                    },
                    tip = FenixTipManager(listOf(MigrationTipProvider(requireContext()))).getTip()
                )
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            DefaultSessionControlController(
                activity = activity,
                fragmentStore = homeFragmentStore,
                navController = findNavController(),
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
                getListOfTabs = ::getListOfTabs,
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                showDeleteCollectionPrompt = ::showDeleteCollectionPrompt,
                openSettingsScreen = ::openSettingsScreen,
                openWhatsNewLink = { openInNormalTab(SupportUtils.getWhatsNewUrl(activity)) },
                openPrivacyNotice = { openInNormalTab(SupportUtils.getMozillaPageUrl(PRIVATE_NOTICE)) },
                showTabTray = ::openTabTray
            )
        )
        updateLayout(view)
        setOffset(view)
        sessionControlView = SessionControlView(
            view.sessionControlRecyclerView,
            sessionControlInteractor,
            homeViewModel
        )
        activity.themeManager.applyStatusBarTheme(activity)

        view.consumeFrom(homeFragmentStore, viewLifecycleOwner) {
            sessionControlView?.update(it)
        }

        view.consumeFrom(requireComponents.core.store, viewLifecycleOwner) {
            val tabCount = if (currentMode.getCurrentMode() == Mode.Normal) {
                it.normalTabs.size
            } else {
                it.privateTabs.size
            }

            view.tab_button.setCountWithAnimation(tabCount)
        }

        return view
    }

    private fun updateLayout(view: View) {
        val shouldUseBottomToolbar = view.context.settings().shouldUseBottomToolbar

        if (!shouldUseBottomToolbar) {
            view.toolbarLayout.layoutParams = CoordinatorLayout.LayoutParams(
                ConstraintLayout.LayoutParams.MATCH_PARENT,
                ConstraintLayout.LayoutParams.WRAP_CONTENT
            )
                .apply {
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

            createNewAppBarListener(HEADER_MARGIN.dpToPx(resources.displayMetrics).toFloat())
            view.homeAppBar.addOnOffsetChangedListener(
                homeAppBarOffSetListener
            )
        } else {
            createNewAppBarListener(0F)
            view.homeAppBar.addOnOffsetChangedListener(
                homeAppBarOffSetListener
            )
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
                SearchWidgetCFR(view.context) { view.toolbar_wrapper }.displayIfNecessary()
            }
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _sessionControlInteractor = null
        sessionControlView = null
        requireView().homeAppBar.removeOnOffsetChangedListener(homeAppBarOffSetListener)
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

    private fun showDeleteCollectionPrompt(tabCollection: TabCollection) {
        val context = context ?: return
        AlertDialog.Builder(context).apply {
            val message =
                context.getString(R.string.tab_collection_dialog_message, tabCollection.title)
            setMessage(message)
            setNegativeButton(R.string.tab_collection_dialog_negative) { dialog: DialogInterface, _ ->
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
        calculateNewOffset()
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
        val directions = HomeFragmentDirections.actionGlobalSearch(
            sessionId = null
        )

        nav(R.id.homeFragment, directions, getToolbarNavOptions(requireContext()))
    }

    private fun openSettingsScreen() {
        val directions = HomeFragmentDirections.actionGlobalPrivateBrowsingFragment()
        nav(R.id.homeFragment, directions)
    }

    private fun openInNormalTab(url: String) {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromHome
        )
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

    private fun getListOfSessions(private: Boolean = browsingModeManager.mode.isPrivate): List<Session> {
        return sessionManager.sessionsOfType(private = private)
            .toList()
    }

    private fun getListOfTabs(): List<Tab> {
        return getListOfSessions().toTabs()
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun scrollAndAnimateCollection(
        tabsAddedToCollectionSize: Int,
        changedCollection: TabCollection? = null
    ) {
        if (view != null) {
            viewLifecycleOwner.lifecycleScope.launch {
                val recyclerView = sessionControlView!!.view
                delay(ANIM_SCROLL_DELAY)
                val tabsSize = getListOfSessions().size

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
                                animateCollection(tabsAddedToCollectionSize, indexOfCollection)
                                recyclerView.removeOnScrollListener(this)
                            }
                        }
                    }
                    recyclerView.addOnScrollListener(onScrollListener)
                    recyclerView.smoothScrollToPosition(indexOfCollection)
                } else {
                    animateCollection(tabsAddedToCollectionSize, indexOfCollection)
                }
            }
        }
    }

    private fun animateCollection(addedTabsSize: Int, indexOfCollection: Int) {
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
            showSavedSnackbar(addedTabsSize)
        }
    }

    private fun showSavedSnackbar(tabSize: Int) {
        viewLifecycleOwner.lifecycleScope.launch {
            delay(ANIM_SNACKBAR_DELAY)
            view?.let { view ->
                @StringRes
                val stringRes = if (tabSize > 1) {
                    R.string.create_collection_tabs_saved
                } else {
                    R.string.create_collection_tab_saved
                }
                FenixSnackbar.make(
                    view = view,
                    duration = Snackbar.LENGTH_LONG,
                    isDisplayedWithBrowserToolbar = false
                )
                    .setText(view.context.getString(stringRes))
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

    private fun List<Session>.toTabs(): List<Tab> {
        val selected = sessionManager.selectedSession

        return map {
            it.toTab(requireContext(), it == selected)
        }
    }

    private fun calculateNewOffset() {
        homeAppBarOffset = ((homeAppBar.layoutParams as CoordinatorLayout.LayoutParams)
            .behavior as AppBarLayout.Behavior).topAndBottomOffset
    }

    private fun setOffset(currentView: View) {
        if (homeAppBarOffset <= 0) {
            (currentView.homeAppBar.layoutParams as CoordinatorLayout.LayoutParams)
                .behavior = AppBarLayout.Behavior().apply {
                topAndBottomOffset = this@HomeFragment.homeAppBarOffset
            }
        } else {
            currentView.homeAppBar.setExpanded(false)
        }
    }

    private fun createNewAppBarListener(margin: Float) {
        homeAppBarOffSetListener =
            AppBarLayout.OnOffsetChangedListener { appBarLayout, verticalOffset ->
                val reduceScrollRanged = appBarLayout.totalScrollRange.toFloat() - margin
                appBarLayout.alpha = 1.0f - abs(verticalOffset / reduceScrollRanged)
            }
    }

    private fun openTabTray() {
        TabTrayDialogFragment.show(parentFragmentManager)
    }

    companion object {
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

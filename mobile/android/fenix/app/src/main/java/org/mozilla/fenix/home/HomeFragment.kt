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
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.Observer
import androidx.lifecycle.OnLifecycleEvent
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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.MediaState.State.PLAYING
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.addons.runIfFragmentIsAttached
import org.mozilla.fenix.browser.BrowserAnimator.Companion.getToolbarNavOptions
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
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
import org.mozilla.fenix.home.sessioncontrol.AdapterItem
import org.mozilla.fenix.home.sessioncontrol.DefaultSessionControlController
import org.mozilla.fenix.home.sessioncontrol.SessionControlAdapter
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
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.whatsnew.WhatsNew
import java.lang.ref.WeakReference
import kotlin.math.abs
import kotlin.math.min

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
    private val homeViewModel: HomeScreenViewModel by viewModels {
        ViewModelProvider.AndroidViewModelFactory(requireActivity().application)
    }

    private val sharedViewModel: SharedViewModel by activityViewModels()

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
    private val singleSessionObserver = object : Session.Observer {
        override fun onTitleChanged(session: Session, title: String) {
            if (deleteAllSessionsJob == null) emitSessionChanges()
        }
    }

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

    var deleteAllSessionsJob: (suspend () -> Unit)? = null
    private var pendingSessionDeletion: PendingSessionDeletion? = null

    data class PendingSessionDeletion(val deletionJob: (suspend () -> Unit), val sessionId: String)

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
        if (!onboarding.userHasBeenOnboarded()) {
            requireComponents.analytics.metrics.track(Event.OpenedAppFirstRun)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_home, container, false)
        val activity = activity as HomeActivity

        val sessionObserver = BrowserSessionsObserver(
            sessionManager,
            requireComponents.core.store,
            singleSessionObserver
        ) {
            emitSessionChanges()
        }

        viewLifecycleOwner.lifecycle.addObserver(sessionObserver)

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
                    tabs = emptyList(),
                    topSites = requireComponents.core.topSiteStorage.cachedTopSites,
                    tip = FenixTipManager(listOf(MigrationTipProvider(requireContext()))).getTip()
                )
            )
        }

        _sessionControlInteractor = SessionControlInteractor(
            DefaultSessionControlController(
                store = requireComponents.core.store,
                activity = activity,
                fragmentStore = homeFragmentStore,
                navController = findNavController(),
                browsingModeManager = browsingModeManager,
                viewLifecycleScope = viewLifecycleOwner.lifecycleScope,
                closeTab = ::closeTab,
                closeAllTabs = ::closeAllTabs,
                getListOfTabs = ::getListOfTabs,
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                invokePendingDeleteJobs = ::invokePendingDeleteJobs,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                scrollToTheTop = ::scrollToTheTop,
                showDeleteCollectionPrompt = ::showDeleteCollectionPrompt,
                openSettingsScreen = ::openSettingsScreen,
                openSearchScreen = ::navigateToSearch,
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

            if (context?.settings()?.useNewTabTray == true) {
                view.tab_button.setCountWithAnimation(it.tabs.size)
            }
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

        view.menuButton.setColorFilter(ContextCompat.getColor(
            requireContext(),
            ThemeManager.resolveAttribute(R.attr.primaryText, requireContext())
        ))

        view.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        view.toolbar_wrapper.setOnClickListener {
            invokePendingDeleteJobs()
            hideOnboardingIfNeeded()
            navigateToSearch()
            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        view.add_tab_button.setOnClickListener {
            invokePendingDeleteJobs()
            hideOnboardingIfNeeded()
            navigateToSearch()
        }

        view.tab_button.setOnClickListener {
            openTabTray()
        }

        PrivateBrowsingButtonView(
            privateBrowsingButton,
            browsingModeManager
        ) { newMode ->
            invokePendingDeleteJobs()

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
                tabs = getListOfSessions().toTabs(),
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
                            FenixSnackbar.make(view = it,
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

    private fun closeTab(sessionId: String) {
        val deletionJob = pendingSessionDeletion?.deletionJob
        context?.let {
            if (sessionManager.findSessionById(sessionId)?.toTab(it)?.mediaState == PLAYING) {
                it.components.core.store.state.media.pauseIfPlaying()
            }
        }

        if (deletionJob == null) {
            removeTabWithUndo(sessionId, browsingModeManager.mode.isPrivate)
        } else {
            viewLifecycleOwner.lifecycleScope.launch {
                deletionJob.invoke()
            }.invokeOnCompletion {
                pendingSessionDeletion = null
                removeTabWithUndo(sessionId, browsingModeManager.mode.isPrivate)
            }
        }
    }

    private fun closeAllTabs(isPrivateMode: Boolean) {
        val deletionJob = pendingSessionDeletion?.deletionJob

        context?.let {
            sessionManager.sessionsOfType(private = isPrivateMode).forEach { session ->
                if (session.toTab(it).mediaState == PLAYING) {
                    it.components.core.store.state.media.pauseIfPlaying()
                }
            }
        }

        if (deletionJob == null) {
            removeAllTabsWithUndo(
                sessionManager.sessionsOfType(private = isPrivateMode),
                isPrivateMode
            )
        } else {
            viewLifecycleOwner.lifecycleScope.launch {
                deletionJob.invoke()
            }.invokeOnCompletion {
                pendingSessionDeletion = null
                removeAllTabsWithUndo(
                    sessionManager.sessionsOfType(private = isPrivateMode),
                    isPrivateMode
                )
            }
        }
    }

    private fun dispatchModeChanges(mode: Mode) {
        if (mode != Mode.fromBrowsingMode(browsingModeManager.mode)) {
            homeFragmentStore.dispatch(HomeFragmentAction.ModeChange(mode))
        }
    }

    private fun invokePendingDeleteJobs() {
        pendingSessionDeletion?.deletionJob?.let {
            viewLifecycleOwner.lifecycleScope.launch {
                it.invoke()
            }.invokeOnCompletion {
                pendingSessionDeletion = null
            }
        }

        deleteAllSessionsJob?.let {
            viewLifecycleOwner.lifecycleScope.launch {
                it.invoke()
            }.invokeOnCompletion {
                deleteAllSessionsJob = null
            }
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
        invokePendingDeleteJobs()
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
        if (sharedViewModel.shouldScrollToSelectedTab) {
            scrollToSelectedTab()
            sharedViewModel.shouldScrollToSelectedTab = false
        }

        requireContext().settings().useNewTabTray.also {
            view?.add_tab_button?.isVisible = !it
            view?.tab_button?.isVisible = it
        }
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
                    mode = currentMode.getCurrentMode(),
                    tabs = getListOfSessions().toTabs()
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
    private fun createHomeMenu(context: Context, menuButtonView: WeakReference<MenuButton>) = HomeMenu(
        this.viewLifecycleOwner,
        context,
        onItemTapped = {
            when (it) {
                HomeMenu.Item.Settings -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionGlobalSettingsFragment()
                    )
                }
                HomeMenu.Item.Bookmarks -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
                    )
                }
                HomeMenu.Item.History -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionGlobalHistoryFragment()
                    )
                }
                HomeMenu.Item.Help -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getSumoURLForTopic(context, HELP),
                        newTab = true,
                        from = BrowserDirection.FromHome
                    )
                }
                HomeMenu.Item.WhatsNew -> {
                    invokePendingDeleteJobs()
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
                        view?.let { view -> FenixSnackbar.make(
                            view = view,
                            isDisplayedWithBrowserToolbar = false
                        )
                        }
                    )
                }
                HomeMenu.Item.Sync -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionGlobalAccountProblemFragment()
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

    private fun removeAllTabsWithUndo(listOfSessionsToDelete: Sequence<Session>, private: Boolean) {
        homeFragmentStore.dispatch(HomeFragmentAction.TabsChange(emptyList()))
        listOfSessionsToDelete.forEach {
            requireComponents.core.pendingSessionDeletionManager.addSession(
                it.id
            )
        }

        val deleteOperation: (suspend () -> Unit) = {
            listOfSessionsToDelete.forEach {
                sessionManager.remove(it)
                requireComponents.core.pendingSessionDeletionManager.removeSession(it.id)
            }
        }
        deleteAllSessionsJob = deleteOperation

        val snackbarMessage = if (private) {
            getString(R.string.snackbar_private_tabs_closed)
        } else {
            getString(R.string.snackbar_tabs_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo), {
                listOfSessionsToDelete.forEach {
                    requireComponents.core.pendingSessionDeletionManager.removeSession(
                        it.id
                    )
                }
                if (private) {
                    requireComponents.analytics.metrics.track(Event.PrivateBrowsingSnackbarUndoTapped)
                }
                deleteAllSessionsJob = null
                emitSessionChanges()
            },
            operation = deleteOperation,
            anchorView = snackbarAnchorView
        )
    }

    private fun removeTabWithUndo(sessionId: String, private: Boolean) {
        val sessionManager = requireComponents.core.sessionManager
        requireComponents.core.pendingSessionDeletionManager.addSession(sessionId)
        val deleteOperation: (suspend () -> Unit) = {
            sessionManager.findSessionById(sessionId)
                ?.let { session ->
                    pendingSessionDeletion = null
                    sessionManager.remove(session)
                    requireComponents.core.pendingSessionDeletionManager.removeSession(sessionId)
                }
        }

        pendingSessionDeletion = PendingSessionDeletion(deleteOperation, sessionId)

        val snackbarMessage = if (private) {
            getString(R.string.snackbar_private_tab_closed)
        } else {
            getString(R.string.snackbar_tab_closed)
        }

        viewLifecycleOwner.lifecycleScope.allowUndo(
            requireView(),
            snackbarMessage,
            getString(R.string.snackbar_deleted_undo), {
                requireComponents.core.pendingSessionDeletionManager.removeSession(sessionId)
                pendingSessionDeletion = null
                emitSessionChanges()
            },
            operation = deleteOperation,
            anchorView = snackbarAnchorView
        )

        // Update the UI with the tab removed, but don't remove it from storage yet
        emitSessionChanges()
    }

    private fun emitSessionChanges() {
        runIfFragmentIsAttached {
            homeFragmentStore.dispatch(HomeFragmentAction.TabsChange(getListOfTabs()))
        }
    }

    private fun getListOfSessions(private: Boolean = browsingModeManager.mode.isPrivate): List<Session> {
        return sessionManager.sessionsOfType(private = private)
            .filter { session: Session -> session.id != pendingSessionDeletion?.sessionId }
            .toList()
    }

    private fun getListOfTabs(): List<Tab> {
        return getListOfSessions().toTabs()
    }

    private fun registerCollectionStorageObserver() {
        requireComponents.core.tabCollectionStorage.register(collectionStorageObserver, this)
    }

    private fun scrollToTheTop() {
        viewLifecycleOwner.lifecycleScope.launch(Main) {
            delay(ANIM_SCROLL_DELAY)
            sessionControlView!!.view.smoothScrollToPosition(0)
        }
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
                FenixSnackbar.make(view = view,
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

    private fun scrollToSelectedTab() {
        val position = (sessionControlView!!.view.adapter as SessionControlAdapter)
            .currentList.indexOfFirst {
            it is AdapterItem.TabItem && it.tab.selected == true
        }
        if (position > 0) {
            (sessionControlView!!.view.layoutManager as LinearLayoutManager)
                .scrollToPositionWithOffset(position, SELECTED_TAB_OFFSET)
        }
    }

    private fun share(tabs: List<Session>) {
        val data = tabs.map {
            ShareData(url = it.url, title = it.title)
        }
        val directions = HomeFragmentDirections.actionGlobalShareFragment(
            data = data.toTypedArray()
        )
        nav(R.id.homeFragment, directions)
    }

    private fun openTabTray() {
        invokePendingDeleteJobs()
        hideOnboardingIfNeeded()
        val tabTrayDialog = TabTrayDialogFragment()
        tabTrayDialog.show(parentFragmentManager, null)
        tabTrayDialog.interactor = object : TabTrayDialogFragment.Interactor {
            override fun onTabSelected(tab: mozilla.components.concept.tabstray.Tab) {
                tabTrayDialog.dismiss()
                (activity as HomeActivity).openToBrowser(BrowserDirection.FromHome)
            }

            override fun onNewTabTapped(private: Boolean) {
                (activity as HomeActivity).browsingModeManager.mode = BrowsingMode.fromBoolean(private)
                tabTrayDialog.dismiss()
            }

            override fun onShareTabsClicked(private: Boolean) {
                share(getListOfSessions(private))
            }

            override fun onCloseAllTabsClicked(private: Boolean) {
                val tabs = getListOfSessions(private)

                val selectedIndex = sessionManager
                    .selectedSession?.let { sessionManager.sessions.indexOf(it) } ?: 0

                val snapshot = tabs
                    .map(sessionManager::createSessionSnapshot)
                    .map { it.copy(engineSession = null, engineSessionState = it.engineSession?.saveState()) }
                    .let { SessionManager.Snapshot(it, selectedIndex) }

                tabs.forEach {
                    sessionManager.remove(it)
                }

                val isPrivate = (activity as HomeActivity).browsingModeManager.mode.isPrivate
                val snackbarMessage = if (isPrivate) {
                    getString(R.string.snackbar_private_tabs_closed)
                } else {
                    getString(R.string.snackbar_tabs_closed)
                }

                viewLifecycleOwner.lifecycleScope.allowUndo(
                    tabTrayDialog.requireView(),
                    snackbarMessage,
                    getString(R.string.snackbar_deleted_undo),
                    {
                        sessionManager.restore(snapshot)
                    },
                    operation = { },
                    elevation = SNACKBAR_ELEVATION
                )
            }

            override fun onSaveToCollectionClicked() {
                val tabs = getListOfSessions(false)
                val tabIds = tabs.map { it.id }.toList().toTypedArray()
                val tabCollectionStorage = (activity as HomeActivity).components.core.tabCollectionStorage
                val navController = findNavController()

                val step = when {
                    // Show the SelectTabs fragment if there are multiple opened tabs to select which tabs
                    // you want to save to a collection.
                    tabs.size > 1 -> SaveCollectionStep.SelectTabs
                    // If there is an existing tab collection, show the SelectCollection fragment to save
                    // the selected tab to a collection of your choice.
                    tabCollectionStorage.cachedTabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
                    // Show the NameCollection fragment to create a new collection for the selected tab.
                    else -> SaveCollectionStep.NameCollection
                }

                if (navController.currentDestination?.id == R.id.collectionCreationFragment) return

                val directions = HomeFragmentDirections.actionHomeFragmentToCreateCollectionFragment(
                    tabIds = tabIds,
                    previousFragmentId = R.id.tabTrayFragment,
                    saveCollectionStep = step,
                    selectedTabIds = tabIds
                )
                navController.nav(R.id.homeFragment, directions)
            }
        }
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
        private const val SELECTED_TAB_OFFSET = 20

        // Layout
        private const val HEADER_MARGIN = 60

        private const val SNACKBAR_ELEVATION = 80f
    }
}

/**
 * Wrapper around sessions manager to observe changes in sessions.
 * Similar to [mozilla.components.browser.session.utils.AllSessionsObserver] but ignores CustomTab sessions.
 *
 * Call [onStart] to start receiving updates into [onChanged] callback.
 * Call [onStop] to stop receiving updates.
 *
 * @param manager [SessionManager] instance to subscribe to.
 * @param observer [Session.Observer] instance that will recieve updates.
 * @param onChanged callback that will be called when any of [SessionManager.Observer]'s events are fired.
 */
private class BrowserSessionsObserver(
    private val manager: SessionManager,
    private val store: BrowserStore,
    private val observer: Session.Observer,
    private val onChanged: () -> Unit
) : LifecycleObserver {
    private var scope: CoroutineScope? = null

    /**
     * Start observing
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        manager.register(managerObserver)
        subscribeToAll()

        scope = store.flowScoped { flow ->
            flow.ifChanged { it.media.aggregate }
                .collect { onChanged() }
        }
    }

    /**
     * Stop observing (will not receive updates till next [onStop] call)
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        scope?.cancel()
        manager.unregister(managerObserver)
        unsubscribeFromAll()
    }

    private fun subscribeToAll() {
        manager.sessions.forEach(::subscribeTo)
    }

    private fun unsubscribeFromAll() {
        manager.sessions.forEach(::unsubscribeFrom)
    }

    private fun subscribeTo(session: Session) {
        session.register(observer)
    }

    private fun unsubscribeFrom(session: Session) {
        session.unregister(observer)
    }

    private val managerObserver = object : SessionManager.Observer {
        override fun onSessionAdded(session: Session) {
            subscribeTo(session)
            onChanged()
        }

        override fun onSessionsRestored() {
            subscribeToAll()
            onChanged()
        }

        override fun onAllSessionsRemoved() {
            unsubscribeFromAll()
            onChanged()
        }

        override fun onSessionRemoved(session: Session) {
            unsubscribeFrom(session)
            onChanged()
        }

        override fun onSessionSelected(session: Session) {
            onChanged()
        }
    }
}

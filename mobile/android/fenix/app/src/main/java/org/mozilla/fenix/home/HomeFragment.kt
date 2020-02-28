/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.animation.Animator
import android.content.Context
import android.content.DialogInterface
import android.graphics.drawable.BitmapDrawable
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
import androidx.constraintlayout.widget.ConstraintSet.TOP
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.Observer
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.FragmentNavigator
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_IDLE
import androidx.transition.TransitionInflater
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.media.ext.getSession
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
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
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.FragmentPreDrawManager
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.whatsnew.WhatsNew
import kotlin.math.abs
import kotlin.math.min

@ExperimentalCoroutinesApi
@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {
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
    private lateinit var sessionControlInteractor: SessionControlInteractor
    private var sessionControlView: SessionControlView? = null
    private lateinit var currentMode: CurrentMode

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        postponeEnterTransition()
        sharedElementEnterTransition =
            TransitionInflater.from(context).inflateTransition(android.R.transition.move)
                .setDuration(SHARED_TRANSITION_MS)

        val sessionObserver = BrowserSessionsObserver(sessionManager, singleSessionObserver) {
            emitSessionChanges()
        }

        lifecycle.addObserver(sessionObserver)

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
                    topSites = requireComponents.core.topSiteStorage.cachedTopSites
                )
            )
        }

        sessionControlInteractor = SessionControlInteractor(
            DefaultSessionControlController(
                activity = activity,
                store = homeFragmentStore,
                navController = findNavController(),
                browsingModeManager = browsingModeManager,
                lifecycleScope = viewLifecycleOwner.lifecycleScope,
                closeTab = ::closeTab,
                closeAllTabs = ::closeAllTabs,
                getListOfTabs = ::getListOfTabs,
                hideOnboarding = ::hideOnboardingAndOpenSearch,
                invokePendingDeleteJobs = ::invokePendingDeleteJobs,
                registerCollectionStorageObserver = ::registerCollectionStorageObserver,
                scrollToTheTop = ::scrollToTheTop,
                showDeleteCollectionPrompt = ::showDeleteCollectionPrompt,
                openSettingsScreen = ::openSettingsScreen,
                openSearchScreen = ::navigateToSearch
            )
        )
        updateLayout(view)
        setOffset(view)
        sessionControlView = SessionControlView(
            homeFragmentStore,
            view.sessionControlRecyclerView, sessionControlInteractor
        )
        activity.themeManager.applyStatusBarTheme(activity)

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
                clear(view.bottomBarShadow.id, BOTTOM)
                connect(view.bottomBarShadow.id, TOP, view.bottom_bar.id, BOTTOM)
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

    @ExperimentalCoroutinesApi
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

        with(view.menuButton) {
            menuBuilder = createHomeMenu(context!!).menuBuilder

            val primaryTextColor = ContextCompat.getColor(
                context,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            )

            setColorFilter(primaryTextColor)

            // Immediately check for `What's new` highlight. If home items that change over time
            // are added, this will need to be called repeatedly.
            setHighlight(menuBuilder?.items?.getHighlight())
        }
        view.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        view.toolbar_wrapper.setOnClickListener {
            invokePendingDeleteJobs()
            hideOnboardingIfNeeded()
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(
                sessionId = null
            )
            val extras =
                FragmentNavigator.Extras.Builder()
                    .addSharedElement(toolbar_wrapper, "toolbar_wrapper_transition")
                    .build()
            nav(R.id.homeFragment, directions, extras)
            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        view.add_tab_button.setOnClickListener {
            invokePendingDeleteJobs()
            hideOnboardingIfNeeded()
            navigateToSearch()
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
    }

    override fun onDestroyView() {
        super.onDestroyView()
        sessionControlView = null
        view!!.homeAppBar.removeOnOffsetChangedListener(homeAppBarOffSetListener)
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
                topSites = components.core.topSiteStorage.cachedTopSites
            )
        )

        requireComponents.backgroundServices.accountManager.register(currentMode, owner = this)
        requireComponents.backgroundServices.accountManager.register(object : AccountObserver {
            override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                if (authType != AuthType.Existing) {
                    view?.let {
                        FenixSnackbar.make(it, Snackbar.LENGTH_SHORT)
                            .setText(it.context.getString(R.string.onboarding_firefox_account_sync_is_on))
                            .setAnchorView(toolbarLayout)
                            .show()
                    }
                }
            }
        }, owner = this)

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
        hideToolbar()
    }

    override fun onPause() {
        super.onPause()
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
        val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(
            sessionId = null
        )
        nav(R.id.homeFragment, directions)
    }

    private fun openSettingsScreen() {
        val directions = HomeFragmentDirections.actionHomeFragmentToPrivateBrowsingFragment()
        nav(R.id.homeFragment, directions)
    }

    @SuppressWarnings("ComplexMethod")
    private fun createHomeMenu(context: Context): HomeMenu {
        return HomeMenu(context) {
            when (it) {
                HomeMenu.Item.Settings -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionHomeFragmentToSettingsFragment()
                    )
                }
                HomeMenu.Item.Bookmarks -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionHomeFragmentToBookmarksFragment(BookmarkRoot.Mobile.id)
                    )
                }
                HomeMenu.Item.History -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionHomeFragmentToHistoryFragment()
                    )
                }
                HomeMenu.Item.Help -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getSumoURLForTopic(
                            context,
                            SupportUtils.SumoTopic.HELP
                        ),
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
                        lifecycleScope,
                        view?.let { view -> FenixSnackbar.makeWithToolbarPadding(view) }
                    )
                }
                HomeMenu.Item.Sync -> {
                    invokePendingDeleteJobs()
                    hideOnboardingIfNeeded()
                    nav(
                        R.id.homeFragment,
                        HomeFragmentDirections.actionHomeFragmentToAccountProblemFragment()
                    )
                }
            }
        }
    }

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
            view!!,
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
            view!!,
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
        homeFragmentStore.dispatch(HomeFragmentAction.TabsChange(getListOfTabs()))
    }

    private fun getListOfSessions(): List<Session> {
        return sessionManager.sessionsOfType(private = browsingModeManager.mode.isPrivate)
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
        lifecycleScope.launch(Main) {
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
                (viewHolder as? CollectionViewHolder)?.view?.findViewById<View>(R.id.selected_border)
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
                FenixSnackbar.make(view, Snackbar.LENGTH_LONG)
                    .setText(view.context.getString(stringRes))
                    .setAnchorView(snackbarAnchorView)
                    .show()
            }
        }
    }

    private fun showRenamedSnackbar() {
        view?.let { view ->
            val string = view.context.getString(R.string.snackbar_collection_renamed)
            FenixSnackbar.make(view, Snackbar.LENGTH_LONG)
                .setText(string)
                .setAnchorView(snackbarAnchorView)
                .show()
        }
    }

    private fun List<Session>.toTabs(): List<Tab> {
        val selected = sessionManager.selectedSession
        val mediaStateSession = MediaStateMachine.state.getSession()

        return this.map {
            val mediaState = if (mediaStateSession?.id == it.id) {
                MediaStateMachine.state
            } else {
                null
            }

            it.toTab(requireContext(), it == selected, mediaState)
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

    companion object {
        private const val NON_TAB_ITEM_NUM = 3
        private const val ANIM_SCROLL_DELAY = 100L
        private const val ANIM_ON_SCREEN_DELAY = 200L
        private const val FADE_ANIM_DURATION = 150L
        private const val ANIM_SNACKBAR_DELAY = 100L
        private const val SHARED_TRANSITION_MS = 200L
        private const val CFR_WIDTH_DIVIDER = 1.7
        private const val CFR_Y_OFFSET = -20

        // Layout
        private const val HEADER_MARGIN = 60
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
    private val observer: Session.Observer,
    private val onChanged: () -> Unit
) : LifecycleObserver {

    /**
     * Start observing
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onStart() {
        MediaStateMachine.register(managerObserver)
        manager.register(managerObserver)
        subscribeToAll()
    }

    /**
     * Stop observing (will not receive updates till next [onStop] call)
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onStop() {
        MediaStateMachine.unregister(managerObserver)
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

    private val managerObserver = object : SessionManager.Observer, MediaStateMachine.Observer {
        override fun onStateChanged(state: MediaState) {
            onChanged()
        }

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

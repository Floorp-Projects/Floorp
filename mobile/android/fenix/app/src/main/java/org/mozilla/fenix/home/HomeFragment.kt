/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.animation.Animator
import android.content.DialogInterface
import android.graphics.Bitmap
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
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
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
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Dispatchers.Main
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.feature.media.ext.getSession
import mozilla.components.feature.media.ext.pauseIfPlaying
import mozilla.components.feature.media.ext.playIfPaused
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.media.state.MediaStateMachine
import mozilla.components.feature.tab.collections.TabCollection
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.BOTTOM
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.END
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.START
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.TOP
import org.jetbrains.anko.constraint.layout.applyConstraintSet
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.sessionsOfType
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.home.sessioncontrol.CollectionAction
import org.mozilla.fenix.home.sessioncontrol.OnboardingAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.home.sessioncontrol.SessionControlComponent
import org.mozilla.fenix.home.sessioncontrol.SessionControlState
import org.mozilla.fenix.home.sessioncontrol.SessionControlViewModel
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.deletebrowsingdata.deleteAndQuit
import org.mozilla.fenix.share.ShareTab
import org.mozilla.fenix.utils.FragmentPreDrawManager
import org.mozilla.fenix.utils.allowUndo
import org.mozilla.fenix.whatsnew.WhatsNew

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment() {

    private val bus = ActionBusFactory.get(this)

    private val browsingModeManager get() = (activity as HomeActivity).browsingModeManager

    private val singleSessionObserver = object : Session.Observer {
        override fun onTitleChanged(session: Session, title: String) {
            if (deleteAllSessionsJob == null) emitSessionChanges()
        }

        override fun onIconChanged(session: Session, icon: Bitmap?) {
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

    private var homeMenu: HomeMenu? = null

    private val sessionManager: SessionManager
        get() = requireComponents.core.sessionManager

    var deleteAllSessionsJob: (suspend () -> Unit)? = null
    private var pendingSessionDeletion: PendingSessionDeletion? = null

    data class PendingSessionDeletion(val deletionJob: (suspend () -> Unit), val sessionId: String)

    private val onboarding by lazy { FenixOnboarding(requireContext()) }
    private lateinit var sessionControlComponent: SessionControlComponent
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

        currentMode = CurrentMode(
            view.context,
            onboarding,
            browsingModeManager,
            getManagedEmitter()
        )

        sessionControlComponent = SessionControlComponent(
            view.homeLayout,
            bus,
            FenixViewModelProvider.create(
                this,
                SessionControlViewModel::class.java
            ) {
                SessionControlViewModel(
                    SessionControlState(
                        emptyList(),
                        emptySet(),
                        requireComponents.core.tabCollectionStorage.cachedTabCollections,
                        currentMode.getCurrentMode()
                    )
                )
            }
        )

        view.homeLayout.applyConstraintSet {
            sessionControlComponent.view {
                connect(
                    TOP to BOTTOM of view.wordmark_spacer,
                    START to START of PARENT_ID,
                    END to END of PARENT_ID,
                    BOTTOM to TOP of view.bottom_bar
                )
            }
        }

        ActionBusFactory.get(this).logMergedObservables()
        val activity = activity as HomeActivity
        activity.themeManager.applyStatusBarTheme(activity)

        return view
    }

    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        FragmentPreDrawManager(this).execute {
            val homeViewModel: HomeScreenViewModel by activityViewModels {
                ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
            }
            homeViewModel.layoutManagerState?.also { parcelable ->
                sessionControlComponent.view.layoutManager?.onRestoreInstanceState(parcelable)
            }
            homeLayout?.progress = homeViewModel.motionLayoutProgress
            homeViewModel.layoutManagerState = null
        }

        setupHomeMenu()

        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
            val iconSize = resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()

            val searchEngine = requireComponents.search.searchEngineManager.getDefaultSearchEngineAsync(
                requireContext(),
                requireContext().settings().defaultSearchEngineName
            )
            val searchIcon = BitmapDrawable(resources, searchEngine.icon)
            searchIcon.setBounds(0, 0, iconSize, iconSize)

            withContext(Dispatchers.Main) {
                search_engine_icon?.setImageDrawable(searchIcon)
            }
        }

        with(view.menuButton) {
            var menu: PopupWindow? = null
            setOnClickListener {
                if (menu == null) {
                    menu = homeMenu?.menuBuilder?.build(requireContext())?.show(
                        anchor = it,
                        orientation = BrowserMenu.Orientation.DOWN,
                        onDismiss = { menu = null }
                    )
                } else {
                    menu?.dismiss()
                }
            }
        }
        view.toolbar.compoundDrawablePadding =
            view.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)
        view.toolbar_wrapper.setOnClickListener {
            invokePendingDeleteJobs()
            onboarding.finish()
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
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(
                sessionId = null
            )
            nav(R.id.homeFragment, directions)
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
                getManagedEmitter<SessionControlChange>().onNext(
                    SessionControlChange.ModeChange(Mode.fromBrowsingMode(newMode))
                )
            }
        }

        // We need the shadow to be above the components.
        bottomBarShadow.bringToFront()
    }

    override fun onDestroyView() {
        homeMenu = null
        super.onDestroyView()
    }

    override fun onResume() {
        super.onResume()

        getAutoDisposeObservable<SessionControlAction>()
            .subscribe {
                when (it) {
                    is SessionControlAction.Tab -> handleTabAction(it.action)
                    is SessionControlAction.Collection -> handleCollectionAction(it.action)
                    is SessionControlAction.Onboarding -> handleOnboardingAction(it.action)
                }
            }

        val context = requireContext()
        val components = context.components

        getManagedEmitter<SessionControlChange>().onNext(
            SessionControlChange.Change(
                tabs = getListOfSessions().toTabs(),
                mode = currentMode.getCurrentMode(),
                collections = components.core.tabCollectionStorage.cachedTabCollections
            )
        )

        (activity as AppCompatActivity).supportActionBar?.hide()

        requireComponents.backgroundServices.accountManager.register(currentMode, owner = this)
        requireComponents.backgroundServices.accountManager.register(object : AccountObserver {
            override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
                if (authType != AuthType.Existing) {
                    view?.let {
                        FenixSnackbar.make(it, Snackbar.LENGTH_SHORT)
                            .setText(it.context.getString(R.string.onboarding_firefox_account_sync_is_on))
                            .setAnchorView(bottom_bar)
                            .show()
                    }
                }
            }
        }, owner = this)

        if (context.settings().showPrivateModeContextualFeatureRecommender &&
            browsingModeManager.mode.isPrivate &&
            !PrivateShortcutCreateManager.doesPrivateBrowsingPinnedShortcutExist(context)) {
            recommendPrivateBrowsingShortcut()
        }
    }

    override fun onStart() {
        super.onStart()
        subscribeToTabCollections()

        // We only want this observer live just before we navigate away to the collection creation screen
        requireComponents.core.tabCollectionStorage.unregister(collectionStorageObserver)
    }

    private fun handleOnboardingAction(action: OnboardingAction) {
        Do exhaustive when (action) {
            is OnboardingAction.Finish -> {
                homeLayout?.progress = 0F
                hideOnboarding()
            }
        }
    }

    @SuppressWarnings("ComplexMethod", "LongMethod")
    private fun handleTabAction(action: TabAction) {
        Do exhaustive when (action) {
            is TabAction.SaveTabGroup -> {
                if (browsingModeManager.mode.isPrivate) return
                invokePendingDeleteJobs()
                saveTabToCollection(action.selectedTabSessionId)
            }
            is TabAction.Select -> {
                invokePendingDeleteJobs()
                val session = sessionManager.findSessionById(action.sessionId)
                sessionManager.select(session!!)
                val directions = HomeFragmentDirections.actionHomeFragmentToBrowserFragment(null)
                val extras =
                    FragmentNavigator.Extras.Builder()
                        .addSharedElement(
                            action.tabView,
                            "$TAB_ITEM_TRANSITION_NAME${action.sessionId}"
                        )
                        .build()
                nav(R.id.homeFragment, directions, extras)
            }
            is TabAction.Close -> {
                if (pendingSessionDeletion?.deletionJob == null) {
                    removeTabWithUndo(action.sessionId, browsingModeManager.mode.isPrivate)
                } else {
                    pendingSessionDeletion?.deletionJob?.let {
                        viewLifecycleOwner.lifecycleScope.launch {
                            it.invoke()
                        }.invokeOnCompletion {
                            pendingSessionDeletion = null
                            removeTabWithUndo(action.sessionId, browsingModeManager.mode.isPrivate)
                        }
                    }
                }
            }
            is TabAction.Share -> {
                invokePendingDeleteJobs()
                sessionManager.findSessionById(action.sessionId)?.let { session ->
                    share(session.url)
                }
            }
            is TabAction.PauseMedia -> {
                MediaStateMachine.state.pauseIfPlaying()
            }
            is TabAction.PlayMedia -> {
                MediaStateMachine.state.playIfPaused()
            }
            is TabAction.CloseAll -> {
                if (pendingSessionDeletion?.deletionJob == null) {
                    removeAllTabsWithUndo(
                        sessionManager.sessionsOfType(private = action.private),
                        action.private
                    )
                } else {
                    pendingSessionDeletion?.deletionJob?.let {
                        viewLifecycleOwner.lifecycleScope.launch {
                            it.invoke()
                        }.invokeOnCompletion {
                            pendingSessionDeletion = null
                            removeAllTabsWithUndo(
                                sessionManager.sessionsOfType(private = action.private),
                                action.private
                            )
                        }
                    }
                }
            }
            is TabAction.PrivateBrowsingLearnMore -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                        (SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS),
                    newTab = true,
                    from = BrowserDirection.FromHome
                )
            }

            is TabAction.ShareTabs -> {
                invokePendingDeleteJobs()
                val shareTabs = sessionManager
                    .sessionsOfType(private = browsingModeManager.mode.isPrivate)
                    .map { ShareTab(it.url, it.title) }
                    .toList()
                share(tabs = shareTabs)
            }
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

    private fun createDeleteCollectionPrompt(tabCollection: TabCollection) {
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

    @SuppressWarnings("LongMethod")
    private fun handleCollectionAction(action: CollectionAction) {
        when (action) {
            is CollectionAction.Expand -> {
                getManagedEmitter<SessionControlChange>()
                    .onNext(SessionControlChange.ExpansionChange(action.collection, true))
            }
            is CollectionAction.Collapse -> {
                getManagedEmitter<SessionControlChange>()
                    .onNext(SessionControlChange.ExpansionChange(action.collection, false))
            }
            is CollectionAction.Delete -> {
                createDeleteCollectionPrompt(action.collection)
            }
            is CollectionAction.AddTab -> {
                requireComponents.analytics.metrics.track(Event.CollectionAddTabPressed)
                showCollectionCreationFragment(
                    step = SaveCollectionStep.SelectTabs,
                    selectedTabCollectionId = action.collection.id
                )
            }
            is CollectionAction.Rename -> {
                showCollectionCreationFragment(
                    step = SaveCollectionStep.RenameCollection,
                    selectedTabCollectionId = action.collection.id
                )
                requireComponents.analytics.metrics.track(Event.CollectionRenamePressed)
            }
            is CollectionAction.OpenTab -> {
                invokePendingDeleteJobs()

                val context = requireContext()
                val components = context.components

                val session = action.tab.restore(
                    context = context,
                    engine = components.core.engine,
                    tab = action.tab,
                    restoreSessionId = false
                )
                if (session == null) {
                    // We were unable to create a snapshot, so just load the tab instead
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = action.tab.url,
                        newTab = true,
                        from = BrowserDirection.FromHome
                    )
                } else {
                    components.core.sessionManager.add(
                        session,
                        true
                    )
                    (activity as HomeActivity).openToBrowser(BrowserDirection.FromHome)
                }
                components.analytics.metrics.track(Event.CollectionTabRestored)
            }
            is CollectionAction.OpenTabs -> {
                invokePendingDeleteJobs()

                val context = requireContext()
                val components = context.components

                action.collection.tabs.reversed().forEach {
                    val session = it.restore(
                        context = context,
                        engine = components.core.engine,
                        tab = it,
                        restoreSessionId = false
                    )
                    if (session == null) {
                        // We were unable to create a snapshot, so just load the tab instead
                        components.useCases.tabsUseCases.addTab.invoke(it.url)
                    } else {
                        components.core.sessionManager.add(
                            session,
                            context.components.core.sessionManager.selectedSession == null
                        )
                    }
                }
                viewLifecycleOwner.lifecycleScope.launch(Main) {
                    delay(ANIM_SCROLL_DELAY)
                    sessionControlComponent.view.smoothScrollToPosition(0)
                }
                components.analytics.metrics.track(Event.CollectionAllTabsRestored)
            }
            is CollectionAction.ShareTabs -> {
                val shareTabs = action.collection.tabs.map { ShareTab(it.url, it.title) }
                share(tabs = shareTabs)
                requireComponents.analytics.metrics.track(Event.CollectionShared)
            }
            is CollectionAction.RemoveTab -> {
                viewLifecycleOwner.lifecycleScope.launch(IO) {
                    requireComponents.core.tabCollectionStorage.removeTabFromCollection(
                        action.collection,
                        action.tab
                    )
                }
                requireComponents.analytics.metrics.track(Event.CollectionTabRemoved)
            }
        }
    }

    override fun onPause() {
        invokePendingDeleteJobs()
        super.onPause()
        val homeViewModel: HomeScreenViewModel by activityViewModels {
            ViewModelProvider.NewInstanceFactory() // this is a workaround for #4652
        }
        homeViewModel.layoutManagerState =
            sessionControlComponent.view.layoutManager?.onSaveInstanceState()
        homeViewModel.motionLayoutProgress = homeLayout?.progress ?: 0F
    }

    private fun recommendPrivateBrowsingShortcut() {
        context?.let {
            val layout = LayoutInflater.from(it)
                .inflate(R.layout.pbm_shortcut_popup, null)
            val trackingOnboarding =
                PopupWindow(
                    layout,
                    (resources.displayMetrics.widthPixels / CFR_WIDTH_DIVIDER).toInt(),
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    true
                )
            layout.findViewById<Button>(R.id.cfr_pos_button).apply {
                setOnClickListener {
                    context.metrics.track(Event.PrivateBrowsingAddShortcutCFR)
                    PrivateShortcutCreateManager.createPrivateShortcut(context)
                    trackingOnboarding.dismiss()
                }
            }
            layout.findViewById<Button>(R.id.cfr_neg_button).apply {
                setOnClickListener {
                    context.metrics.track(Event.PrivateBrowsingCancelCFR)
                    trackingOnboarding.dismiss()
                }
            }
            // We want to show the popup only after privateBrowsingButton is available.
            // Otherwise, we will encounter an activity token error.
            privateBrowsingButton.post {
                trackingOnboarding.showAsDropDown(privateBrowsingButton, 0, CFR_Y_OFFSET, Gravity.TOP or Gravity.END)
            }
        }
    }

    private fun hideOnboardingIfNeeded() {
        if (!onboarding.userHasBeenOnboarded()) hideOnboarding()
    }

    private fun hideOnboarding() {
        onboarding.finish()
        currentMode.emitModeChanges()
    }

    private fun setupHomeMenu() {
        val context = requireContext()
        homeMenu = HomeMenu(context) {
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
                    context.metrics.track(Event.WhatsNewTapped(Event.WhatsNewTapped.Source.HOME))
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getSumoURLForTopic(
                            context,
                            SupportUtils.SumoTopic.WHATS_NEW
                        ),
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
                        view?.let { view ->
                            FenixSnackbar.make(view, Snackbar.LENGTH_INDEFINITE)
                                .setText(view.context.getString(R.string.deleting_browsing_data_in_progress))
                                .setAnchorView(bottom_bar)
                        }
                    )
                }
            }
        }
    }

    private fun subscribeToTabCollections(): Observer<List<TabCollection>> {
        return Observer<List<TabCollection>> {
            requireComponents.core.tabCollectionStorage.cachedTabCollections = it
            getManagedEmitter<SessionControlChange>().onNext(
                SessionControlChange.CollectionsChange(
                    it
                )
            )
        }.also { observer ->
            requireComponents.core.tabCollectionStorage.getCollections().observe(this, observer)
        }
    }

    private fun removeAllTabsWithUndo(listOfSessionsToDelete: Sequence<Session>, private: Boolean) {
        val sessionManager = requireComponents.core.sessionManager

        getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.TabsChange(listOf()))

        val deleteOperation: (suspend () -> Unit) = {
            listOfSessionsToDelete.forEach {
                sessionManager.remove(it)
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
                if (private) {
                    requireComponents.analytics.metrics.track(Event.PrivateBrowsingSnackbarUndoTapped)
                }
                deleteAllSessionsJob = null
                emitSessionChanges()
            },
            operation = deleteOperation,
            anchorView = bottom_bar
        )
    }

    private fun removeTabWithUndo(sessionId: String, private: Boolean) {
        val sessionManager = requireComponents.core.sessionManager
        val deleteOperation: (suspend () -> Unit) = {
            sessionManager.findSessionById(sessionId)
                ?.let { session ->
                    pendingSessionDeletion = null
                    sessionManager.remove(session)
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
                pendingSessionDeletion = null
                emitSessionChanges()
            },
            operation = deleteOperation,
            anchorView = bottom_bar
        )

        // Update the UI with the tab removed, but don't remove it from storage yet
        emitSessionChanges()
    }

    private fun emitSessionChanges() {
        getManagedEmitter<SessionControlChange>().onNext(
            SessionControlChange.TabsChange(
                getListOfSessions().toTabs()
            )
        )
    }

    private fun getListOfSessions(): List<Session> {
        return sessionManager.sessionsOfType(private = browsingModeManager.mode.isPrivate)
            .filter { session: Session -> session.id != pendingSessionDeletion?.sessionId }
            .toList()
    }

    private fun showCollectionCreationFragment(
        step: SaveCollectionStep,
        selectedTabIds: Array<String>? = null,
        selectedTabCollectionId: Long? = null
    ) {
        if (findNavController().currentDestination?.id == R.id.collectionCreationFragment) return

        val storage = requireComponents.core.tabCollectionStorage
        // Only register the observer right before moving to collection creation
        storage.register(collectionStorageObserver, this)

        val tabIds = getListOfSessions().toTabs().map { it.sessionId }.toTypedArray()
        view?.let {
            val directions = HomeFragmentDirections.actionHomeFragmentToCreateCollectionFragment(
                tabIds = tabIds,
                previousFragmentId = R.id.homeFragment,
                saveCollectionStep = step,
                selectedTabIds = selectedTabIds,
                selectedTabCollectionId = selectedTabCollectionId ?: -1
            )
            nav(R.id.homeFragment, directions)
        }
    }

    private fun saveTabToCollection(selectedTabId: String?) {
        val tabs = getListOfSessions().toTabs()
        val storage = requireComponents.core.tabCollectionStorage

        val step = when {
            tabs.size > 1 -> SaveCollectionStep.SelectTabs
            storage.cachedTabCollections.isNotEmpty() -> SaveCollectionStep.SelectCollection
            else -> SaveCollectionStep.NameCollection
        }

        showCollectionCreationFragment(step, selectedTabId?.let { arrayOf(it) })
    }

    private fun share(url: String? = null, tabs: List<ShareTab>? = null) {
        val directions =
            HomeFragmentDirections.actionHomeFragmentToShareFragment(
                url = url,
                tabs = tabs?.toTypedArray()
            )
        nav(R.id.homeFragment, directions)
    }

    private fun scrollAndAnimateCollection(
        tabsAddedToCollectionSize: Int,
        changedCollection: TabCollection? = null
    ) {
        if (view != null) {
            viewLifecycleOwner.lifecycleScope.launch {
                val recyclerView = sessionControlComponent.view
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
                sessionControlComponent.view.findViewHolderForAdapterPosition(indexOfCollection)
            val border =
                (viewHolder as? CollectionViewHolder)?.view?.findViewById<View>(R.id.selected_border)
            val listener = object : Animator.AnimatorListener {
                override fun onAnimationCancel(animation: Animator?) {
                    border?.visibility = View.GONE
                }

                override fun onAnimationStart(animation: Animator?) { /* noop */ }
                override fun onAnimationRepeat(animation: Animator?) { /* noop */ }
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
                    .setAnchorView(bottom_bar)
                    .show()
            }
        }
    }

    private fun showRenamedSnackbar() {
        view?.let { view ->
            val string = view.context.getString(R.string.snackbar_collection_renamed)
            FenixSnackbar.make(view, Snackbar.LENGTH_LONG)
                .setText(string)
                .setAnchorView(bottom_bar)
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

    companion object {
        private const val NON_TAB_ITEM_NUM = 3
        private const val ANIM_SCROLL_DELAY = 100L
        private const val ANIM_ON_SCREEN_DELAY = 200L
        private const val FADE_ANIM_DURATION = 150L
        private const val ANIM_SNACKBAR_DELAY = 100L
        private const val SHARED_TRANSITION_MS = 200L
        private const val TAB_ITEM_TRANSITION_NAME = "tab_item"
        private const val CFR_WIDTH_DIVIDER = 1.7
        private const val CFR_Y_OFFSET = -20
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

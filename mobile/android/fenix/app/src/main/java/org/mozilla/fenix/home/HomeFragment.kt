/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProviders
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.BOTTOM
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.END
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.START
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.TOP
import org.jetbrains.anko.constraint.layout.applyConstraintSet
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.FenixViewModelProvider
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.collections.CreateCollectionViewModel
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.allowUndo
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.ext.urlToTrimmedHost
import org.mozilla.fenix.home.sessioncontrol.CollectionAction
import org.mozilla.fenix.home.sessioncontrol.Mode
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.home.sessioncontrol.SessionControlComponent
import org.mozilla.fenix.home.sessioncontrol.SessionControlState
import org.mozilla.fenix.home.sessioncontrol.SessionControlViewModel
import org.mozilla.fenix.home.sessioncontrol.Tab
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.home.sessioncontrol.TabCollection
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import kotlin.coroutines.CoroutineContext
import kotlin.math.roundToInt

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment(), CoroutineScope {
    private val bus = ActionBusFactory.get(this)
    private var sessionObserver: SessionManager.Observer? = null
    private var homeMenu: HomeMenu? = null

    var deleteSessionJob: (suspend () -> Unit)? = null

    private val onboarding by lazy { FenixOnboarding(requireContext()) }
    private lateinit var sessionControlComponent: SessionControlComponent

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    // TODO Remove this stub when we have the a-c version!
    var storedCollections = mutableListOf<TabCollection>()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        job = Job()
        val view = inflater.inflate(R.layout.fragment_home, container, false)

        val mode = currentMode()

        sessionControlComponent = SessionControlComponent(
            view.homeLayout,
            bus,
            FenixViewModelProvider.create(
                this,
                SessionControlViewModel::class.java
            ) {
                SessionControlViewModel(SessionControlState(listOf(), listOf(), mode))
            }
        )

        view.homeLayout.applyConstraintSet {
            sessionControlComponent.view {
                connect(
                    TOP to BOTTOM of view.homeDivider,
                    START to START of PARENT_ID,
                    END to END of PARENT_ID,
                    BOTTOM to BOTTOM of PARENT_ID
                )
            }
        }

        ActionBusFactory.get(this).logMergedObservables()
        val activity = activity as HomeActivity
        DefaultThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity)
        return view
    }

    @SuppressWarnings("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupHomeMenu()

        launch(Dispatchers.Default) {
            val iconSize = resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()

            val searchIcon = requireComponents.search.searchEngineManager.getDefaultSearchEngine(
                requireContext()
            ).let {
                BitmapDrawable(resources, it.icon)
            }
            searchIcon.setBounds(0, 0, iconSize, iconSize)

            runBlocking(Dispatchers.Main) {
                view.toolbar.setCompoundDrawables(searchIcon, null, null, null)
            }
        }

        view.menuButton.setOnClickListener {
            homeMenu?.menuBuilder?.build(requireContext())?.show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN
            )
        }
        val roundToInt =
            (toolbarPaddingDp * Resources.getSystem().displayMetrics.density).roundToInt()
        view.toolbar.compoundDrawablePadding = roundToInt
        view.toolbar.setOnClickListener {
            invokePendingDeleteSessionJob()
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
            Navigation.findNavController(it).navigate(directions)

            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        val isPrivate = (activity as HomeActivity).browsingModeManager.isPrivate

        privateBrowsingButton.contentDescription =
            contentDescriptionForPrivateBrowsingButton(isPrivate)

        privateBrowsingButton.setOnClickListener {
            val browsingModeManager = (activity as HomeActivity).browsingModeManager
            val newMode = when (browsingModeManager.mode) {
                BrowsingModeManager.Mode.Normal -> BrowsingModeManager.Mode.Private
                BrowsingModeManager.Mode.Private -> BrowsingModeManager.Mode.Normal
            }

            val mode = if (newMode == BrowsingModeManager.Mode.Private) Mode.Private else Mode.Normal
            getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.ModeChange(mode))

            browsingModeManager.mode = newMode
        }

        // We need the shadow to be above the components.
        homeDividerShadow.bringToFront()
    }

    override fun onViewStateRestored(savedInstanceState: Bundle?) {
        super.onViewStateRestored(savedInstanceState)
        if (savedInstanceState != null) {
            getManagedEmitter<SessionControlChange>().onNext(
                SessionControlChange.TabsChange(
                    (savedInstanceState.getParcelableArrayList<Tab>(
                        KEY_TABS
                    ) ?: arrayListOf()).toList()
                )
            )
            getManagedEmitter<SessionControlChange>().onNext(
                SessionControlChange.CollectionsChange(
                    (savedInstanceState.getParcelableArrayList<TabCollection>(
                        KEY_COLLECTIONS
                    ) ?: arrayListOf()).toList()
                )
            )
        }
    }

    override fun onDestroyView() {
        homeMenu = null
        job.cancel()
        super.onDestroyView()
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    @SuppressWarnings("ComplexMethod")
    override fun onStart() {
        super.onStart()
        if (isAdded) {
            getAutoDisposeObservable<SessionControlAction>()
                .subscribe {
                    when (it) {
                        is SessionControlAction.Tab -> handleTabAction(it.action)
                        is SessionControlAction.Collection -> handleCollectionAction(it.action)
                    }
                }
        }

        val mode = currentMode()
        getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.ModeChange(mode))

        emitSessionChanges()
        sessionObserver = subscribeToSessions()
    }

    override fun onStop() {
        sessionObserver?.let {
            requireComponents.core.sessionManager.unregister(it)
        }
        super.onStop()
    }

    @SuppressWarnings("ComplexMethod")
    private fun handleTabAction(action: TabAction) {
        Do exhaustive when (action) {
            is TabAction.SaveTabGroup -> {
                showCollectionCreationFragment(action.selectedTabSessionId)
            }
            is TabAction.Select -> {
                invokePendingDeleteSessionJob()
                val session =
                    requireComponents.core.sessionManager.findSessionById(action.sessionId)
                requireComponents.core.sessionManager.select(session!!)
                (activity as HomeActivity).openToBrowser(BrowserDirection.FromHome)
            }
            is TabAction.Close -> {
                if (deleteSessionJob == null) removeTabWithUndo(action.sessionId) else {
                    deleteSessionJob?.let {
                        launch {
                            it.invoke()
                        }.invokeOnCompletion {
                            deleteSessionJob = null
                            removeTabWithUndo(action.sessionId)
                        }
                    }
                }
            }
            is TabAction.Share -> {
                requireComponents.core.sessionManager.findSessionById(action.sessionId)
                    ?.let { session ->
                        requireContext().share(session.url)
                    }
            }
            is TabAction.CloseAll -> {
                requireComponents.useCases.tabsUseCases.removeAllTabsOfType.invoke(action.private)
            }
            is TabAction.PrivateBrowsingLearnMore -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getGenericSumoURLForTopic
                        (SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS),
                    newTab = true,
                    from = BrowserDirection.FromHome
                )
            }
            is TabAction.Add -> {
                invokePendingDeleteSessionJob()
                val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
            }
            is TabAction.ShareTabs -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1843")
            }
        }
    }

    private fun invokePendingDeleteSessionJob() {
        deleteSessionJob?.let {
            launch {
                it.invoke()
            }.invokeOnCompletion {
                deleteSessionJob = null
            }
        }
    }

    @Suppress("ComplexMethod")
    private fun handleCollectionAction(action: CollectionAction) {
        when (action) {
            is CollectionAction.Expand -> {
                storedCollections.find { it.id == action.collection.id }?.apply { expanded = true }
            }
            is CollectionAction.Collapse -> {
                storedCollections.find { it.id == action.collection.id }?.apply { expanded = false }
            }
            is CollectionAction.Delete -> {
                storedCollections.find { it.id == action.collection.id }?.let { storedCollections.remove(it) }
            }
            is CollectionAction.AddTab -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1575")
            }
            is CollectionAction.Rename -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1575")
            }
            is CollectionAction.OpenTabs -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "2205")
            }
            is CollectionAction.ShareTabs -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1585")
            }
            is CollectionAction.RemoveTab -> {
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "1578")
            }
        }

        emitCollectionChange()
    }

    private fun emitCollectionChange() {
        storedCollections.map { it.copy() }.let {
            getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.CollectionsChange(it))
        }
    }

    override fun onPause() {
        super.onPause()
        sessionObserver?.let {
            requireComponents.core.sessionManager.unregister(it)
        }
    }

    private fun setupHomeMenu() {
        homeMenu = HomeMenu(requireContext()) {
            when (it) {
                HomeMenu.Item.Settings -> {
                    invokePendingDeleteSessionJob()
                    Navigation.findNavController(homeLayout).navigate(
                        HomeFragmentDirections.actionHomeFragmentToSettingsFragment()
                    )
                }
                HomeMenu.Item.Library -> {
                    invokePendingDeleteSessionJob()
                    Navigation.findNavController(homeLayout).navigate(
                        HomeFragmentDirections.actionHomeFragmentToLibraryFragment()
                    )
                }
                HomeMenu.Item.Help -> {
                    invokePendingDeleteSessionJob()
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.getSumoURLForTopic(
                            context!!,
                            SupportUtils.SumoTopic.HELP
                        ),
                        newTab = true,
                        from = BrowserDirection.FromHome
                    )
                }
            }
        }
    }

    private fun contentDescriptionForPrivateBrowsingButton(isPrivate: Boolean): String {
        val resourceId =
            if (isPrivate) R.string.content_description_disable_private_browsing_button else
                R.string.content_description_private_browsing_button

        return getString(resourceId)
    }

    private fun subscribeToSessions(): SessionManager.Observer {
        val observer = object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                super.onSessionAdded(session)
                emitSessionChanges()
            }

            override fun onSessionRemoved(session: Session) {
                super.onSessionRemoved(session)
                emitSessionChanges()
            }

            override fun onSessionSelected(session: Session) {
                super.onSessionSelected(session)
                emitSessionChanges()
            }

            override fun onSessionsRestored() {
                super.onSessionsRestored()
                emitSessionChanges()
            }

            override fun onAllSessionsRemoved() {
                super.onAllSessionsRemoved()
                emitSessionChanges()
            }
        }
        requireComponents.core.sessionManager.register(observer)
        return observer
    }

    private fun removeTabWithUndo(sessionId: String) {
        val sessionManager = requireComponents.core.sessionManager
        getManagedEmitter<SessionControlChange>().onNext(
            SessionControlChange.TabsChange(
                sessionManager.sessions
                    .filter { (activity as HomeActivity).browsingModeManager.isPrivate == it.private }
                    .filter { it.id != sessionId }
                    .map {
                        val selected =
                            it == sessionManager.selectedSession
                        Tab(
                            it.id,
                            it.url,
                            it.url.urlToTrimmedHost(),
                            it.title,
                            selected,
                            it.thumbnail
                        )
                    }
            )
        )

        deleteSessionJob = {
            sessionManager.findSessionById(sessionId)
                ?.let { session ->
                    sessionManager.remove(session)
                }
        }

        CoroutineScope(Dispatchers.Main).allowUndo(
            view!!, getString(R.string.snackbar_tab_deleted),
            getString(R.string.snackbar_deleted_undo), {
                deleteSessionJob = null
                emitSessionChanges()
            }
        ) {
            sessionManager.findSessionById(sessionId)
                ?.let { session ->
                    sessionManager.remove(session)
                }
        }
    }

    private fun emitSessionChanges() {
        val sessionManager = requireComponents.core.sessionManager

        getManagedEmitter<SessionControlChange>().onNext(
            SessionControlChange.TabsChange(
                sessionManager.sessions
                    .filter { (activity as HomeActivity).browsingModeManager.isPrivate == it.private }
                    .map {
                        val selected = it == sessionManager.selectedSession
                        Tab(
                            it.id,
                            it.url,
                            it.url.urlToTrimmedHost(),
                            it.title,
                            selected,
                            it.thumbnail
                        )
                    }
            )
        )
    }

    private fun showCollectionCreationFragment(selectedTabId: String?) {
        val tabs = requireComponents.core.sessionManager.sessions
            .map { Tab(it.id, it.url, it.url.urlToTrimmedHost(), it.title) }

        val viewModel = activity?.run {
            ViewModelProviders.of(this).get(CreateCollectionViewModel::class.java)
        }
        viewModel?.tabs = tabs
        val selectedTabs = tabs.find { tab -> tab.sessionId == selectedTabId }
        val selectedSet = if (selectedTabs == null) setOf() else setOf(selectedTabs)
        viewModel?.selectedTabs = selectedSet
        viewModel?.saveCollectionStep = SaveCollectionStep.SelectTabs

        view?.let {
            val directions = HomeFragmentDirections.actionHomeFragmentToCreateCollectionFragment()
            Navigation.findNavController(it).navigate(directions)
        }
    }

    private fun currentMode(): Mode = if (!onboarding.userHasBeenOnboarded()) {
        Mode.Onboarding
    } else if ((activity as HomeActivity).browsingModeManager.isPrivate) {
        Mode.Private
    } else { Mode.Normal }

    companion object {
        private const val toolbarPaddingDp = 12f
        private const val KEY_TABS = "tabs"
        private const val KEY_COLLECTIONS = "collections"
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.BOTTOM
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.TOP
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.START
import org.jetbrains.anko.constraint.layout.ConstraintSetBuilder.Side.END
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams.PARENT_ID
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.navigation.Navigation
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.jetbrains.anko.constraint.layout.applyConstraintSet
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.utils.ItsNotBrokenSnack
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.archive
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.sessioncontrol.ArchivedSession
import org.mozilla.fenix.home.sessioncontrol.ArchivedSessionAction
import org.mozilla.fenix.home.sessioncontrol.Mode
import org.mozilla.fenix.home.sessioncontrol.SessionControlAction
import org.mozilla.fenix.home.sessioncontrol.SessionControlChange
import org.mozilla.fenix.home.sessioncontrol.SessionControlComponent
import org.mozilla.fenix.home.sessioncontrol.SessionControlState
import org.mozilla.fenix.home.sessioncontrol.TabAction
import org.mozilla.fenix.lib.Do
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.settings.SupportUtils
import kotlin.coroutines.CoroutineContext
import kotlin.math.roundToInt

@SuppressWarnings("TooManyFunctions", "LargeClass")
class HomeFragment : Fragment(), CoroutineScope {
    private val bus = ActionBusFactory.get(this)
    private var sessionObserver: SessionManager.Observer? = null
    private var homeMenu: HomeMenu? = null
    private lateinit var sessionControlComponent: SessionControlComponent

    private lateinit var job: Job
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        job = Job()
        val view = inflater.inflate(R.layout.fragment_home, container, false)
        val mode = if ((activity as HomeActivity).browsingModeManager.isPrivate) Mode.Private else Mode.Normal
        sessionControlComponent = SessionControlComponent(
            view.homeLayout,
            bus,
            SessionControlState(listOf(), listOf(), mode)
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

        val bundles = requireComponents.core.sessionStorage.bundles(limit = temporaryNumberOfSessions)

        bundles.observe(this, Observer { sessionBundles ->
            val sessions = sessionBundles
                .filter { it.id != requireComponents.core.sessionStorage.current()?.id }
                .mapNotNull { sessionBundle ->
                    sessionBundle.id?.let {
                        ArchivedSession(it, sessionBundle, sessionBundle.lastSavedAt, sessionBundle.urls)
                    }
                }

            getManagedEmitter<SessionControlChange>().onNext(SessionControlChange.ArchivedSessionsChange(sessions))
        })

        val searchIcon = requireComponents.search.searchEngineManager.getDefaultSearchEngine(
            requireContext()
        ).let {
            BitmapDrawable(resources, it.icon)
        }

        view.menuButton.setOnClickListener {
            homeMenu?.menuBuilder?.build(requireContext())?.show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN)
        }

        val iconSize = resources.getDimension(R.dimen.preference_icon_drawable_size).toInt()
        searchIcon.setBounds(0, 0, iconSize, iconSize)
        view.toolbar.setCompoundDrawables(searchIcon, null, null, null)
        val roundToInt = (toolbarPaddingDp * Resources.getSystem().displayMetrics.density).roundToInt()
        view.toolbar.compoundDrawablePadding = roundToInt
        view.toolbar.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
            Navigation.findNavController(it).navigate(directions)

            requireComponents.analytics.metrics.track(Event.SearchBarTapped(Event.SearchBarTapped.Source.HOME))
        }

        val isPrivate = (activity as HomeActivity).browsingModeManager.isPrivate

        view.toolbar_wrapper.isPrivateModeEnabled = isPrivate
        privateBrowsingButton.contentDescription = contentDescriptionForPrivateBrowsingButton(isPrivate)

        privateBrowsingButton.setOnClickListener {
            val browsingModeManager = (activity as HomeActivity).browsingModeManager
            browsingModeManager.mode = when (browsingModeManager.mode) {
                BrowsingModeManager.Mode.Normal -> BrowsingModeManager.Mode.Private
                BrowsingModeManager.Mode.Private -> BrowsingModeManager.Mode.Normal
            }
        }

        // We need the shadow to be above the components.
        homeDividerShadow.bringToFront()
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
                        is SessionControlAction.Session -> handleSessionAction(it.action)
                    }
                }
        }

        sessionObserver = subscribeToSessions()
        sessionObserver?.onSessionsRestored()
    }

    @SuppressWarnings("ComplexMethod")
    private fun handleTabAction(action: TabAction) {
        Do exhaustive when (action) {
            is TabAction.Archive -> {
                launch {
                    requireComponents.core.sessionStorage.archive(requireComponents.core.sessionManager)
                }
            }
            is TabAction.MenuTapped -> {
                val isPrivate = (activity as HomeActivity).browsingModeManager.isPrivate
                val titles = requireComponents.core.sessionManager.sessions
                    .filter { session -> session.private == isPrivate }
                    .map { session -> session.title }

                val sessionType = if (isPrivate) {
                    SessionBottomSheetFragment.SessionType.Private(titles)
                } else {
                    SessionBottomSheetFragment.SessionType.Current(titles)
                }

                openSessionMenu(sessionType)
            }
            is TabAction.Select -> {
                val session = requireComponents.core.sessionManager.findSessionById(action.sessionId)
                requireComponents.core.sessionManager.select(session!!)
                val directions = HomeFragmentDirections.actionHomeFragmentToBrowserFragment(action.sessionId)
                Navigation.findNavController(view!!).navigate(directions)
            }
            is TabAction.Close -> {
                requireComponents.core.sessionManager.findSessionById(action.sessionId)?.let { session ->
                    requireComponents.core.sessionManager.remove(session)
                }
            }
            is TabAction.CloseAll -> {
                requireComponents.useCases.tabsUseCases.removeAllTabsOfType.invoke(action.private)
            }
            is TabAction.PrivateBrowsingLearnMore -> {
                requireComponents.useCases.tabsUseCases.addPrivateTab
                    .invoke(SupportUtils.getSumoURLForTopic(context!!, SupportUtils.SumoTopic.PRIVATE_BROWSING_MYTHS))
                (activity as HomeActivity).openToBrowser(requireComponents.core.sessionManager.selectedSession?.id,
                    BrowserDirection.FromHome)
            }
            is TabAction.Add -> {
                val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
                Navigation.findNavController(view!!).navigate(directions)
            }
        }
    }

    private fun handleSessionAction(action: ArchivedSessionAction) {
        when (action) {
            is ArchivedSessionAction.Select -> {
                launch {
                    requireComponents.core.sessionStorage.archive(requireComponents.core.sessionManager)
                    action.session.bundle.restoreSnapshot()?.apply {
                        requireComponents.core.sessionManager.restore(this)
                    }
                }
            }
            is ArchivedSessionAction.Delete -> {
                launch(IO) {
                    requireComponents.core.sessionStorage.remove(action.session.bundle)
                }
            }
            is ArchivedSessionAction.MenuTapped ->
                openSessionMenu(SessionBottomSheetFragment.SessionType.Archived(action.session))
            is ArchivedSessionAction.ShareTapped ->
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "244")
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
            val directions = when (it) {
                HomeMenu.Item.Settings -> HomeFragmentDirections.actionHomeFragmentToSettingsFragment()
                HomeMenu.Item.Library -> HomeFragmentDirections.actionHomeFragmentToLibraryFragment()
                HomeMenu.Item.Help -> return@HomeMenu // Not implemented yetN
            }

            Navigation.findNavController(homeLayout).navigate(directions)
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

    private fun emitSessionChanges() {
        val sessionManager = requireComponents.core.sessionManager
        getManagedEmitter<SessionControlChange>().onNext(
            SessionControlChange.TabsChange(
                sessionManager.sessions
                    .filter { (activity as HomeActivity).browsingModeManager.isPrivate == it.private }
                    .map {
                        val selected = it == sessionManager.selectedSession
                        org.mozilla.fenix.home.sessioncontrol.Tab(it.id, it.url, selected, it.thumbnail)
                    }
            )
        )
    }

    private fun openSessionMenu(sessionType: SessionBottomSheetFragment.SessionType) {
        SessionBottomSheetFragment.create(sessionType).apply {
            onArchive = {
                launch {
                    requireComponents.core.sessionStorage.archive(requireComponents.core.sessionManager)
                }
            }
            onDelete = {
                when (it) {
                    is SessionBottomSheetFragment.SessionType.Archived -> {
                        launch(IO) {
                            requireComponents.core.sessionStorage.remove(it.archivedSession.bundle)
                        }
                    }
                    is SessionBottomSheetFragment.SessionType.Current -> {
                        requireComponents.useCases.tabsUseCases.removeAllTabsOfType.invoke(false)
                        launch(IO) {
                            requireComponents.core.sessionStorage.current()?.apply {
                                requireComponents.core.sessionStorage.remove(this)
                            }
                        }
                    }
                    is SessionBottomSheetFragment.SessionType.Private -> {
                        requireComponents.useCases.tabsUseCases.removeAllTabsOfType.invoke(true)
                    }
                }
            }
        }.show(requireActivity().supportFragmentManager, SessionBottomSheetFragment.overflowFragmentTag)
    }

    companion object {
        const val toolbarPaddingDp = 12f
        const val temporaryNumberOfSessions = 25
    }
}

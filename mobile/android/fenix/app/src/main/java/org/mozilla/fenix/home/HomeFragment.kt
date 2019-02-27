/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import android.text.SpannableString
import android.text.style.ClickableSpan
import android.text.style.ForegroundColorSpan
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.motion.widget.MotionLayout
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.bundling.SessionBundleStorage
import org.mozilla.fenix.BrowsingModeManager
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.sessions.ArchivedSession
import org.mozilla.fenix.home.sessions.SessionsAction
import org.mozilla.fenix.home.sessions.SessionsChange
import org.mozilla.fenix.home.sessions.SessionsComponent
import org.mozilla.fenix.home.tabs.TabsAction
import org.mozilla.fenix.home.tabs.TabsChange
import org.mozilla.fenix.home.tabs.TabsComponent
import org.mozilla.fenix.home.tabs.TabsState
import org.mozilla.fenix.home.tabs.toSessionViewState
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getAutoDisposeObservable
import org.mozilla.fenix.mvi.getManagedEmitter
import kotlin.math.roundToInt

fun SessionBundleStorage.archive(sessionManager: SessionManager) {
        save(sessionManager.createSnapshot())
        sessionManager.removeAll()
        new()
}

class HomeFragment : Fragment() {
    private val bus = ActionBusFactory.get(this)
    private var sessionObserver: SessionManager.Observer? = null
    private lateinit var homeMenu: HomeMenu
    private lateinit var tabsComponent: TabsComponent
    private lateinit var sessionsComponent: SessionsComponent

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_home, container, false)
        val sessionManager = requireComponents.core.sessionManager
        tabsComponent = TabsComponent(
            view.homeContainer,
            bus,
            TabsState(sessionManager.sessions.map { it.toSessionViewState(it == sessionManager.selectedSession) })
        )
        sessionsComponent = SessionsComponent(view.homeContainer, bus)

        ActionBusFactory.get(this).logMergedObservables()
        val activity = activity as HomeActivity
        DefaultThemeManager.applyStatusBarTheme(activity.window, activity.themeManager, activity)
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        setupHomeMenu()
        setupPrivateBrowsingDescription()
        updatePrivateSessionDescriptionVisibility()

        sessionsComponent.view.visibility = if ((activity as HomeActivity).browsingModeManager.isPrivate)
            View.GONE else View.VISIBLE
        tabsComponent.tabList.isNestedScrollingEnabled = false
        sessionsComponent.view.isNestedScrollingEnabled = false

        val bundles = requireComponents.core.sessionStorage.bundles(temporaryNumberOfSessions)

        bundles.observe(this, Observer { sessionBundles ->
            val archivedSessions = sessionBundles.mapNotNull { sessionBundle ->
                sessionBundle.id?.let {
                    ArchivedSession(it, sessionBundle, sessionBundle.lastSavedAt, sessionBundle.urls)
                }
            }

            getManagedEmitter<SessionsChange>().onNext(SessionsChange.Changed(archivedSessions))
        })

        val searchIcon = requireComponents.search.searchEngineManager.getDefaultSearchEngine(requireContext()).let {
            BitmapDrawable(resources, it.icon)
        }

        view.menuButton.setOnClickListener {
            homeMenu.menuBuilder.build(requireContext()).show(
                anchor = it,
                orientation = BrowserMenu.Orientation.DOWN)
        }

        view.toolbar.setCompoundDrawablesWithIntrinsicBounds(searchIcon, null, null, null)
        val roundToInt = (toolbarPaddingDp * Resources.getSystem().displayMetrics.density).roundToInt()
        view.toolbar.compoundDrawablePadding = roundToInt
        view.toolbar.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
            Navigation.findNavController(it).navigate(directions)
        }

        // There is currently an issue with visibility changes in ConstraintLayout 2.0.0-alpha3
        // https://issuetracker.google.com/issues/122090772
        // For now we're going to manually implement KeyTriggers.
        view.homeLayout.setTransitionListener(object : MotionLayout.TransitionListener {
            private val firstKeyTrigger = KeyTrigger(
                firstKeyTriggerFrame,
                { view.toolbar_wrapper.transitionToDark() },
                { view.toolbar_wrapper.transitionToLight() }
            )
            private val secondKeyTrigger = KeyTrigger(
                secondKeyTriggerFrame,
                { view.toolbar_wrapper.transitionToDarkNoBorder() },
                { view.toolbar_wrapper.transitionToDarkFromNoBorder() }
            )

            override fun onTransitionChange(
                motionLayout: MotionLayout?,
                startId: Int,
                endId: Int,
                progress: Float
            ) {
                firstKeyTrigger.conditionallyFire(progress)
                secondKeyTrigger.conditionallyFire(progress)
            }

            override fun onTransitionCompleted(p0: MotionLayout?, p1: Int) { }
        })

        view.toolbar_wrapper.isPrivateModeEnabled = (activity as HomeActivity).browsingModeManager.isPrivate

        privateBrowsingButton.setOnClickListener {
            val browsingModeManager = (activity as HomeActivity).browsingModeManager
            browsingModeManager.mode = when (browsingModeManager.mode) {
                BrowsingModeManager.Mode.Normal -> BrowsingModeManager.Mode.Private
                BrowsingModeManager.Mode.Private -> BrowsingModeManager.Mode.Normal
            }
        }
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.hide()
    }

    override fun onStart() {
        super.onStart()
        if (isAdded) {
            getAutoDisposeObservable<TabsAction>()
                .subscribe {
                    when (it) {
                        is TabsAction.Archive -> {
                            requireComponents.core.sessionStorage.archive(requireComponents.core.sessionManager)
                        }
                        is TabsAction.Select -> {
                            val session = requireComponents.core.sessionManager.findSessionById(it.sessionId)
                            requireComponents.core.sessionManager.select(session!!)
                            val directions = HomeFragmentDirections.actionHomeFragmentToBrowserFragment(it.sessionId)
                            Navigation.findNavController(view!!).navigate(directions)
                        }
                        is TabsAction.Close -> {
                            requireComponents.core.sessionManager.findSessionById(it.sessionId)?.let { session ->
                                requireComponents.core.sessionManager.remove(session)
                            }
                        }
                    }
                }

            getAutoDisposeObservable<SessionsAction>()
                .subscribe {
                    when (it) {
                        is SessionsAction.Select -> {
                            requireComponents.core.sessionStorage.archive(requireComponents.core.sessionManager)
                            it.archivedSession.bundle.restoreSnapshot(requireComponents.core.engine)?.apply {
                                requireComponents.core.sessionManager.restore(this)
                            }
                        }
                        is SessionsAction.Delete -> {
                            requireComponents.core.sessionStorage.remove(it.archivedSession.bundle)
                        }
                    }
                }
        }

        sessionObserver = subscribeToSessions()
        sessionObserver?.onSessionsRestored()
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

    private fun setupPrivateBrowsingDescription() {
        // Format the description text to include a hyperlink
        val descriptionText = String
            .format(private_session_description.text.toString(), System.getProperty("line.separator"))
        val linkStartIndex = descriptionText.indexOf("\n\n") + 2
        val linkAction = object : ClickableSpan() {
            override fun onClick(widget: View?) {
                // TODO Go to SUMO page
            }
        }
        val textWithLink = SpannableString(descriptionText).apply {
            setSpan(linkAction, linkStartIndex, descriptionText.length, 0)

            val colorSpan = ForegroundColorSpan(private_session_description.currentTextColor)
            setSpan(colorSpan, linkStartIndex, descriptionText.length, 0)
        }
        private_session_description.text = textWithLink
    }

    private fun updatePrivateSessionDescriptionVisibility() {
        val isPrivate = (activity as HomeActivity).browsingModeManager.isPrivate
        val hasNoTabs = requireComponents.core.sessionManager.all.none { it.private }

        private_session_description_wrapper.visibility = if (isPrivate && hasNoTabs) {
            View.VISIBLE
        } else {
            View.GONE
        }
    }

    private fun subscribeToSessions(): SessionManager.Observer {
        val observer = object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                super.onSessionAdded(session)
                emitSessionChanges()
                updatePrivateSessionDescriptionVisibility()
            }

            override fun onSessionRemoved(session: Session) {
                super.onSessionRemoved(session)
                emitSessionChanges()
                updatePrivateSessionDescriptionVisibility()
            }

            override fun onSessionSelected(session: Session) {
                super.onSessionSelected(session)
                emitSessionChanges()
                updatePrivateSessionDescriptionVisibility()
            }

            override fun onSessionsRestored() {
                super.onSessionsRestored()
                emitSessionChanges()
                updatePrivateSessionDescriptionVisibility()
            }

            override fun onAllSessionsRemoved() {
                super.onAllSessionsRemoved()
                emitSessionChanges()
                updatePrivateSessionDescriptionVisibility()
            }
        }
        requireComponents.core.sessionManager.register(observer)
        return observer
    }

    private fun emitSessionChanges() {
        val sessionManager = requireComponents.core.sessionManager
        getManagedEmitter<TabsChange>().onNext(
            TabsChange.Changed(
                sessionManager.sessions
                    .filter { (activity as HomeActivity).browsingModeManager.isPrivate == it.private }
                    .map { it.toSessionViewState(it == sessionManager.selectedSession) }
            )
        )
    }

    companion object {
        const val addTabButtonIncreaseDps = 8
        const val overflowButtonIncreaseDps = 8
        const val toolbarPaddingDp = 12f
        const val firstKeyTriggerFrame = 55
        const val secondKeyTriggerFrame = 90

        const val temporaryNumberOfSessions = 25
    }
}

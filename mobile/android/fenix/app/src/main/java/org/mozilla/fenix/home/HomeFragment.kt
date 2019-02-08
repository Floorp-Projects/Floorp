/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.annotation.SuppressLint
import android.content.res.Resources
import android.graphics.drawable.BitmapDrawable
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.motion.widget.MotionLayout
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_home.*
import kotlinx.android.synthetic.main.fragment_home.view.*
import kotlinx.android.synthetic.main.tab_list_header.view.*
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.ThemeManager
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.sessions.SessionsComponent
import org.mozilla.fenix.home.tabs.TabsAction
import org.mozilla.fenix.home.tabs.TabsChange
import org.mozilla.fenix.home.tabs.TabsComponent
import org.mozilla.fenix.home.tabs.TabsState
import org.mozilla.fenix.isPrivate
import org.mozilla.fenix.mvi.ActionBusFactory
import org.mozilla.fenix.mvi.getManagedEmitter
import org.mozilla.fenix.mvi.getSafeManagedObservable
import kotlin.math.roundToInt

class HomeFragment : Fragment() {
    private val bus = ActionBusFactory.get(this)
    private var sessionObserver: SessionManager.Observer? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_home, container, false)
        TabsComponent(view.homeLayout, bus, TabsState(requireComponents.core.sessionManager.sessions))
        SessionsComponent(view.homeLayout, bus)
        ActionBusFactory.get(this).logMergedObservables()
        return view
    }

    @SuppressLint("CheckResult")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        (activity as AppCompatActivity).supportActionBar?.hide()

        layoutComponents(view.homeLayout)

        getSafeManagedObservable<TabsAction>()
            .subscribe {
                when (it) {
                    is TabsAction.Select -> {
                        requireComponents.core.sessionManager.select(it.session)
                        val directions = HomeFragmentDirections.actionHomeFragmentToBrowserFragment(it.session.id)
                        Navigation.findNavController(view).navigate(directions)
                    }
                    is TabsAction.Close -> {
                        requireComponents.core.sessionManager.remove(it.session)
                    }
                }
            }

        val searchIcon = requireComponents.search.searchEngineManager.getDefaultSearchEngine(requireContext()).let {
            BitmapDrawable(resources, it.icon)
        }

        // Temporary so we can easily test settings
        view.menuButton.setOnClickListener {
            val directions = HomeFragmentDirections.actionHomeFragmentToSettingsFragment()
            Navigation.findNavController(it).navigate(directions)
        }

        view.toolbar.setCompoundDrawablesWithIntrinsicBounds(searchIcon, null, null, null)
        val roundToInt = (toolbarPaddingDp * Resources.getSystem().displayMetrics.density).roundToInt()
        view.toolbar.compoundDrawablePadding = roundToInt
        view.toolbar.setOnClickListener { it ->
            val directions = HomeFragmentDirections.actionHomeFragmentToSearchFragment(null)
            Navigation.findNavController(it).navigate(directions)
        }
        view.add_tab_button.increaseTapArea(addTabButtonIncreaseDps)
        view.add_tab_button.setOnClickListener {
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

        view.toolbar_wrapper.isPrivateModeEnabled = (requireActivity() as HomeActivity)
            .themeManager
            .currentTheme
            .isPrivate()

        privateBrowsingButton.setOnClickListener {
            // When we build out private mode we will want to handle this logic elsewhere.
            (requireActivity() as HomeActivity).themeManager.apply {
                val newTheme = when (this.currentTheme) {
                    ThemeManager.Theme.Light -> ThemeManager.Theme.Private
                    ThemeManager.Theme.Private -> ThemeManager.Theme.Light
                }

                setTheme(newTheme)
            }
        }
    }

    @SuppressWarnings("MagicNumber")
    private class KeyTrigger(
        frame: Int,
        private val onPositiveCross: () -> Unit,
        private val onNegativeCross: () -> Unit
    ) {
        private val fireThreshhold = (frame + 0.5F) / 100.0F
        private var negativeReset = false
        private var positiveReset = false
        private var lastFirePosition = 0f
        private val triggerSlack = 0.1f

        fun conditionallyFire(progress: Float) {
            var offset: Float
            var lastOffset: Float

            if (negativeReset) {
                offset = progress - fireThreshhold
                lastOffset = lastFirePosition - fireThreshhold
                if (offset * lastOffset < 0.0f && offset < 0.0f) {
                    onNegativeCross.invoke()
                    negativeReset = false
                }
            } else if (Math.abs(progress - fireThreshhold) > triggerSlack) {
                negativeReset = true
            }

            if (positiveReset) {
                offset = progress - fireThreshhold
                lastOffset = lastFirePosition - fireThreshhold
                if (offset * lastOffset < 0.0f && offset > 0.0f) {
                    onPositiveCross.invoke()
                    positiveReset = false
                }
            } else if (Math.abs(progress - fireThreshhold) > triggerSlack) {
                positiveReset = true
            }

            lastFirePosition = progress
        }
    }

    override fun onResume() {
        super.onResume()
        sessionObserver = subscribeToSessions()
    }

    override fun onPause() {
        super.onPause()
        sessionObserver?.let {
            requireComponents.core.sessionManager.unregister(it)
        }
    }

    private fun subscribeToSessions(): SessionManager.Observer {
        val observer = object : SessionManager.Observer {
            override fun onSessionAdded(session: Session) {
                super.onSessionAdded(session)
                getManagedEmitter<TabsChange>().onNext(
                    TabsChange.Changed(requireComponents.core.sessionManager.sessions))
            }

            override fun onSessionRemoved(session: Session) {
                super.onSessionRemoved(session)
                getManagedEmitter<TabsChange>().onNext(
                    TabsChange.Changed(requireComponents.core.sessionManager.sessions))
            }

            override fun onSessionSelected(session: Session) {
                super.onSessionSelected(session)
                getManagedEmitter<TabsChange>().onNext(
                    TabsChange.Changed(requireComponents.core.sessionManager.sessions))
            }

            override fun onSessionsRestored() {
                super.onSessionsRestored()
                getManagedEmitter<TabsChange>().onNext(
                    TabsChange.Changed(requireComponents.core.sessionManager.sessions))
            }

            override fun onAllSessionsRemoved() {
                super.onAllSessionsRemoved()
                getManagedEmitter<TabsChange>().onNext(
                    TabsChange.Changed(requireComponents.core.sessionManager.sessions))
            }
        }
        requireComponents.core.sessionManager.register(observer)
        return observer
    }

    companion object {
        const val addTabButtonIncreaseDps = 8
        const val toolbarPaddingDp = 12f
        const val firstKeyTriggerFrame = 55
        const val secondKeyTriggerFrame = 90
    }
}

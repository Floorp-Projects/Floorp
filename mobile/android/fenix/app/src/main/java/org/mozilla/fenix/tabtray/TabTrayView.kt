/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray_fab.view.*
import kotlinx.android.synthetic.main.fragment_tab_tray.*
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.tabstray.TabsFeature
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components


interface TabTrayInteractor {
    fun onTabSelected(tab: Tab)
    fun onNewTabTapped(private: Boolean)
    fun onTabTrayDismissed()
}
/**
 * View that contains and configures the BrowserAwesomeBar
 */
class TabTrayView(
    private val container: ViewGroup,
    private val interactor: TabTrayInteractor
) : LayoutContainer, TabsTray.Observer, TabLayout.OnTabSelectedListener {
    val fabView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray_fab, container, true)

    val view = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray, container, true)

    private val behavior = BottomSheetBehavior.from(view.tab_wrapper)
    private var tabsFeature: TabsFeature

    override val containerView: View?
        get() = container

    init {
        fabView.new_tab_button.compatElevation = 80.0f

        behavior.addBottomSheetCallback(object : BottomSheetBehavior.BottomSheetCallback() {
            override fun onSlide(bottomSheet: View, slideOffset: Float) {

                if (slideOffset > -0.4) {
                    fabView.new_tab_button.show()
                } else {
                    fabView.new_tab_button.hide()
                }
            }

            override fun onStateChanged(bottomSheet: View, newState: Int) {
                if (newState == BottomSheetBehavior.STATE_HIDDEN) {
                    interactor.onTabTrayDismissed()
                }
            }
        })

        view.tab_layout.addOnTabSelectedListener(this)

        tabsFeature = TabsFeature(
            view.tabsTray,
            view.context.components.core.store,
            view.context.components.useCases.tabsUseCases,
            { true },
            { })

        (view.tabsTray as? BrowserTabsTray)?.also { tray ->
            TabsTouchHelper(tray.tabsAdapter).attachToRecyclerView(tray)
        }

        fabView.new_tab_button.setOnClickListener {
            interactor.onNewTabTapped(view.tab_layout.selectedTabPosition == 1)
        }

        tabsTray.register(this)
        tabsFeature.start()
    }

    override fun onTabClosed(tab: Tab) {}

    override fun onTabSelected(tab: Tab) {
        interactor.onTabSelected(tab)
    }

    override fun onTabReselected(tab: TabLayout.Tab?) {

    }

    override fun onTabUnselected(tab: TabLayout.Tab?) {

    }

    override fun onTabSelected(tab: TabLayout.Tab?) {
        // Todo: We need a better way to determine which tab was selected.
        val filter: (TabSessionState) -> Boolean = when (tab?.position) {
            1 -> { state -> state.content.private }
            else -> { state -> !state.content.private }
        }

        tabsFeature.filterTabs(filter)
    }
}
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.session.ui

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.PopupWindow
import androidx.recyclerview.widget.LinearLayoutManager
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.focus.Components
import org.mozilla.focus.GleanMetrics.TabCount
import org.mozilla.focus.databinding.PopupTabsBinding
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.telemetry.TelemetryWrapper

class TabsPopup(
    private val parentView: ViewGroup,
    private val components: Components,
) : PopupWindow() {
    private lateinit var binding: PopupTabsBinding

    init {
        initializeLayout()
    }

    private fun initializeLayout() {
        val layoutInflater =
            parentView.context.getSystemService(Context.LAYOUT_INFLATER_SERVICE) as LayoutInflater
        binding = PopupTabsBinding.inflate(layoutInflater, parentView, false)
        val sessionsAdapter = TabsAdapter(
            tabList = components.store.state.privateTabs,
            isCurrentSession = { tab -> isCurrentSession(tab) },
            selectSession = { tab -> selectSession(tab) },
            closeSession = { tab -> closeSession(tab) },
        )

        binding.sessions.apply {
            adapter = sessionsAdapter
            layoutManager = LinearLayoutManager(parentView.context)
            setHasFixedSize(true)
        }
        contentView = binding.root
        isFocusable = true
        width = FrameLayout.LayoutParams.WRAP_CONTENT
        height = FrameLayout.LayoutParams.WRAP_CONTENT
        animationStyle = android.R.style.Animation_Dialog
        binding.root.setOnClickListener { dismiss() }
    }

    override fun dismiss() {
        super.dismiss()
        val openedTabs = components.store.state.tabs.size
        TabCount.sessionListClosed.record(TabCount.SessionListClosedExtra(openedTabs))

        TelemetryWrapper.closeTabsTrayEvent()

        components.appStore.dispatch(AppAction.HideTabs)
    }

    private fun isCurrentSession(tab: TabSessionState): Boolean {
        return components.store.state.selectedTabId == tab.id
    }

    private fun selectSession(tab: TabSessionState) {
        components.tabsUseCases.selectTab.invoke(tab.id)
        val openedTabs = components.store.state.tabs.size
        TabCount.sessionListItemTapped.record(TabCount.SessionListItemTappedExtra(openedTabs))
        dismiss()
    }

    private fun closeSession(tab: TabSessionState) {
        components.tabsUseCases.removeTab.invoke(tab.id, selectParentIfExists = false)
        val openedTabs = components.store.state.tabs.size
        TabCount.sessionListItemTapped.record(TabCount.SessionListItemTappedExtra(openedTabs))
        dismiss()
    }
}

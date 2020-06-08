/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.tabs.TabLayout
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_tabstray.*
import kotlinx.android.synthetic.main.component_tabstray.view.*
import kotlinx.android.synthetic.main.component_tabstray_fab.view.*
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.tabstray.BrowserTabsTray
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

interface TabTrayInteractor {
    fun onTabClosed(tab: Tab)
    fun onTabSelected(tab: Tab)
    fun onNewTabTapped(private: Boolean)
    fun onTabTrayDismissed()
    fun onShareTabsClicked(private: Boolean)
    fun onSaveToCollectionClicked()
    fun onCloseAllTabsClicked(private: Boolean)
}
/**
 * View that contains and configures the BrowserAwesomeBar
 */
class TabTrayView(
    private val container: ViewGroup,
    private val interactor: TabTrayInteractor,
    isPrivate: Boolean,
    startingInLandscape: Boolean,
    private val filterTabs: ((TabSessionState) -> Boolean) -> Unit
) : LayoutContainer, TabsTray.Observer, TabLayout.OnTabSelectedListener {
    val fabView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray_fab, container, true)

    val view = LayoutInflater.from(container.context)
        .inflate(R.layout.component_tabstray, container, true)

    val isPrivateModeSelected: Boolean get() = view.tab_layout.selectedTabPosition == PRIVATE_TAB_ID

    private val behavior = BottomSheetBehavior.from(view.tab_wrapper)

    private var tabTrayItemMenu: TabTrayItemMenu

    private var hasLoaded = false

    override val containerView: View?
        get() = container

    init {
        toggleFabText(isPrivate)

        behavior.addBottomSheetCallback(object : BottomSheetBehavior.BottomSheetCallback() {
            override fun onSlide(bottomSheet: View, slideOffset: Float) {
                if (slideOffset >= SLIDE_OFFSET) {
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

        val selectedTabIndex = if (!isPrivate) {
            DEFAULT_TAB_ID
        } else {
            PRIVATE_TAB_ID
        }

        view.tab_layout.getTabAt(selectedTabIndex)?.also {
            view.tab_layout.selectTab(it, true)
        }

        view.tab_layout.addOnTabSelectedListener(this)

        val tabs = if (isPrivate) {
            view.context.components.core.store.state.privateTabs
        } else {
            view.context.components.core.store.state.normalTabs
        }

        val selectedBrowserTabIndex = tabs
            .indexOfFirst { it.id == view.context.components.core.store.state.selectedTabId }

        if (tabs.size > EXPAND_AT_SIZE || startingInLandscape) {
            expand()
        }

        behavior.setExpandedOffset(view.context.resources.getDimension(R.dimen.tab_tray_top_offset).toInt())

        (view.tabsTray as? BrowserTabsTray)?.also { tray ->
            TabsTouchHelper(tray.tabsAdapter).attachToRecyclerView(tray)
            (tray.tabsAdapter as? FenixTabsAdapter)?.also { adapter ->
                adapter.onTabsUpdated = {
                    if (!hasLoaded) {
                        hasLoaded = true
                        tray.layoutManager?.scrollToPosition(selectedBrowserTabIndex)
                    }
                }
            }
        }

        tabTrayItemMenu = TabTrayItemMenu(view.context, { view.tab_layout.selectedTabPosition == 0 }) {
            when (it) {
                is TabTrayItemMenu.Item.ShareAllTabs -> interactor.onShareTabsClicked(
                    isPrivateModeSelected
                )
                is TabTrayItemMenu.Item.SaveToCollection -> interactor.onSaveToCollectionClicked()
                is TabTrayItemMenu.Item.CloseAllTabs -> interactor.onCloseAllTabsClicked(
                    isPrivateModeSelected
                )
            }
        }

        view.tab_tray_overflow.setOnClickListener {
            tabTrayItemMenu.menuBuilder
                .build(view.context)
                .show(anchor = it)
                .also { pu ->
                    (pu.contentView as? CardView)?.setCardBackgroundColor(ContextCompat.getColor(
                        view.context,
                        R.color.foundation_normal_theme
                    ))
                }
        }

        fabView.new_tab_button.setOnClickListener {
            interactor.onNewTabTapped(isPrivateModeSelected)
        }
    }

    fun expand() {
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    override fun onTabSelected(tab: Tab) {
        interactor.onTabSelected(tab)
    }

    override fun onTabSelected(tab: TabLayout.Tab?) {
        // We need a better way to determine which tab was selected.
        val filter: (TabSessionState) -> Boolean = when (tab?.position) {
            1 -> { state -> state.content.private }
            else -> { state -> !state.content.private }
        }

        toggleFabText(isPrivateModeSelected)
        filterTabs.invoke(filter)

        updateState(view.context.components.core.store.state)
    }

    fun updateState(state: BrowserState) {
        view.let {
            val hasNoTabs = if (isPrivateModeSelected) {
                state.privateTabs.isEmpty()
            } else {
                state.normalTabs.isEmpty()
            }

            view.tab_tray_empty_view.isVisible = hasNoTabs
            if (hasNoTabs) {
                view.tab_tray_empty_view.text = if (isPrivateModeSelected) {
                    view.context.getString(R.string.no_private_tabs_description)
                } else {
                    view.context?.getString(R.string.no_open_tabs_description)
                }
            }

            view.tabsTray.asView().isVisible = !hasNoTabs
            view.tab_tray_overflow.isVisible = !hasNoTabs
        }
    }

    override fun onTabClosed(tab: Tab) {
        interactor.onTabClosed(tab)
    }
    override fun onTabReselected(tab: TabLayout.Tab?) { /*noop*/ }
    override fun onTabUnselected(tab: TabLayout.Tab?) { /*noop*/ }

    fun toggleFabText(private: Boolean) {
        if (private) {
            fabView.new_tab_button.extend()
            fabView.new_tab_button.contentDescription = view.context.resources.getString(R.string.add_private_tab)
        } else {
            fabView.new_tab_button.shrink()
            fabView.new_tab_button.contentDescription = view.context.resources.getString(R.string.add_tab)
        }
    }

    companion object {
        private const val DEFAULT_TAB_ID = 0
        private const val PRIVATE_TAB_ID = 1
        private const val EXPAND_AT_SIZE = 3
        private const val SLIDE_OFFSET = 0
    }
}

class TabTrayItemMenu(
    private val context: Context,
    private val shouldShowSaveToCollection: () -> Boolean,
    private val onItemTapped: (Item) -> Unit = {}
) {

    sealed class Item {
        object ShareAllTabs : Item()
        object SaveToCollection : Item()
        object CloseAllTabs : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_save),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.SaveToCollection)
            }.apply { visible = shouldShowSaveToCollection },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_share),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.ShareAllTabs)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_close),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.CloseAllTabs)
            }
        )
    }
}

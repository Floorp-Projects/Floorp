/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.util.TypedValue
import android.view.View
import android.view.ViewGroup
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import java.lang.ref.WeakReference

/**
 * A [Toolbar.Action] implementation that shows a [TabCounter].
 */
@OptIn(ExperimentalCoroutinesApi::class)
class TabCounterToolbarButton(
    private val lifecycleOwner: LifecycleOwner,
    private val isPrivate: Boolean,
    private val onItemTapped: (TabCounterMenuItem) -> Unit = {},
    private val showTabs: () -> Unit
) : Toolbar.Action {
    private var reference: WeakReference<TabCounter> = WeakReference<TabCounter>(null)

    override fun createView(parent: ViewGroup): View {
        parent.context.components.core.store.flowScoped(lifecycleOwner) { flow ->
            flow.map { state -> state.getNormalOrPrivateTabs(isPrivate).size }
                .ifChanged()
                .collect { tabs -> updateCount(tabs) }
        }

        val view = TabCounter(parent.context).apply {
            reference = WeakReference(this)
            setOnClickListener {
                showTabs.invoke()
            }

            setOnLongClickListener {
                getTabContextMenu(it.context).show(it)
                true
            }

            addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
                override fun onViewAttachedToWindow(v: View?) {
                    setCount(context.components.core.store.state.getNormalOrPrivateTabs(isPrivate).size)
                }

                override fun onViewDetachedFromWindow(v: View?) { /* no-op */
                }
            })
        }

        // Set selectableItemBackgroundBorderless
        val outValue = TypedValue()
        parent.context.theme.resolveAttribute(
            android.R.attr.selectableItemBackgroundBorderless,
            outValue,
            true
        )
        view.setBackgroundResource(outValue.resourceId)
        return view
    }

    override fun bind(view: View) = Unit

    private fun updateCount(count: Int) {
        reference.get()?.setCountWithAnimation(count)
    }

    private fun getTabContextMenu(context: Context): BrowserMenu {
        val primaryTextColor = ThemeManager.resolveAttribute(R.attr.primaryText, context)
        val metrics = context.components.analytics.metrics
        val menuItems = listOf(
            BrowserMenuImageText(
                label = context.getString(R.string.browser_menu_new_tab),
                imageResource = R.drawable.ic_new,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_TAB))
                onItemTapped(TabCounterMenuItem.NewTab(false))
            },
            BrowserMenuImageText(
                label = context.getString(R.string.home_screen_shortcut_open_new_private_tab_2),
                imageResource = R.drawable.ic_private_browsing,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_PRIVATE_TAB))
                onItemTapped(TabCounterMenuItem.NewTab(true))
            },
            BrowserMenuDivider(),
            BrowserMenuImageText(
                label = context.getString(R.string.close_tab),
                imageResource = R.drawable.ic_close,
                iconTintColorResource = primaryTextColor,
                textColorResource = primaryTextColor
            ) {
                metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.CLOSE_TAB))
                onItemTapped(TabCounterMenuItem.CloseTab)
            }
        )

        return BrowserMenuBuilder(
            when (context.settings().toolbarPosition) {
                ToolbarPosition.BOTTOM -> menuItems.reversed()
                ToolbarPosition.TOP -> menuItems
            }
        ).build(context)
    }
}

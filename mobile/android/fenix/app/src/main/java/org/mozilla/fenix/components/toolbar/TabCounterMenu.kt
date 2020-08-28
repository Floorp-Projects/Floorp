/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

class TabCounterMenu(
    context: Context,
    private val metrics: MetricController,
    private val onItemTapped: (Item) -> Unit
) {

    sealed class Item {
        object CloseTab : Item()
        data class NewTab(val mode: BrowsingMode) : Item()
    }

    val menuController: MenuController by lazy { BrowserMenuController() }

    private val newTabItem: TextMenuCandidate
    private val newPrivateTabItem: TextMenuCandidate
    private val closeTabItem: TextMenuCandidate

    init {
        val primaryTextColor = context.getColorFromAttr(R.attr.primaryText)
        val textStyle = TextStyle(color = primaryTextColor)

        newTabItem = TextMenuCandidate(
            text = context.getString(R.string.browser_menu_new_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.ic_new,
                tint = primaryTextColor
            ),
            textStyle = textStyle
        ) {
            metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_TAB))
            onItemTapped(Item.NewTab(BrowsingMode.Normal))
        }

        newPrivateTabItem = TextMenuCandidate(
            text = context.getString(R.string.home_screen_shortcut_open_new_private_tab_2),
            start = DrawableMenuIcon(
                context,
                R.drawable.ic_private_browsing,
                tint = primaryTextColor
            ),
            textStyle = textStyle
        ) {
            metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_PRIVATE_TAB))
            onItemTapped(Item.NewTab(BrowsingMode.Private))
        }

        closeTabItem = TextMenuCandidate(
            text = context.getString(R.string.close_tab),
            start = DrawableMenuIcon(
                context,
                R.drawable.ic_close,
                tint = primaryTextColor
            ),
            textStyle = textStyle
        ) {
            metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.CLOSE_TAB))
            onItemTapped(Item.CloseTab)
        }
    }

    @VisibleForTesting
    internal fun menuItems(showOnly: BrowsingMode?): List<MenuCandidate> {
        return when (showOnly) {
            BrowsingMode.Normal -> listOf(newTabItem)
            BrowsingMode.Private -> listOf(newPrivateTabItem)
            null -> listOf(
                newTabItem,
                newPrivateTabItem,
                DividerMenuCandidate(),
                closeTabItem
            )
        }
    }

    fun updateMenu(showOnly: BrowsingMode? = null) {
        menuController.submitList(menuItems(showOnly))
    }
}

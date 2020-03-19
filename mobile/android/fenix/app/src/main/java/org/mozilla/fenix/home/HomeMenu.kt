/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.whatsnew.WhatsNew

class HomeMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object WhatsNew : Item()
        object Help : Item()
        object Settings : Item()
        object Library : Item()
        object History : Item()
        object Bookmarks : Item()
        object Quit : Item()
        object Sync : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val hasAccountProblem get() = context.components.backgroundServices.accountManager.accountNeedsReauth()
    private val primaryTextColor =
        ThemeManager.resolveAttribute(R.attr.primaryText, context)
    private val syncDisconnectedColor = ThemeManager.resolveAttribute(R.attr.syncDisconnected, context)
    private val syncDisconnectedBackgroundColor = context.getColorFromAttr(R.attr.syncDisconnectedBackground)

    private val menuCategoryTextColor =
        ThemeManager.resolveAttribute(R.attr.menuCategoryText, context)

    private val menuItems by lazy {

        val reconnectToSyncItem = BrowserMenuHighlightableItem(
            context.getString(R.string.sync_reconnect),
            R.drawable.ic_sync_disconnected,
            iconTintColorResource = syncDisconnectedColor,
            textColorResource = primaryTextColor,
            highlight = BrowserMenuHighlight.HighPriority(
                backgroundTint = syncDisconnectedBackgroundColor
            ),
            isHighlighted = { true }
        ) {
            onItemTapped.invoke(Item.Sync)
        }

        val whatsNewItem = BrowserMenuHighlightableItem(
            context.getString(R.string.browser_menu_whats_new),
            R.drawable.ic_whats_new,
            iconTintColorResource = primaryTextColor,
            highlight = BrowserMenuHighlight.LowPriority(
                notificationTint = getColor(context, R.color.whats_new_notification_color)
            ),
            isHighlighted = { WhatsNew.shouldHighlightWhatsNew(context) }
        ) {
            onItemTapped.invoke(Item.WhatsNew)
        }

        val bookmarksItem = BrowserMenuImageText(
            context.getString(R.string.library_bookmarks),
            R.drawable.ic_bookmark_outline,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Bookmarks)
        }

        val libraryItem = BrowserMenuImageText(
            context.getString(R.string.library_history),
            R.drawable.ic_history,
            primaryTextColor) {
            onItemTapped.invoke(Item.History)
        }

        val settingsItem = BrowserMenuImageText(
            context.getString(R.string.browser_menu_settings),
            R.drawable.ic_settings,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Settings)
        }

        val helpItem = BrowserMenuImageText(
            context.getString(R.string.browser_menu_help),
            R.drawable.ic_help,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Help)
        }

        val quitItem = BrowserMenuImageText(
            context.getString(R.string.delete_browsing_data_on_quit_action),
            R.drawable.ic_exit,
            primaryTextColor
        ) {
            onItemTapped.invoke(Item.Quit)
        }

        val items = listOfNotNull(
            if (hasAccountProblem) reconnectToSyncItem else null,
            whatsNewItem,
            BrowserMenuDivider(),
            BrowserMenuCategory(
                context.getString(R.string.browser_menu_library),
                textColorResource = menuCategoryTextColor
            ),
            bookmarksItem,
            libraryItem,
            BrowserMenuDivider(),
            settingsItem,
            helpItem,
            if (Settings.getInstance(context).shouldDeleteBrowsingDataOnQuit) quitItem else null
        )

        items
    }
}

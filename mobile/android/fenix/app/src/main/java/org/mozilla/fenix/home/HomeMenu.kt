/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager
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
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_settings),
                R.drawable.ic_settings,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Settings)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_your_library),
                R.drawable.ic_library,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Library)
            },

            BrowserMenuDivider(),
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Help)
            },

            BrowserMenuHighlightableItem(
                context.getString(R.string.browser_menu_whats_new),
                R.drawable.ic_whats_new,
                highlight = BrowserMenuHighlightableItem.Highlight(
                    startImageResource = R.drawable.ic_whats_new_notification,
                    backgroundResource = ThemeManager.resolveAttribute(R.attr.selectableItemBackground, context),
                    colorResource = ContextCompat.getColor(context, R.color.whats_new_notification_color)
                ),
                isHighlighted = { WhatsNew.shouldHighlightWhatsNew(context) }
            ) {
                onItemTapped.invoke(Item.WhatsNew)
            }
        )
    }
}

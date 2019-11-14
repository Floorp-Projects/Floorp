/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
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
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val hasAccountProblem get() = context.components.backgroundServices.accountManager.accountNeedsReauth()
    private val primaryTextColor =
        ThemeManager.resolveAttribute(R.attr.primaryText, context)

    private val menuCategoryTextColor =
        ThemeManager.resolveAttribute(R.attr.menuCategoryText, context)

    private val menuItems by lazy {
        val items = mutableListOf(
            BrowserMenuCategory(
                context.getString(R.string.browser_menu_your_library),
                textColorResource = menuCategoryTextColor
            ),

            BrowserMenuImageText(
                context.getString(R.string.library_bookmarks),
                R.drawable.ic_bookmark_outline,
                primaryTextColor
            ) {
                onItemTapped.invoke(Item.Bookmarks)
            },

            BrowserMenuImageText(
                context.getString(R.string.library_history),
                R.drawable.ic_history,
                primaryTextColor
            ) {
                onItemTapped.invoke(Item.History)
            },

            BrowserMenuDivider(),

            BrowserMenuHighlightableItem(
                label = context.getString(R.string.browser_menu_settings),
                imageResource = R.drawable.ic_settings,
                iconTintColorResource =
                if (hasAccountProblem) R.color.sync_error_text_color else primaryTextColor,

                textColorResource =
                if (hasAccountProblem) R.color.sync_error_text_color else primaryTextColor,

                highlight = if (hasAccountProblem) {
                    BrowserMenuHighlightableItem.Highlight(
                        endImageResource = R.drawable.ic_alert,
                        backgroundResource = R.drawable.sync_error_background_with_ripple,
                        colorResource = R.color.sync_error_background_color
                    )
                } else null
            ) {
                onItemTapped.invoke(Item.Settings)
            },

            BrowserMenuHighlightableItem(
                context.getString(R.string.browser_menu_whats_new),
                R.drawable.ic_whats_new,
                highlight = BrowserMenuHighlightableItem.Highlight(
                    startImageResource = R.drawable.ic_whats_new_notification,
                    backgroundResource = ThemeManager.resolveAttribute(
                        R.attr.selectableItemBackground,
                        context
                    ),
                    colorResource = R.color.whats_new_notification_color
                ),
                isHighlighted = { WhatsNew.shouldHighlightWhatsNew(context) }
            ) {
                onItemTapped.invoke(Item.WhatsNew)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                primaryTextColor
            ) {
                onItemTapped.invoke(Item.Help)
            }

        )

        if (Settings.getInstance(context).shouldDeleteBrowsingDataOnQuit) {
            items.add(
                BrowserMenuImageText(
                    context.getString(R.string.delete_browsing_data_on_quit_action),
                    R.drawable.ic_exit,
                    primaryTextColor
                ) {
                    onItemTapped.invoke(Item.Quit)
                }
            )
        }

        items
    }
}

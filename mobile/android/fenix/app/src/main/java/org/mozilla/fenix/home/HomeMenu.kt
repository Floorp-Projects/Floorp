/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager

class HomeMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
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
                onItemTapped.invoke(HomeMenu.Item.Settings)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_your_library),
                R.drawable.ic_library,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(HomeMenu.Item.Library)
            },

            BrowserMenuDivider(),
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(HomeMenu.Item.Help)
            })
    }
}

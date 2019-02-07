/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.toolbar

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import org.mozilla.fenix.R

class ToolbarMenu(private val context: Context, private val onItemTapped: (Item) -> Unit = {}) {
    sealed class Item {
        object Help : Item()
        object Settings : Item()
        object Library : Item()
        data class RequestDesktop(val isChecked: Boolean) : Item()
        object FindInPage : Item()
        object NewPrivateTab : Item()
        object NewTab : Item()
        object Share : Item()
        object Back : Item()
        object Forward : Item()
        object Reload : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_back)
        ) {
            onItemTapped.invoke(Item.Back)
        }

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_forward)
        ) {
            onItemTapped.invoke(Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_refresh)
        ) {
            onItemTapped.invoke(Item.Reload)
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        listOf(
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                context.getString(R.string.browser_menu_help),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.Help)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_settings),
                R.drawable.ic_settings,
                context.getString(R.string.browser_menu_settings),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.Settings)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_library),
                R.drawable.ic_library,
                context.getString(R.string.browser_menu_library),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.Library)
            },

            BrowserMenuDivider(),

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site), {
                false
            }) { checked ->
                onItemTapped.invoke(Item.RequestDesktop(checked))
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                context.getString(R.string.browser_menu_find_in_page),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.FindInPage)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_private_tab),
                R.drawable.ic_private_browsing,
                context.getString(R.string.browser_menu_private_tab),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.NewPrivateTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_new_tab),
                R.drawable.ic_new,
                context.getString(R.string.browser_menu_new_tab),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.NewTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                context.getString(R.string.browser_menu_share),
                R.color.icons
            ) {
                onItemTapped.invoke(Item.Share)
            },

            BrowserMenuDivider(),

            menuToolbar
        )
    }
}

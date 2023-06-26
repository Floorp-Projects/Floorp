/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.menu.home

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuImageText
import org.mozilla.focus.R

/**
 * The overflow menu shown on the start/home screen.
 */
class HomeMenu(
    private val context: Context,
    private val onItemTapped: ((HomeMenuItem) -> Unit),
) {
    fun getMenuBuilder(): BrowserMenuBuilder {
        val help = BrowserMenuImageText(
            label = context.getString(R.string.menu_help),
            imageResource = R.drawable.mozac_ic_help_circle_24,
        ) {
            onItemTapped.invoke(HomeMenuItem.Help)
        }

        val settings = BrowserMenuImageText(
            label = context.getString(R.string.menu_settings),
            imageResource = R.drawable.mozac_ic_settings_24,
        ) {
            onItemTapped.invoke(HomeMenuItem.Settings)
        }
        return BrowserMenuBuilder(items = listOf(help, settings))
    }
}

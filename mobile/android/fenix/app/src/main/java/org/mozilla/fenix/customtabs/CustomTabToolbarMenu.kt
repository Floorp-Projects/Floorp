/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.ext.components

class CustomTabToolbarMenu(
    private val context: Context,
    private val requestDesktopStateProvider: () -> Boolean = { false },
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {}
) : ToolbarMenu {
    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                context.components.core.sessionManager.selectedSession?.canGoBack ?: true
            },
            secondaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.neutral,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back)
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                context.components.core.sessionManager.selectedSession?.canGoForward ?: true
            },
            secondaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.neutral,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                val loading = context.components.core.sessionManager.selectedSession?.loading
                loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = DefaultThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            disableInSecondaryState = false
        ) {
            if (context.components.core.sessionManager.selectedSession?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload)
            }
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                {
                    val appName = context.getString(R.string.app_name)
                    context.getString(R.string.browser_menu_powered_by, appName).toUpperCase()
                }(),
                ToolbarMenu.CAPTION_TEXT_SIZE,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ),

            BrowserMenuDivider(),

            SimpleBrowserMenuItem(
                {
                    val appName = context.getString(R.string.app_name)
                    context.getString(R.string.browser_menu_open_in_fenix, appName)
                }(),
                textColorResource = DefaultThemeManager.resolveAttribute(
                    R.attr.primaryText,
                    context
                )
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.OpenInFenix)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
            },

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site),
                requestDesktopStateProvider, { checked ->
                    onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
                }),

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            },
            menuToolbar
        )
    }
}

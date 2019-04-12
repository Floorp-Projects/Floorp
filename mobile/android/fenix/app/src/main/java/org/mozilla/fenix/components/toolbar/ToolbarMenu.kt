/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class ToolbarMenu(
    private val context: Context,
    private val sessionId: String?,
    private val requestDesktopStateProvider: () -> Boolean = { false },
    private val onItemTapped: (Item) -> Unit = {}
) {
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
        object Stop : Item()
        object ReportIssue : Item()
        object OpenInFenix : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    val menuToolbar by lazy {
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
                R.attr.secondaryText,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(Item.Back)
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
                R.attr.secondaryText,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(Item.Forward)
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
                onItemTapped.invoke(Item.Stop)
            } else {
                onItemTapped.invoke(Item.Reload)
            }
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        listOf(
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Help)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_settings),
                R.drawable.ic_settings,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Settings)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_library),
                R.drawable.ic_library,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Library)
            },

            BrowserMenuDivider(),

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site),
                requestDesktopStateProvider, { checked ->
                    onItemTapped.invoke(Item.RequestDesktop(checked))
                }),

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.FindInPage)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_private_tab),
                R.drawable.ic_private_browsing,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.NewPrivateTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_new_tab),
                R.drawable.ic_new,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.NewTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.Share)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_report_issue),
                R.drawable.ic_report_issues,
                DefaultThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(Item.ReportIssue)
            },

            BrowserMenuDivider(),

            menuToolbar
        )
    }

    companion object {
        const val CAPTION_TEXT_SIZE = 12f
    }
}

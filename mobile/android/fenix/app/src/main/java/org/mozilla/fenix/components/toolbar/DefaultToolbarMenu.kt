/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

class DefaultToolbarMenu(
    private val context: Context,
    private val hasAccountProblem: Boolean = false,
    private val requestDesktopStateProvider: () -> Boolean = { false },
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {}
) : ToolbarMenu {

    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems, endOfMenuAlwaysVisible = true) }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                context.components.core.sessionManager.selectedSession?.canGoBack ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.disabled,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back)
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                context.components.core.sessionManager.selectedSession?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.disabled,
                context
            ),
            disableInSecondaryState = true
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                val loading = context.components.core.sessionManager.selectedSession?.loading
                loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = ThemeManager.resolveAttribute(
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
        val items = mutableListOf(
            BrowserMenuImageText(
                context.getString(R.string.browser_menu_help),
                R.drawable.ic_help,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Help)
            },

            BrowserMenuHighlightableItem(
                label = context.getString(R.string.browser_menu_settings),
                imageResource = R.drawable.ic_settings,
                iconTintColorResource = if (hasAccountProblem)
                    R.color.sync_error_text_color else
                    ThemeManager.resolveAttribute(R.attr.primaryText, context),
                textColorResource = if (hasAccountProblem)
                    R.color.sync_error_text_color else
                    ThemeManager.resolveAttribute(R.attr.primaryText, context),
                highlight = if (hasAccountProblem) {
                    BrowserMenuHighlightableItem.Highlight(
                        endImageResource = R.drawable.ic_alert,
                        backgroundResource = R.drawable.sync_error_background_with_ripple,
                        colorResource = R.color.sync_error_background_color
                    )
                } else null
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Settings)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_your_library),
                R.drawable.ic_library,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Library)
            },

            BrowserMenuImageSwitch(
                R.drawable.ic_desktop,
                context.getString(R.string.browser_menu_desktop_site),
                requestDesktopStateProvider
            ) { checked ->
                onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_add_to_homescreen),
                R.drawable.ic_add_to_homescreen,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.AddToHomeScreen)
            }.apply {
                visible = ::shouldShowAddToHomescreen
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_private_tab),
                R.drawable.ic_private_browsing,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.NewPrivateTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_new_tab),
                R.drawable.ic_new,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.NewTab)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_report_issue),
                R.drawable.ic_report_issues,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.ReportIssue)
            }
        )

        if ((context.asActivity() as? HomeActivity)?.browsingModeManager?.mode == BrowsingMode.Normal) {
            items.add(
                BrowserMenuImageText(
                    context.getString(R.string.browser_menu_save_to_collection),
                    R.drawable.ic_tab_collection,
                    ThemeManager.resolveAttribute(R.attr.primaryText, context)
                ) {
                    onItemTapped.invoke(ToolbarMenu.Item.SaveToCollection)
                }
            )
        }

        if (Settings.getInstance(context).shouldDeleteBrowsingDataOnQuit) {
            items.add(
                BrowserMenuImageText(
                    context.getString(R.string.delete_browsing_data_on_quit_action),
                    R.drawable.ic_exit,
                    ThemeManager.resolveAttribute(R.attr.primaryText, context)
                ) {
                    onItemTapped.invoke(ToolbarMenu.Item.Quit)
                }
            )
        }

        items.add(
            BrowserMenuDivider()
        )

        items.add(
            menuToolbar
        )

        items
    }

    private fun shouldShowAddToHomescreen(): Boolean {
        return context.components.useCases.webAppUseCases.isPinningSupported() &&
                context.components.core.sessionManager.selectedSession != null
    }
}

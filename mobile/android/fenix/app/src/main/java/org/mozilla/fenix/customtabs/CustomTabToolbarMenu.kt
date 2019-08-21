/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.components.toolbar.ToolbarMenu

class CustomTabToolbarMenu(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val sessionId: String?,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {}
) : ToolbarMenu {
    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val session: Session?
        get() = sessionId?.let { sessionManager.findSessionById(it) }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.primaryText,
                context
            ),
            isInPrimaryState = {
                session?.canGoBack ?: true
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
                session?.canGoForward ?: true
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
                val loading = session?.loading
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
            if (session?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload)
            }
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        listOf(
            menuToolbar,

            BrowserMenuDivider(),

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                textColorResource = ThemeManager.resolveAttribute(
                    R.attr.primaryText,
                    context
                ),
                iconTintColorResource = ThemeManager.resolveAttribute(
                    R.attr.primaryText,
                    context
                )
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.Share)
            },

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site),
                { session?.desktopMode ?: false }, { checked ->
                    onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
                }),

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
            },

            SimpleBrowserMenuItem(
                {
                    val appName = context.getString(R.string.app_name)
                    context.getString(R.string.browser_menu_open_in_fenix, appName)
                }(),
                textColorResource = ThemeManager.resolveAttribute(
                    R.attr.primaryText,
                    context
                )
            ) {
                onItemTapped.invoke(ToolbarMenu.Item.OpenInFenix)
            },

            BrowserMenuDivider(),

            SimpleBrowserMenuItem(
                {
                    val appName = context.getString(R.string.app_name)
                    context.getString(R.string.browser_menu_powered_by, appName).toUpperCase()
                }(),
                ToolbarMenu.CAPTION_TEXT_SIZE,
                ThemeManager.resolveAttribute(R.attr.primaryText, context)
            )
        )
    }
}

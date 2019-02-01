/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.content.Intent
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.share
import org.mozilla.fenix.settings.SettingsActivity

/**
 * Component group for all functionality related to the browser toolbar.
 */
class Toolbar(
    private val context: Context,
    private val sessionUseCases: SessionUseCases,
    private val sessionManager: SessionManager
) {

    /**
     * Helper class for building browser menus.app
     */
    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    /**
     * Provides autocomplete functionality for shipped / provided domain lists.
     */
    val shippedDomainsProvider by lazy {
        ShippedDomainsProvider().also { it.initialize(context) }
    }

    private val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_back,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_back)
        ) {
            sessionUseCases.goBack.invoke()
        }

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_forward)
        ) {
            sessionUseCases.goForward.invoke()
        }

        val refresh = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            iconTintColorResource = R.color.icons,
            contentDescription = context.getString(R.string.browser_menu_refresh)
        ) {
            sessionUseCases.reload.invoke()
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
                // TODO Help
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_settings),
                R.drawable.ic_settings,
                context.getString(R.string.browser_menu_settings),
                R.color.icons
            ) {
                openSettingsActivity()
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_library),
                R.drawable.ic_library,
                context.getString(R.string.browser_menu_library),
                R.color.icons
            ) {
                // TODO Your Library
            },

            BrowserMenuDivider(),

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site), {
                sessionManager.selectedSessionOrThrow.desktopMode
            }) { checked ->
                sessionUseCases.requestDesktopSite.invoke(checked)
            }.apply {
                visible = { sessionManager.selectedSession != null }
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_find_in_page),
                R.drawable.mozac_ic_search,
                context.getString(R.string.browser_menu_find_in_page),
                R.color.icons
            ) {
                // TODO Find in Page
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_private_tab),
                R.drawable.ic_private_browsing,
                context.getString(R.string.browser_menu_private_tab),
                R.color.icons
            ) {
                // TODO Private Tab
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_new_tab),
                R.drawable.ic_new,
                context.getString(R.string.browser_menu_new_tab),
                R.color.icons
            ) {
                // TODO New Tab
            },

            BrowserMenuImageText(
                context.getString(R.string.browser_menu_share),
                R.drawable.mozac_ic_share,
                context.getString(R.string.browser_menu_share),
                R.color.icons
            ) {
                val url = sessionManager.selectedSession?.url ?: ""
                context.share(url)
            }.apply {
                visible = { sessionManager.selectedSession != null }
            },

            BrowserMenuDivider(),

            menuToolbar
        )
    }

    private fun openSettingsActivity() {
        val intent = Intent(context, SettingsActivity::class.java)
        intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
        context.startActivity(intent)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.fenix.R

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
            contentDescription = "Back"
        ) {
            sessionUseCases.goBack.invoke()
        }

        val forward = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_forward,
            iconTintColorResource = R.color.icons,
            contentDescription = "Forward"
        ) {
            sessionUseCases.goForward.invoke()
        }

        val refresh = BrowserMenuItemToolbar.Button(
            mozilla.components.ui.icons.R.drawable.mozac_ic_refresh,
            iconTintColorResource = R.color.icons,
            contentDescription = "Refresh"
        ) {
            sessionUseCases.reload.invoke()
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        listOf(
            menuToolbar,
            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_help)) {
                // TODO Help
            },

            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_settings)) {
                openSettingsActivity()
            },

            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_library)) {
                // TODO Your Library
            },

            BrowserMenuSwitch(context.getString(R.string.browser_menu_desktop_site), {
                sessionManager.selectedSessionOrThrow.desktopMode
            }) { checked ->
                sessionUseCases.requestDesktopSite.invoke(checked)
            }.apply {
                visible = { sessionManager.selectedSession != null }
            },

            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_find_in_page)) {
                // TODO Find in Page
            },

            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_private_tab)) {
                // TODO Private Tab
            },

            SimpleBrowserMenuItem(context.getString(R.string.browser_menu_new_tab)) {
                // TODO New Tab
            }
        )
    }

    private fun openSettingsActivity() {
        // TODO Open Settings
    }
}
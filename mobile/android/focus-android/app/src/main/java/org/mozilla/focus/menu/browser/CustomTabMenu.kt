/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.menu.browser

import android.content.Context
import android.graphics.Typeface
import mozilla.components.browser.menu.WebExtensionBrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.menu.item.WebExtensionPlaceholderMenuItem
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.webcompat.reporter.WebCompatReporterFeature
import org.mozilla.focus.R
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.theme.resolveAttribute

class CustomTabMenu(
    private val context: Context,
    private val store: BrowserStore,
    private val currentTabId: String,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
) : ToolbarMenu {

    private val selectedSession: CustomTabSessionState?
        get() = store.state.findCustomTab(currentTabId)

    override val menuBuilder by lazy {
        WebExtensionBrowserMenuBuilder(
            items = menuItems,
            store = store,
        )
    }

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.mozac_ic_back_24,
            primaryContentDescription = context.getString(R.string.content_description_back),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.canGoBack ?: false
            },
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.disabled),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.CustomTabItem.Back) },
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.Back)
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.mozac_ic_forward_24,
            primaryContentDescription = context.getString(R.string.content_description_forward),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.canGoForward ?: true
            },
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.disabled),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.CustomTabItem.Forward) },
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.Forward)
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = R.drawable.mozac_ic_arrow_clockwise_24,
            primaryContentDescription = context.getString(R.string.content_description_reload),
            primaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            isInPrimaryState = {
                selectedSession?.content?.loading == false
            },
            secondaryImageResource = R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.content_description_stop),
            secondaryImageTintResource = context.theme.resolveAttribute(R.attr.primaryText),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.CustomTabItem.Reload) },
        ) {
            if (selectedSession?.content?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.CustomTabItem.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.CustomTabItem.Reload)
            }
        }
        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private val menuItems by lazy {
        val findInPage = BrowserMenuImageText(
            label = context.getString(R.string.find_in_page),
            imageResource = R.drawable.mozac_ic_search_24,
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.FindInPage)
        }

        val desktopMode = BrowserMenuImageSwitch(
            imageResource = R.drawable.mozac_ic_device_desktop_24,
            label = context.getString(R.string.preference_performance_request_desktop_site2),
            initialState = {
                selectedSession?.content?.desktopMode ?: true
            },
        ) { checked ->
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.RequestDesktop(checked))
        }

        val reportSiteIssue = WebExtensionPlaceholderMenuItem(
            id = WebCompatReporterFeature.WEBCOMPAT_REPORTER_EXTENSION_ID,
            iconTintColorResource = context.theme.resolveAttribute(R.attr.primaryText),
        )

        val addToHomescreen = BrowserMenuImageText(
            label = context.getString(R.string.menu_add_to_home_screen),
            imageResource = R.drawable.mozac_ic_add_to_homescreen_24,
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.AddToHomeScreen)
        }

        val appName = context.getString(R.string.app_name)
        val openInFocus = SimpleBrowserMenuItem(
            label = context.getString(R.string.menu_open_with_default_browser2, appName),
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.OpenInBrowser)
        }

        val openInApp = SimpleBrowserMenuItem(
            label = context.getString(R.string.menu_open_with_a_browser2),
        ) {
            onItemTapped.invoke(ToolbarMenu.CustomTabItem.OpenInApp)
        }

        val poweredBy = BrowserMenuCategory(
            label = context.getString(R.string.menu_custom_tab_branding, context.getString(R.string.app_name)),
            textSize = CAPTION_TEXT_SIZE,
            textColorResource = context.theme.resolveAttribute(R.attr.secondaryText),
            backgroundColorResource = context.theme.resolveAttribute(R.attr.colorPrimary),
            textStyle = Typeface.NORMAL,
        )

        listOfNotNull(
            menuToolbar,
            BrowserMenuDivider(),
            findInPage,
            desktopMode,
            reportSiteIssue,
            BrowserMenuDivider(),
            addToHomescreen,
            openInFocus,
            openInApp,
            poweredBy,
        )
    }

    companion object {
        private const val CAPTION_TEXT_SIZE = 12f
    }
}

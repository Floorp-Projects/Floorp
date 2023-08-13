/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import android.graphics.Typeface
import androidx.annotation.ColorRes
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat.getColor
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.item.BrowserMenuCategory
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.menu.item.BrowserMenuImageSwitch
import mozilla.components.browser.menu.item.BrowserMenuImageText
import mozilla.components.browser.menu.item.BrowserMenuItemToolbar
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarMenu
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getStringWithArgSafe
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import java.util.Locale

/**
 * Builds the toolbar object used with the 3-dot menu in the custom tab browser fragment.
 *
 * @property context An Android [Context].
 * @property store reference to the application's [BrowserStore].
 * @property sessionId ID of the open custom tab session.
 * @property shouldReverseItems If true, reverse the menu items.
 * @property isSandboxCustomTab If true, menu should not show the "Open in Firefox" and "POWERED BY FIREFOX" items.
 * @property onItemTapped Called when a menu item is tapped.
 */
class CustomTabToolbarMenu(
    private val context: Context,
    private val store: BrowserStore,
    private val sessionId: String?,
    private val shouldReverseItems: Boolean,
    private val isSandboxCustomTab: Boolean,
    private val onItemTapped: (ToolbarMenu.Item) -> Unit = {},
) : ToolbarMenu {

    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    /** Gets the current custom tab session */
    @VisibleForTesting
    internal val session: CustomTabSessionState? get() = sessionId?.let { store.state.findCustomTab(it) }
    private val appName = context.getString(R.string.app_name)

    override val menuToolbar by lazy {
        val back = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_back_24,
            primaryContentDescription = context.getString(R.string.browser_menu_back),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.content?.canGoBack ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.textDisabled,
                context,
            ),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = true)) },
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Back(viewHistory = false))
        }

        val forward = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_forward_24,
            primaryContentDescription = context.getString(R.string.browser_menu_forward),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.content?.canGoForward ?: true
            },
            secondaryImageTintResource = ThemeManager.resolveAttribute(
                R.attr.textDisabled,
                context,
            ),
            disableInSecondaryState = true,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = true)) },
        ) {
            onItemTapped.invoke(ToolbarMenu.Item.Forward(viewHistory = false))
        }

        val refresh = BrowserMenuItemToolbar.TwoStateButton(
            primaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_arrow_clockwise_24,
            primaryContentDescription = context.getString(R.string.browser_menu_refresh),
            primaryImageTintResource = primaryTextColor(),
            isInPrimaryState = {
                session?.content?.loading == false
            },
            secondaryImageResource = mozilla.components.ui.icons.R.drawable.mozac_ic_stop,
            secondaryContentDescription = context.getString(R.string.browser_menu_stop),
            secondaryImageTintResource = primaryTextColor(),
            disableInSecondaryState = false,
            longClickListener = { onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = true)) },
        ) {
            if (session?.content?.loading == true) {
                onItemTapped.invoke(ToolbarMenu.Item.Stop)
            } else {
                onItemTapped.invoke(ToolbarMenu.Item.Reload(bypassCache = false))
            }
        }

        BrowserMenuItemToolbar(listOf(back, forward, refresh))
    }

    private fun shouldShowOpenInApp(): Boolean = session?.let { session ->
        val appLink = context.components.useCases.appLinksUseCases.appLinkRedirect
        appLink(session.content.url).hasExternalApp()
    } ?: false

    private val menuItems by lazy {
        val menuItems = listOf(
            poweredBy.apply { visible = { !isSandboxCustomTab } },
            BrowserMenuDivider().apply { visible = { !isSandboxCustomTab } },
            desktopMode,
            findInPage,
            openInApp.apply { visible = ::shouldShowOpenInApp },
            openInFenix.apply { visible = { !isSandboxCustomTab } },
            BrowserMenuDivider(),
            menuToolbar,
        )
        if (shouldReverseItems) {
            menuItems.reversed()
        } else {
            menuItems
        }
    }

    private val desktopMode = BrowserMenuImageSwitch(
        imageResource = R.drawable.ic_desktop,
        label = context.getString(R.string.browser_menu_desktop_site),
        initialState = { session?.content?.desktopMode ?: false },
    ) { checked ->
        onItemTapped.invoke(ToolbarMenu.Item.RequestDesktop(checked))
    }

    private val findInPage = BrowserMenuImageText(
        label = context.getString(R.string.browser_menu_find_in_page),
        imageResource = R.drawable.mozac_ic_search_24,
        iconTintColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.FindInPage)
    }

    private val openInApp = BrowserMenuHighlightableItem(
        label = context.getString(R.string.browser_menu_open_app_link),
        startImageResource = R.drawable.ic_open_in_app,
        iconTintColorResource = primaryTextColor(),
        highlight = BrowserMenuHighlight.LowPriority(
            label = context.getString(R.string.browser_menu_open_app_link),
            notificationTint = getColor(context, R.color.fx_mobile_icon_color_information),
        ),
        isHighlighted = { !context.settings().openInAppOpened },
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.OpenInApp)
    }

    private val openInFenix = SimpleBrowserMenuItem(
        label = context.getString(R.string.browser_menu_open_in_fenix, appName),
        textColorResource = primaryTextColor(),
    ) {
        onItemTapped.invoke(ToolbarMenu.Item.OpenInFenix)
    }

    private val poweredBy = BrowserMenuCategory(
        label = context.getStringWithArgSafe(R.string.browser_menu_powered_by, appName)
            .uppercase(Locale.getDefault()),
        textSize = CAPTION_TEXT_SIZE,
        textColorResource = primaryTextColor(),
        textStyle = Typeface.NORMAL,
    )

    @ColorRes
    private fun primaryTextColor() = ThemeManager.resolveAttribute(R.attr.textPrimary, context)

    companion object {
        private const val CAPTION_TEXT_SIZE = 12f
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.menu

import android.content.Context
import androidx.annotation.ColorInt
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.NestedMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.feature.addons.R

private fun createBackMenuItem(
    context: Context,
    @ColorInt webExtIconTintColor: Int?,
) = NestedMenuCandidate(
    id = R.drawable.mozac_ic_back,
    text = context.getString(R.string.mozac_feature_addons_addons),
    start = DrawableMenuIcon(
        context,
        R.drawable.mozac_ic_back,
        tint = webExtIconTintColor,
    ),
    subMenuItems = null,
)

private fun createAddonsManagerItem(
    context: Context,
    @ColorInt webExtIconTintColor: Int?,
    onAddonsManagerTapped: () -> Unit,
) = TextMenuCandidate(
    text = context.getString(R.string.mozac_feature_addons_addons_manager),
    start = DrawableMenuIcon(
        context,
        R.drawable.mozac_ic_extensions,
        tint = webExtIconTintColor,
    ),
    onClick = onAddonsManagerTapped,
)

private fun createWebExtensionSubMenuItems(
    context: Context,
    extensions: Collection<WebExtensionState>,
    tab: SessionState?,
    onAddonsItemTapped: (String) -> Unit,
): List<MenuCandidate> {
    val menuItems = mutableListOf<MenuCandidate>()

    extensions
        .filter { it.enabled }
        .filterNot { !it.allowedInPrivateBrowsing && tab?.content?.private == true }
        .sortedBy { it.name }
        .forEach { extension ->
            val tabExtensionState = tab?.extensionState?.get(extension.id)
            extension.browserAction?.let { browserAction ->
                menuItems.add(
                    browserAction.copyWithOverride(tabExtensionState?.browserAction).createMenuCandidate(
                        context,
                    ) {
                        onAddonsItemTapped(extension.id)
                        browserAction.onClick()
                    },
                )
            }

            extension.pageAction?.let { pageAction ->
                menuItems.add(
                    pageAction.copyWithOverride(tabExtensionState?.pageAction).createMenuCandidate(
                        context,
                    ) {
                        onAddonsItemTapped(extension.id)
                        pageAction.onClick()
                    },
                )
            }
        }

    return menuItems
}

/**
 * Create a browser menu item for displaying a list of web extensions.
 *
 * @param tabId ID of tab used to load tab-specific extension state.
 * @param webExtIconTintColor Optional color used to tint the icons of back and add-ons manager menu items.
 * @param appendExtensionSubMenuAt If web extension sub menu should appear at the top (start) of
 * the menu, or if web extensions should appear at the bottom of the menu (end).
 * @param onAddonsItemTapped Callback to be invoked when a web extension action item is selected.
 * Can be used to emit telemetry.
 * @param onAddonsManagerTapped Callback to be invoked when add-ons manager menu item is selected.
 */
@Suppress("LongParameterList")
fun BrowserState.createWebExtensionMenuCandidate(
    context: Context,
    tabId: String? = null,
    @ColorInt webExtIconTintColor: Int? = null,
    appendExtensionSubMenuAt: Side = Side.END,
    onAddonsItemTapped: (String) -> Unit = {},
    onAddonsManagerTapped: () -> Unit = {},
): MenuCandidate {
    val items = createWebExtensionSubMenuItems(
        context,
        extensions = extensions.values,
        tab = findTabOrCustomTabOrSelectedTab(tabId),
        onAddonsItemTapped = onAddonsItemTapped,
    )

    val addonsManagerItem = createAddonsManagerItem(
        context,
        webExtIconTintColor = webExtIconTintColor,
        onAddonsManagerTapped = onAddonsManagerTapped,
    )

    return if (items.isNotEmpty()) {
        val firstItem: MenuCandidate
        val lastItem: MenuCandidate
        when (appendExtensionSubMenuAt) {
            Side.START -> {
                firstItem = createBackMenuItem(context, webExtIconTintColor)
                lastItem = addonsManagerItem
            }
            Side.END -> {
                firstItem = addonsManagerItem
                lastItem = createBackMenuItem(context, webExtIconTintColor)
            }
        }

        NestedMenuCandidate(
            id = R.string.mozac_feature_addons_addons,
            text = context.getString(R.string.mozac_feature_addons_addons),
            start = addonsManagerItem.start,
            subMenuItems = listOf(firstItem, DividerMenuCandidate()) +
                items + listOf(DividerMenuCandidate(), lastItem),
        )
    } else {
        addonsManagerItem.copy(
            text = context.getString(R.string.mozac_feature_addons_addons),
        )
    }
}

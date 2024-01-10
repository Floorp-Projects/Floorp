/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.layout.Column
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.viewinterop.AndroidView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme

/**
 *  A helper class to add NavigationBar composable to a [ViewGroup].
 *
 * @param context The Context the view is running in.
 * @param container The ViewGroup into which the NavigationBar composable will be added.
 * @param navigationItems A list of [ActionItem] objects representing the items to be displayed in the navigation bar.
 * @param androidToolbarView An option toolbar view that will be added atop of the navigation bar.
 * @param menuButton A [MenuButton] to be used for [ItemType.MENU].
 * @param browsingModeManager A helper class that provides access to the current [BrowsingMode].
 *
 * Defaults to [NavigationItems.defaultItems] which provides a standard set of navigation items.
 */
class BottomToolbarContainerView(
    context: Context,
    container: ViewGroup,
    navigationItems: List<ActionItem> = NavigationItems.defaultItems,
    androidToolbarView: View? = null,
    menuButton: MenuButton,
    browsingModeManager: BrowsingModeManager,
) {

    init {
        val composeView = ComposeView(context).apply {
            setContent {
                val isPrivate = browsingModeManager.mode.isPrivate
                val tabCount = context.components.core.store.observeAsState(initialValue = 0) { browserState ->
                    if (isPrivate) {
                        browserState.privateTabs.size
                    } else {
                        browserState.normalTabs.size
                    }
                }.value

                FirefoxTheme {
                    Column {
                        if (androidToolbarView != null) {
                            AndroidView(factory = { _ -> androidToolbarView })
                        } else {
                            Divider()
                        }

                        NavigationBar(
                            actionItems = navigationItems,
                            tabCount = tabCount,
                            menuButton = menuButton,
                        )
                    }
                }
            }
        }

        val layoutParams = CoordinatorLayout.LayoutParams(
            CoordinatorLayout.LayoutParams.MATCH_PARENT,
            CoordinatorLayout.LayoutParams.WRAP_CONTENT,
        ).apply {
            gravity = Gravity.BOTTOM
        }

        composeView.layoutParams = layoutParams
        container.addView(composeView)
    }
}

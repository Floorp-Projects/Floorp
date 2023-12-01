/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.Gravity
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import org.mozilla.fenix.theme.FirefoxTheme

/**
 *  A helper class to add NavigationBar composable to a [ViewGroup].
 *
 * @param context The Context the view is running in.
 * @param container The ViewGroup into which the NavigationBar composable will be added.
 * @param navigationItems A list of [ActionItem] objects representing the items to be displayed in the navigation bar.
 * Defaults to [NavigationItems.defaultItems] which provides a standard set of navigation items.
 */
class NavigationBarView(
    context: Context,
    container: ViewGroup,
    navigationItems: List<ActionItem> = NavigationItems.defaultItems,
) {

    init {
        val composeView = ComposeView(context).apply {
            setContent {
                FirefoxTheme {
                    NavigationBar(navigationItems)
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

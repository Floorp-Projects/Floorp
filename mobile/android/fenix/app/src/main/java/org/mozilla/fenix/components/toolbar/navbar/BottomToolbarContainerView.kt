/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.compose.foundation.layout.Column
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.viewinterop.AndroidView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.lib.state.ext.observeAsState
import mozilla.components.ui.widgets.behavior.EngineViewScrollingBehavior
import mozilla.components.ui.widgets.behavior.ViewPosition
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme

/**
 *  A helper class to add NavigationBar composable to a [ViewGroup].
 *
 * @param context The Context the view is running in.
 * @param parent The ViewGroup into which the NavigationBar composable will be added.
 * @param navigationItems A list of [ActionItem] objects representing the items to be displayed in the navigation bar.
 * @param androidToolbarView An option toolbar view that will be added atop of the navigation bar.
 * @param menuButton A [MenuButton] to be used for [ItemType.MENU].
 * @param isPrivateMode If browsing in [BrowsingMode.Private].
 *
 * Defaults to [NavigationItems.defaultItems] which provides a standard set of navigation items.
 */
class BottomToolbarContainerView(
    context: Context,
    parent: ViewGroup,
    navigationItems: List<ActionItem> = NavigationItems.defaultItems,
    androidToolbarView: View? = null,
    menuButton: MenuButton,
    isPrivateMode: Boolean = false,
) {

    val toolbarContainerView = ToolbarContainerView(context)
    val composeView: ComposeView

    init {
        composeView = ComposeView(context).apply {
            setContent {
                val tabCount = context.components.core.store.observeAsState(initialValue = 0) { browserState ->
                    if (isPrivateMode) {
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

            toolbarContainerView.addView(this)
        }

        toolbarContainerView.layoutParams = CoordinatorLayout.LayoutParams(
            CoordinatorLayout.LayoutParams.MATCH_PARENT,
            CoordinatorLayout.LayoutParams.WRAP_CONTENT,
        ).apply {
            gravity = Gravity.BOTTOM
            behavior = EngineViewScrollingBehavior(parent.context, null, ViewPosition.BOTTOM)
        }

        parent.addView(toolbarContainerView)
    }
}

/**
 * A container view that hosts a navigation bar and, possibly, a toolbar.
 * Facilitates hide-on-scroll behavior.
 */
class ToolbarContainerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : LinearLayout(context, attrs, defStyleAttr), ScrollableToolbar {
    override fun enableScrolling() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.enableScrolling()
        }
    }

    override fun disableScrolling() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.disableScrolling()
        }
    }

    override fun expand() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.forceExpand(this@ToolbarContainerView)
        }
    }

    override fun collapse() {
        (layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? EngineViewScrollingBehavior)?.forceCollapse(this@ToolbarContainerView)
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.ViewGroup
import android.widget.LinearLayout
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.ui.widgets.behavior.EngineViewScrollingBehavior
import mozilla.components.ui.widgets.behavior.ViewPosition

/**
 *  A helper class to add NavigationBar composable to a [ViewGroup].
 *
 * @param context The Context the view is running in.
 * @param parent The ViewGroup into which the NavigationBar composable will be added.
 * @param hideOnScroll If the container should react to the [EngineView] content being scrolled.
 * @param composableContent
 */
class BottomToolbarContainerView(
    context: Context,
    parent: ViewGroup,
    hideOnScroll: Boolean = false,
    composableContent: @Composable () -> Unit,
) {

    val toolbarContainerView = ToolbarContainerView(context)
    val composeView: ComposeView

    init {
        composeView = ComposeView(context).apply {
            setContent {
                composableContent()
            }

            toolbarContainerView.addView(this)
        }

        toolbarContainerView.layoutParams = CoordinatorLayout.LayoutParams(
            CoordinatorLayout.LayoutParams.MATCH_PARENT,
            CoordinatorLayout.LayoutParams.WRAP_CONTENT,
        ).apply {
            gravity = Gravity.BOTTOM
            if (hideOnScroll) {
                behavior = EngineViewScrollingBehavior(parent.context, null, ViewPosition.BOTTOM)
            }
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

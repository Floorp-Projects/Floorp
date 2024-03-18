/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import android.content.Context
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.ui.widgets.behavior.EngineViewClippingBehavior
import mozilla.components.ui.widgets.behavior.EngineViewScrollingBehavior
import org.mozilla.focus.R
import mozilla.components.ui.widgets.behavior.ToolbarPosition as engineToolbarPosition
import mozilla.components.ui.widgets.behavior.ViewPosition as browserToolbarPosition

/**
 * Collapse the toolbar and block it from appearing until calling [enableDynamicBehavior].
 * Useful in situations like entering fullscreen.
 *
 * @param engineView [EngineView] previously set to react to toolbar's dynamic behavior.
 * Will now go through a bit of cleanup to ensure everything will be displayed nicely even without a toolbar.
 */
fun BrowserToolbar.disableDynamicBehavior(engineView: EngineView) {
    (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior = null

    engineView.setDynamicToolbarMaxHeight(0)
    engineView.asView().translationY = 0f
    (engineView.asView().layoutParams as? CoordinatorLayout.LayoutParams)?.behavior = null
}

/**
 * Expand the toolbar and reenable the dynamic behavior.
 * Useful after [disableDynamicBehavior] for situations like exiting fullscreen.
 *
 * @param context [Context] used in setting up the dynamic behavior.
 * @param engineView [EngineView] that should react to toolbar's dynamic behavior.
 */
fun BrowserToolbar.enableDynamicBehavior(context: Context, engineView: EngineView) {
    (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior = EngineViewScrollingBehavior(
        context,
        null,
        browserToolbarPosition.TOP,
    )

    val toolbarHeight = context.resources.getDimension(R.dimen.browser_toolbar_height).toInt()
    engineView.setDynamicToolbarMaxHeight(toolbarHeight)
    (engineView.asView().layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
        topMargin = 0
        behavior = EngineViewClippingBehavior(
            context,
            null,
            engineView.asView(),
            toolbarHeight,
            engineToolbarPosition.TOP,
        )
    }
}

/**
 * Show this toolbar at the top of the screen, fixed in place, with the EngineView immediately below it.
 *
 * @param context [Context] used for various system interactions
 * @param engineView [EngineView] that must be shown immediately below the toolbar.
 */
fun BrowserToolbar.showAsFixed(context: Context, engineView: EngineView) {
    isVisible = true

    engineView.setDynamicToolbarMaxHeight(0)

    val toolbarHeight = context.resources.getDimension(R.dimen.browser_toolbar_height).toInt()
    (engineView.asView().layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin = toolbarHeight
}

/**
 * Remove this toolbar from the screen and allow the EngineView to occupy the entire screen.
 *
 * @param engineView [EngineView] that will be configured to occupy the entire screen.
 */
fun BrowserToolbar.hide(engineView: EngineView) {
    engineView.setDynamicToolbarMaxHeight(0)
    (engineView.asView().layoutParams as? CoordinatorLayout.LayoutParams)?.topMargin = 0

    isVisible = false
}

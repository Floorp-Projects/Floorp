/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.content.Context
import android.util.AttributeSet
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import kotlin.math.roundToInt

/**
 * A [CoordinatorLayout.Behavior] implementation that allows the [EngineView] to automatically
 * size itself in relation to the Y translation of the [ScrollableToolbar].
 *
 * This is useful for dynamic [ScrollableToolbar]s ensuring the web content is displayed immediately
 * below / above the toolbar even when that is animated.
 *
 * @param context [Context] used for various Android interactions
 * @param attrs XML set attributes configuring this
 * @param engineViewParent NestedScrollingChild parent of the [EngineView]
 * @param toolbarHeight size of [ScrollableToolbar] when it is placed above the [EngineView]
 * @param toolbarPosition whether the [ScrollableToolbar] is placed above or below the [EngineView]
 */
class EngineViewClippingBehavior(
    context: Context?,
    attrs: AttributeSet?,
    engineViewParent: View,
    toolbarHeight: Int,
    toolbarPosition: ToolbarPosition,
) : CoordinatorLayout.Behavior<View>(context, attrs) {

    @VisibleForTesting
    internal val engineView = engineViewParent.findViewInHierarchy { it is EngineView } as EngineView?

    @VisibleForTesting
    internal var toolbarChangedAction: (Float) -> Unit?
    private val bottomToolbarChangedAction = { newToolbarTranslationY: Float ->
        if (!newToolbarTranslationY.isNaN()) {
            engineView?.setVerticalClipping(-newToolbarTranslationY.roundToInt())
        }
    }
    private val topToolbarChangedAction = { newToolbarTranslationY: Float ->
        // the top toolbar is translated upwards when collapsing-> all values received are 0 or negative
        engineView?.let {
            it.setVerticalClipping(newToolbarTranslationY.roundToInt())
            // Need to add the toolbarHeight to effectively place the engineView below the toolbar.
            engineViewParent.translationY = newToolbarTranslationY + toolbarHeight
        }
    }

    init {
        toolbarChangedAction = if (toolbarPosition == ToolbarPosition.TOP) {
            topToolbarChangedAction
        } else {
            bottomToolbarChangedAction
        }
    }

    override fun layoutDependsOn(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        if (dependency is ScrollableToolbar) {
            return true
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    /**
     * Apply vertical clipping to [EngineView]. This requires [EngineViewClippingBehavior] to be set
     * in/on the [EngineView] or its parent. Must be a direct descending child of [CoordinatorLayout].
     */
    override fun onDependentViewChanged(parent: CoordinatorLayout, child: View, dependency: View): Boolean {
        toolbarChangedAction.invoke(dependency.translationY)

        return true
    }
}

/**
 * Where the toolbar is placed on the screen.
 */
enum class ToolbarPosition {
    TOP,
    BOTTOM,
}

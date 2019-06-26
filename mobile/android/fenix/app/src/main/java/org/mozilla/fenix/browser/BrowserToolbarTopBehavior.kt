/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.animation.ValueAnimator
import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.View
import android.view.View.SCROLL_AXIS_VERTICAL
import android.view.animation.DecelerateInterpolator
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat.TYPE_NON_TOUCH
import androidx.core.view.ViewCompat.TYPE_TOUCH
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.R
import kotlin.math.max
import kotlin.math.min

private const val SNAP_ANIMATION_DURATION = 150L

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [BrowserToolbar] at the top of the screen.
 *
 * This implementation will:
 * - Show/Hide the [BrowserToolbar] automatically when scrolling vertically.
 * - On showing a [Snackbar] position it above the [BrowserToolbar].
 * - Snap the [BrowserToolbar] to be hidden or visible when the user stops scrolling.
 */
class BrowserToolbarTopBehavior(
    context: Context?,
    attrs: AttributeSet?
) : CoordinatorLayout.Behavior<BrowserToolbar>(context, attrs) {
    // This implementation is heavily based on this blog article:
    // https://android.jlelse.eu/scroll-your-bottom-navigation-view-away-with-10-lines-of-code-346f1ed40e9e

    internal var shouldSnapAfterScroll: Boolean = false

    internal var snapAnimator: ValueAnimator = ValueAnimator().apply {
        interpolator = DecelerateInterpolator()
        duration = SNAP_ANIMATION_DURATION
    }

    fun forceExpand(view: View) {
        animateSnap(view, SnapDirection.DOWN)
    }

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int
    ): Boolean {
        return if (axes == SCROLL_AXIS_VERTICAL) {
            shouldSnapAfterScroll = type == TYPE_TOUCH
            snapAnimator.cancel()
            true
        } else {
            false
        }
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        target: View,
        type: Int
    ) {
        if (shouldSnapAfterScroll || type == TYPE_NON_TOUCH) {
            if (child.translationY >= (-child.height / 2f)) {
                animateSnap(child, SnapDirection.DOWN)
            } else {
                animateSnap(child, SnapDirection.UP)
            }
        }
    }

    override fun onNestedPreScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        target: View,
        dx: Int,
        dy: Int,
        consumed: IntArray,
        type: Int
    ) {
        super.onNestedPreScroll(coordinatorLayout, child, target, dx, dy, consumed, type)
        child.translationY = max(-child.height.toFloat(), min(0f, child.translationY - dy))
    }

    override fun layoutDependsOn(parent: CoordinatorLayout, child: BrowserToolbar, dependency: View): Boolean {
        if (dependency is Snackbar.SnackbarLayout) {
            positionSnackbar(dependency)
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    private fun animateSnap(child: View, direction: SnapDirection) = with(snapAnimator) {
        addUpdateListener { child.translationY = it.animatedValue as Float }
        setFloatValues(child.translationY, if (direction == SnapDirection.DOWN) 0f else -child.height.toFloat())
        start()
    }

    private fun positionSnackbar(view: View) {
        val params = view.layoutParams as CoordinatorLayout.LayoutParams

        // Position the snackbar below the toolbar so that it doesn't overlay the toolbar.
        params.anchorId = R.id.quick_action_sheet
        params.anchorGravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
        params.gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL

        view.layoutParams = params
    }
}

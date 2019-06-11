/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.toolbar.BrowserToolbar
import kotlin.math.max
import kotlin.math.min

private const val SNAP_ANIMATION_DURATION = 150L

private const val SMALL_ELEVATION_CHANGE = 0.01f

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [BrowserToolbar] at the bottom of the screen.
 *
 * This implementation will:
 * - Show/Hide the [BrowserToolbar] automatically when scrolling vertically.
 * - On showing a [Snackbar] position it above the [BrowserToolbar].
 * - Snap the [BrowserToolbar] to be hidden or visible when the user stops scrolling.
 */
class BrowserToolbarBottomBehavior(
    context: Context?,
    attrs: AttributeSet?
) : CoordinatorLayout.Behavior<BrowserToolbar>(context, attrs) {
    // This implementation is heavily based on this blog article:
    // https://android.jlelse.eu/scroll-your-bottom-navigation-view-away-with-10-lines-of-code-346f1ed40e9e

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var shouldSnapAfterScroll: Boolean = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var snapAnimator: ValueAnimator = ValueAnimator().apply {
        interpolator = DecelerateInterpolator()
        duration = SNAP_ANIMATION_DURATION
    }

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int
    ): Boolean {
        return if (axes == ViewCompat.SCROLL_AXIS_VERTICAL) {
            shouldSnapAfterScroll = type == ViewCompat.TYPE_TOUCH
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
        if (shouldSnapAfterScroll || type == ViewCompat.TYPE_NON_TOUCH) {
            if (child.translationY >= (child.height / 2f)) {
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
        child.translationY = max(0f, min(child.height.toFloat(), child.translationY + dy))
    }

    override fun layoutDependsOn(parent: CoordinatorLayout, child: BrowserToolbar, dependency: View): Boolean {
        if (dependency is Snackbar.SnackbarLayout) {
            positionSnackbar(child, dependency)
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun animateSnap(child: View, direction: SnapDirection) = with(snapAnimator) {
        addUpdateListener { child.translationY = it.animatedValue as Float }
        setFloatValues(child.translationY, if (direction == SnapDirection.UP) 0f else child.height.toFloat())
        start()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun positionSnackbar(child: View, snackbarLayout: Snackbar.SnackbarLayout) {
        val params = snackbarLayout.layoutParams as CoordinatorLayout.LayoutParams

        // Position the snackbar above the toolbar so that it doesn't overlay the toolbar.
        params.anchorId = child.id
        params.anchorGravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
        params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL

        snackbarLayout.layoutParams = params

        // In order to avoid the snackbar casting a shadow on the toolbar we adjust the elevation of the snackbar here.
        // We still place it slightly behind the toolbar so that it will not animate over the toolbar but instead pop
        // out from under the toolbar.
        snackbarLayout.elevation = child.elevation - SMALL_ELEVATION_CHANGE
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal enum class SnapDirection {
    UP,
    DOWN
}

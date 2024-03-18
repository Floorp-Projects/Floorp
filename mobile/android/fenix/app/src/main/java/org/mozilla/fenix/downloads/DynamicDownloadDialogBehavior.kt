/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads

import android.animation.ValueAnimator
import android.content.Context
import android.util.AttributeSet
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import kotlin.math.max
import kotlin.math.min

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [DynamicDownloadDialog]
 * at the bottom of the screen. Based off of BrowserToolbarBottomBehavior.
 *
 * This implementation will:
 * - Show/Hide the [DynamicDownloadDialog] automatically when scrolling vertically.
 * - Snap the [DynamicDownloadDialog] to be hidden or visible when the user stops scrolling.
 */

private const val SNAP_ANIMATION_DURATION = 150L

class DynamicDownloadDialogBehavior<V : View>(
    context: Context?,
    attrs: AttributeSet?,
    private val bottomToolbarHeight: Float = 0f,
) : CoordinatorLayout.Behavior<V>(context, attrs) {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var expanded: Boolean = true

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var shouldSnapAfterScroll: Boolean = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var snapAnimator: ValueAnimator = ValueAnimator()
        .apply {
            interpolator = DecelerateInterpolator()
            duration = SNAP_ANIMATION_DURATION
        }

    /**
     * Reference to [EngineView] used to check user's [android.view.MotionEvent]s.
     */
    @VisibleForTesting
    internal var engineView: EngineView? = null

    /**
     * Depending on how user's touch was consumed by EngineView / current website,
     *
     * we will animate the dynamic download notification dialog if:
     * - touches were used for zooming / panning operations in the website.
     *
     * We will do nothing if:
     * - the website is not scrollable
     * - the website handles the touch events itself through it's own touch event listeners.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val shouldScroll: Boolean
        get() = engineView?.getInputResultDetail()?.let {
            (it.canScrollToBottom() || it.canScrollToTop())
        } ?: false

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int,
    ): Boolean {
        return if (shouldScroll && axes == ViewCompat.SCROLL_AXIS_VERTICAL) {
            shouldSnapAfterScroll = type == ViewCompat.TYPE_TOUCH
            snapAnimator.cancel()
            true
        } else if (engineView?.getInputResultDetail()?.isTouchUnhandled() == true) {
            // Force expand the notification dialog if event is unhandled, otherwise user could get stuck in a
            // state where they cannot show it
            forceExpand(child)
            snapAnimator.cancel()
            false
        } else {
            false
        }
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        type: Int,
    ) {
        if (shouldSnapAfterScroll || type == ViewCompat.TYPE_NON_TOUCH) {
            if (expanded) {
                if (child.translationY >= bottomToolbarHeight / 2) {
                    animateSnap(child, SnapDirection.DOWN)
                } else {
                    animateSnap(child, SnapDirection.UP)
                }
            } else {
                if (child.translationY < (bottomToolbarHeight + child.height.toFloat() / 2)) {
                    animateSnap(child, SnapDirection.UP)
                } else {
                    animateSnap(child, SnapDirection.DOWN)
                }
            }
        }
    }

    override fun onNestedPreScroll(
        coordinatorLayout: CoordinatorLayout,
        child: V,
        target: View,
        dx: Int,
        dy: Int,
        consumed: IntArray,
        type: Int,
    ) {
        if (shouldScroll) {
            super.onNestedPreScroll(coordinatorLayout, child, target, dx, dy, consumed, type)
            child.translationY = max(
                0f,
                min(
                    child.height.toFloat() + bottomToolbarHeight,
                    child.translationY + dy,
                ),
            )
        }
    }

    override fun layoutDependsOn(
        parent: CoordinatorLayout,
        child: V,
        dependency: View,
    ): Boolean {
        engineView = parent.findViewInHierarchy { it is EngineView } as? EngineView
        return super.layoutDependsOn(parent, child, dependency)
    }

    fun forceExpand(view: View) {
        animateSnap(view, SnapDirection.UP)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun animateSnap(child: View, direction: SnapDirection) = with(snapAnimator) {
        expanded = direction == SnapDirection.UP
        addUpdateListener { child.translationY = it.animatedValue as Float }
        setFloatValues(
            child.translationY,
            if (direction == SnapDirection.UP) {
                0f
            } else {
                child.height.toFloat() + bottomToolbarHeight
            },
        )
        start()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal enum class SnapDirection {
        UP,
        DOWN,
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.animation.ValueAnimator
import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.animation.DecelerateInterpolator
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.android.view.findViewInHierarchy
import kotlin.math.max
import kotlin.math.min

private const val SNAP_ANIMATION_DURATION = 150L

private const val SMALL_ELEVATION_CHANGE = 0.01f

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [BrowserToolbar] at the bottom of the screen.
 *
 * This is safe to use even if the [BrowserToolbar] may be added / removed from a parent layout later
 * or if it could have Visibility.GONE set.
 *
 * This implementation will:
 * - Show/Hide the [BrowserToolbar] automatically when scrolling vertically.
 * - On showing a [Snackbar] position it above the [BrowserToolbar].
 * - Snap the [BrowserToolbar] to be hidden or visible when the user stops scrolling.
 */
@Suppress("TooManyFunctions")
class BrowserToolbarBottomBehavior(
    val context: Context?,
    attrs: AttributeSet?
) : CoordinatorLayout.Behavior<BrowserToolbar>(context, attrs) {
    // This implementation is heavily based on this blog article:
    // https://android.jlelse.eu/scroll-your-bottom-navigation-view-away-with-10-lines-of-code-346f1ed40e9e

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var shouldSnapAfterScroll: Boolean = false

    private var lastSnapStartedWasUp = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var startedScroll = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var snapAnimator: ValueAnimator = ValueAnimator().apply {
        interpolator = DecelerateInterpolator()
        duration = SNAP_ANIMATION_DURATION
    }

    /**
     * Reference to [EngineView] used to check user's [android.view.MotionEvent]s.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var engineView: EngineView? = null

    /**
     * Reference to the actual [BrowserToolbar] that we'll animate.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var browserToolbar: BrowserToolbar? = null

    /**
     * Depending on how user's touch was consumed by EngineView / current website,
     *
     * we will animate the dynamic navigation bar if:
     * - touches were used for zooming / panning operations in the website.
     *
     * We will do nothing if:
     * - the website is not scrollable
     * - the website handles the touch events itself through it's own touch event listeners.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal val shouldScroll: Boolean
        get() = engineView?.getInputResult() == EngineView.InputResult.INPUT_RESULT_HANDLED

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var gesturesDetector: BrowserGestureDetector = createGestureDetector()

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int
    ): Boolean {
        return if (browserToolbar != null) {
            startNestedScroll(axes, type, child)
        } else {
            return false // not interested in subsequent scroll events
        }
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: BrowserToolbar,
        target: View,
        type: Int
    ) {
        if (browserToolbar != null) {
            stopNestedScroll(type, child)
        }
    }

    override fun onInterceptTouchEvent(
        parent: CoordinatorLayout,
        child: BrowserToolbar,
        ev: MotionEvent
    ): Boolean {
        if (browserToolbar != null) {
            gesturesDetector.handleTouchEvent(ev)
        }
        return false // allow events to be passed to below listeners
    }

    override fun layoutDependsOn(parent: CoordinatorLayout, child: BrowserToolbar, dependency: View): Boolean {
        if (dependency is Snackbar.SnackbarLayout) {
            positionSnackbar(child, dependency)
        }

        return super.layoutDependsOn(parent, child, dependency)
    }

    override fun onLayoutChild(
        parent: CoordinatorLayout,
        child: BrowserToolbar,
        layoutDirection: Int
    ): Boolean {
        browserToolbar = child
        engineView = parent.findViewInHierarchy { it is EngineView } as? EngineView

        return super.onLayoutChild(parent, child, layoutDirection)
    }

    /**
     * Used to expand the [BrowserToolbar] upwards
     */
    fun forceExpand(view: View) {
        animateSnap(view, SnapDirection.UP)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun animateSnap(child: View, direction: SnapDirection) = with(snapAnimator) {
        lastSnapStartedWasUp = direction == SnapDirection.UP
        addUpdateListener { child.translationY = it.animatedValue as Float }
        setFloatValues(child.translationY, if (direction == SnapDirection.UP) 0f else child.height.toFloat())
        start()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun snapToolbarVertically() {
        if (snapAnimator.isStarted) {
            snapAnimator.end()
        } else {
            browserToolbar?.apply {
                translationY = if (translationY >= height / 2) {
                    height.toFloat()
                } else {
                    0f
                }
            }
        }
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun tryToScrollVertically(distance: Float) {
        browserToolbar?.let { toolbar ->
            if (shouldScroll && startedScroll) {
                toolbar.translationY =
                    max(0f, min(toolbar.height.toFloat(), toolbar.translationY + distance))
            } else {
                // Force expand the toolbar if the user scrolled up, it is not already expanded and
                // an animation to expand it is not already in progress,
                // otherwise the user could get stuck in a state where they cannot show the toolbar
                val isAnimatingUp = snapAnimator.isStarted && lastSnapStartedWasUp
                if (distance < 0 && toolbar.translationY != 0f && !isAnimatingUp) {
                    snapAnimator.cancel()
                    forceExpand(toolbar)
                }
            }
        }
    }

    /**
     * Helper function to ease testing.
     * (Re)Initializes the [BrowserGestureDetector] in a new context.
     *
     * Useful in spied behaviors, to ensure callbacks are of the spy and not of the initially created object
     * if the passed in argument is the result of [createGestureDetector].
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun initGesturesDetector(detector: BrowserGestureDetector) {
        gesturesDetector = detector
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun createGestureDetector() =
        BrowserGestureDetector(context!!, BrowserGestureDetector.GesturesListener(
            onVerticalScroll = ::tryToScrollVertically,
            onScaleBegin = {
                // Scale shouldn't animate the toolbar but a small y translation is still possible
                // because of a previous scroll. Try to be swift about such an in progress animation.
                snapToolbarVertically()
            }
        ))

    @VisibleForTesting
    internal fun startNestedScroll(axes: Int, type: Int, toolbar: BrowserToolbar): Boolean {
        return if (shouldScroll && axes == ViewCompat.SCROLL_AXIS_VERTICAL) {
            startedScroll = true
            shouldSnapAfterScroll = type == ViewCompat.TYPE_TOUCH
            snapAnimator.cancel()
            true
        } else if (engineView?.getInputResult() == EngineView.InputResult.INPUT_RESULT_UNHANDLED) {
            // Force expand the toolbar if event is unhandled, otherwise user could get stuck in a
            // state where they cannot show the toolbar
            snapAnimator.cancel()
            forceExpand(toolbar)
            false
        } else {
            false
        }
    }

    @VisibleForTesting
    internal fun stopNestedScroll(type: Int, toolbar: BrowserToolbar) {
        startedScroll = false
        if (shouldSnapAfterScroll || type == ViewCompat.TYPE_NON_TOUCH) {
            if (toolbar.translationY >= (toolbar.height / 2f)) {
                animateSnap(toolbar, SnapDirection.DOWN)
            } else {
                animateSnap(toolbar, SnapDirection.UP)
            }
        }
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal enum class SnapDirection {
    UP,
    DOWN
}

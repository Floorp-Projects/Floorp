/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets.behavior

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.engine.EngineView
import mozilla.components.support.ktx.android.view.findViewInHierarchy

/**
 * Where the view is placed on the screen.
 */
enum class ViewPosition {
    TOP,
    BOTTOM,
}

/**
 * A [CoordinatorLayout.Behavior] implementation to be used when placing [View] at the bottom of the screen.
 *
 * This is safe to use even if the [View] may be added / removed from a parent layout later
 * or if it could have Visibility.GONE set.
 *
 * This implementation will:
 * - Show/Hide the [View] automatically when scrolling vertically.
 * - Snap the [View] to be hidden or visible when the user stops scrolling.
 */
class EngineViewScrollingBehavior(
    val context: Context?,
    attrs: AttributeSet?,
    private val viewPosition: ViewPosition,
    private val crashReporting: CrashReporting? = null,
) : CoordinatorLayout.Behavior<View>(context, attrs) {
    // This implementation is heavily based on this blog article:
    // https://android.jlelse.eu/scroll-your-bottom-navigation-view-away-with-10-lines-of-code-346f1ed40e9e

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var shouldSnapAfterScroll: Boolean = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var startedScroll = false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var isScrollEnabled = false

    /**
     * Reference to [EngineView] used to check user's [android.view.MotionEvent]s.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var engineView: EngineView? = null

    /**
     * Reference to the actual [View] that we'll animate.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var dynamicScrollView: View? = null

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
        get() = engineView?.getInputResultDetail()?.let {
            (it.canScrollToBottom() || it.canScrollToTop()) && isScrollEnabled
        } ?: false

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var gesturesDetector: BrowserGestureDetector = createGestureDetector()

    @VisibleForTesting
    internal var yTranslator: ViewYTranslator = createYTranslationStrategy()

    private fun createYTranslationStrategy() = ViewYTranslator(viewPosition)

    override fun onStartNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: View,
        directTargetChild: View,
        target: View,
        axes: Int,
        type: Int,
    ): Boolean {
        return if (dynamicScrollView != null) {
            startNestedScroll(axes, type, child)
        } else {
            return false // not interested in subsequent scroll events
        }
    }

    override fun onStopNestedScroll(
        coordinatorLayout: CoordinatorLayout,
        child: View,
        target: View,
        type: Int,
    ) {
        if (dynamicScrollView != null) {
            stopNestedScroll(type, child)
        }
    }

    override fun onInterceptTouchEvent(
        parent: CoordinatorLayout,
        child: View,
        ev: MotionEvent,
    ): Boolean {
        if (dynamicScrollView != null) {
            gesturesDetector.handleTouchEvent(ev)
        }
        return false // allow events to be passed to below listeners
    }

    override fun onLayoutChild(
        parent: CoordinatorLayout,
        child: View,
        layoutDirection: Int,
    ): Boolean {
        dynamicScrollView = child
        engineView = parent.findViewInHierarchy { it is EngineView } as? EngineView

        return super.onLayoutChild(parent, child, layoutDirection)
    }

    /**
     * Used to expand the [View]
     */
    fun forceExpand(view: View) {
        yTranslator.expandWithAnimation(view)
    }

    /**
     * Used to collapse the [View]
     */
    fun forceCollapse(view: View) {
        yTranslator.collapseWithAnimation(view)
    }

    /**
     * Allow this view to be animated.
     *
     * @see disableScrolling
     */
    fun enableScrolling() {
        isScrollEnabled = true
    }

    /**
     * Disable scrolling of the view irrespective of the intrinsic checks.
     *
     * @see enableScrolling
     */
    fun disableScrolling() {
        isScrollEnabled = false
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun tryToScrollVertically(distance: Float) {
        dynamicScrollView?.let { view ->
            if (shouldScroll && startedScroll) {
                yTranslator.translate(view, distance)
            } else if (engineView?.getInputResultDetail()?.isTouchHandlingUnknown() == false) {
                // Force expand the view if the user scrolled up, it is not already expanded and
                // an animation to expand it is not already in progress,
                // otherwise the user could get stuck in a state where they cannot show the view
                // See https://github.com/mozilla-mobile/android-components/issues/7101
                yTranslator.forceExpandIfNotAlready(view, distance)
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
        BrowserGestureDetector(
            context!!,
            BrowserGestureDetector.GesturesListener(
                onVerticalScroll = ::tryToScrollVertically,
                onScaleBegin = {
                    // Scale shouldn't animate the view but a small y translation is still possible
                    // because of a previous scroll. Try to be swift about such an in progress animation.
                    yTranslator.snapImmediately(dynamicScrollView)
                },
            ),
            crashReporting = crashReporting,
        )

    @VisibleForTesting
    internal fun startNestedScroll(axes: Int, type: Int, view: View): Boolean {
        return if (shouldScroll && axes == ViewCompat.SCROLL_AXIS_VERTICAL) {
            startedScroll = true
            shouldSnapAfterScroll = type == ViewCompat.TYPE_TOUCH
            yTranslator.cancelInProgressTranslation()
            true
        } else if (engineView?.getInputResultDetail()?.isTouchUnhandled() == true) {
            // Force expand the view if event is unhandled, otherwise user could get stuck in a
            // state where they cannot show the view
            yTranslator.cancelInProgressTranslation()
            yTranslator.expandWithAnimation(view)
            false
        } else {
            false
        }
    }

    @VisibleForTesting
    internal fun stopNestedScroll(type: Int, view: View) {
        startedScroll = false
        if (shouldSnapAfterScroll || type == ViewCompat.TYPE_NON_TOUCH) {
            yTranslator.snapWithAnimation(view)
        }
    }
}

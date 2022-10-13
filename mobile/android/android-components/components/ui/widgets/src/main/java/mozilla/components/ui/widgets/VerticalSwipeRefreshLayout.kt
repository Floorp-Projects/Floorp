/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.widgets

import android.annotation.SuppressLint
import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import androidx.annotation.VisibleForTesting
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout
import kotlin.math.abs

/**
 * [SwipeRefreshLayout] that filters only vertical scrolls for triggering pull to refresh.
 *
 * Following situations will not trigger pull to refresh:
 * - a scroll happening more on the horizontal axis
 * - a scale in/out gesture
 * - a quick scale gesture
 *
 * To control responding to scrolls and showing the pull to refresh throbber or not
 * use the [View.isEnabled] property.
 */
class VerticalSwipeRefreshLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : SwipeRefreshLayout(context, attrs) {
    @VisibleForTesting
    internal var isQuickScaleInProgress = false

    @VisibleForTesting
    internal var quickScaleEvents = QuickScaleEvents()
    private var previousX = 0f
    private var previousY = 0f
    private val doubleTapTimeout = ViewConfiguration.getDoubleTapTimeout()
    private val doubleTapSlop = ViewConfiguration.get(context).scaledDoubleTapSlop
    private val doubleTapSlopSquare = doubleTapSlop * doubleTapSlop

    @Suppress("ComplexMethod", "ReturnCount")
    override fun onInterceptTouchEvent(event: MotionEvent): Boolean {
        // Setting "isEnabled = false" is recommended for users of this ViewGroup
        // who who are not interested in the pull to refresh functionality
        // Setting this easily avoids executing code unneededsly before the check for "canChildScrollUp".
        if (!isEnabled) {
            return false
        }

        // Layman's scale gesture (with two fingers) detector.
        // Allows for quick, serial inference as opposed to using ScaleGestureDetector
        // which uses callbacks and would be hard to synchronize in the little time we have.
        if (event.pointerCount > 1) {
            return false
        }

        val eventAction = event.action

        // Cleanup if the gesture has been aborted or quick scale just ended/
        if (MotionEvent.ACTION_CANCEL == eventAction ||
            (MotionEvent.ACTION_UP == eventAction && isQuickScaleInProgress)
        ) {
            forgetQuickScaleEvents()
            return callSuperOnInterceptTouchEvent(event)
        }

        // Disable pull to refresh if quick scale is in progress.
        maybeAddDoubleTapEvent(event)
        if (isQuickScaleInProgress(quickScaleEvents)) {
            isQuickScaleInProgress = true
            return false
        }

        // Disable pull to refresh if the move was more on the X axis.
        if (MotionEvent.ACTION_DOWN == eventAction) {
            previousX = event.x
            previousY = event.y
        } else if (MotionEvent.ACTION_MOVE == eventAction) {
            val xDistance = abs(event.x - previousX)
            val yDistance = abs(event.y - previousY)
            previousX = event.x
            previousY = event.y
            if (xDistance > yDistance) {
                return false
            }
        }

        return callSuperOnInterceptTouchEvent(event)
    }

    override fun onStartNestedScroll(child: View, target: View, nestedScrollAxes: Int): Boolean {
        // Ignoring nested scrolls from descendants.
        // Allowing descendants to trigger nested scrolls would defeat the purpose of this class
        // and result in pull to refresh to happen for all movements on the Y axis
        // (even as part of scale/quick scale gestures) while also doubling the throbber with the overscroll shadow.
        return if (isEnabled) {
            return false
        } else {
            callSuperOnStartNestedScroll(child, target, nestedScrollAxes)
        }
    }

    @SuppressLint("Recycle") // we do recycle the events in forgetQuickScaleEvents()
    @VisibleForTesting
    internal fun maybeAddDoubleTapEvent(event: MotionEvent) {
        val currentEventAction = event.action

        // A double tap event must follow the order:
        // ACTION_DOWN - ACTION_UP - ACTION_DOWN
        // all these events happening in an interval defined by a system constant - DOUBLE_TAP_TIMEOUT

        if (MotionEvent.ACTION_DOWN == currentEventAction) {
            if (quickScaleEvents.upEvent != null) {
                if (event.eventTime - quickScaleEvents.upEvent!!.eventTime > doubleTapTimeout) {
                    // Too much time passed for the MotionEvents sequence to be considered
                    // a quick scale gesture. Restart counting.
                    forgetQuickScaleEvents()
                    quickScaleEvents.firstDownEvent = MotionEvent.obtain(event)
                } else {
                    quickScaleEvents.secondDownEvent = MotionEvent.obtain(event)
                }
            } else {
                // This may be the first time the user touches the screen or
                // the gesture was not finished with ACTION_UP.
                forgetQuickScaleEvents()
                quickScaleEvents.firstDownEvent = MotionEvent.obtain(event)
            }
        }
        // For the double tap events series we need ACTION_DOWN first
        // and then ACTION_UP second.
        else if (MotionEvent.ACTION_UP == currentEventAction && quickScaleEvents.firstDownEvent != null) {
            quickScaleEvents.upEvent = MotionEvent.obtain(event)
        }
    }

    @VisibleForTesting
    internal fun forgetQuickScaleEvents() {
        quickScaleEvents.firstDownEvent?.recycle()
        quickScaleEvents.upEvent?.recycle()
        quickScaleEvents.secondDownEvent?.recycle()
        quickScaleEvents.firstDownEvent = null
        quickScaleEvents.upEvent = null
        quickScaleEvents.secondDownEvent = null

        isQuickScaleInProgress = false
    }

    @VisibleForTesting
    internal fun isQuickScaleInProgress(events: QuickScaleEvents): Boolean {
        return if (events.isNotNull()) {
            isQuickScaleInProgress(events.firstDownEvent!!, events.upEvent!!, events.secondDownEvent!!)
        } else {
            false
        }
    }

    // Method closely following GestureDetectorCompat#isConsideredDoubleTap.
    // Allows for serial inference of double taps as opposed to using callbacks.
    @VisibleForTesting
    internal fun isQuickScaleInProgress(
        firstDown: MotionEvent,
        firstUp: MotionEvent,
        secondDown: MotionEvent,
    ): Boolean {
        if (secondDown.eventTime - firstUp.eventTime > doubleTapTimeout) {
            return false
        }

        val deltaX = firstDown.x.toInt() - secondDown.x.toInt()
        val deltaY = firstDown.y.toInt() - secondDown.y.toInt()

        return deltaX * deltaX + deltaY * deltaY < doubleTapSlopSquare
    }

    @VisibleForTesting
    internal fun callSuperOnInterceptTouchEvent(event: MotionEvent) =
        super.onInterceptTouchEvent(event)

    @VisibleForTesting
    internal fun callSuperOnStartNestedScroll(child: View, target: View, nestedScrollAxes: Int) =
        super.onStartNestedScroll(child, target, nestedScrollAxes)

    private fun QuickScaleEvents.isNotNull(): Boolean {
        return firstDownEvent != null && upEvent != null && secondDownEvent != null
    }

    /**
     * Wrapper over the MotionEvents that compose a quickScale gesture.
     */
    @VisibleForTesting
    internal data class QuickScaleEvents(
        var firstDownEvent: MotionEvent? = null,
        var upEvent: MotionEvent? = null,
        var secondDownEvent: MotionEvent? = null,
    )
}

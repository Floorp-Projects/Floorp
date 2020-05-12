/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.behavior

import android.content.Context
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import androidx.annotation.VisibleForTesting
import kotlin.math.abs

/**
 * Custom [MotionEvent] gestures detector with scroll / zoom callbacks.
 *
 * Favors zoom gestures in detriment of the scroll gestures with:
 *  - higher sensitivity for multi-finger zoom gestures
 *  - ignoring scrolls if zoom is in progress
 *
 *  @param applicationContext context used for registering internal gesture listeners.
 *  @param listener client interested in zoom / scroll events.
 */
internal class BrowserGestureDetector(
    applicationContext: Context,
    listener: GesturesListener
) {
    /**
     * Accepts MotionEvents and dispatches zoom / scroll events to the registered listener when appropriate.
     *
     * Applications should pass a complete and consistent event stream to this method.
     * A complete and consistent event stream involves all MotionEvents from the initial ACTION_DOWN
     * to the final ACTION_UP or ACTION_CANCEL.
     *
     * @return if the event was handled by any of the registered detectors
     */
    internal fun handleTouchEvent(event: MotionEvent): Boolean {
        // A double tap for a quick scale gesture (quick double tap followed by a drag)
        // would trigger a ACTION_CANCEL event before the MOVE_EVENT.
        // This would prevent the scale detector from properly inferring the movement.
        // We'll want to ignore ACTION_CANCEL but process the next stream of events.
        if (event.actionMasked != MotionEvent.ACTION_CANCEL) {
            scaleGestureDetector.onTouchEvent(event)
        }

        // Ignore scrolling if zooming is already in progress.
        return if (!scaleGestureDetector.isInProgress) {
            gestureDetector.onTouchEvent(event)
        } else {
            false
        }
    }

    @VisibleForTesting
    @Suppress("MaxLineLength")
    internal val gestureDetector: GestureDetector by lazy(LazyThreadSafetyMode.NONE) {
        GestureDetector(
            applicationContext,
            CustomScrollDetectorListener { previousEvent: MotionEvent, currentEvent: MotionEvent, distanceX, distanceY ->
                run {
                    listener.onScroll?.invoke(distanceX, distanceY)

                    if (abs(currentEvent.y - previousEvent.y) >= abs(currentEvent.x - previousEvent.x)) {
                        listener.onVerticalScroll?.invoke(distanceY)
                    } else {
                        listener.onHorizontalScroll?.invoke(distanceX)
                    }
                }
            }
        )
    }

    @VisibleForTesting
    internal val scaleGestureDetector: ScaleGestureDetector by lazy(LazyThreadSafetyMode.NONE) {
        ScaleGestureDetector(
            applicationContext,
            CustomScaleDetectorListener(
                listener.onScaleBegin ?: {},
                listener.onScale ?: {},
                listener.onScaleEnd ?: {})
        ).apply {
            // Use reflection to modify two fields controlling the sensitivity of our scale detector.
            // The lower the values the higher the sensitivity.
            // Values of 0 here would mean all swipe events will be treated as pinch/spread to zoom gestures,
            // in our context effectively meaning no scrolling, only zooming.
            listOf(
                javaClass.getDeclaredField("mSpanSlop"),
                javaClass.getDeclaredField("mMinSpan")
            ).forEach { field ->
                field.isAccessible = true
                field.set(this, (field.get(this) as Int) / 2)
            }
        }
    }

    /**
     * A convenience containing listeners for zoom / scroll events
     *
     * Provide implementation for the events you are interested in.
     * The others will be no-op.
     */
    internal class GesturesListener(
        /**
         * Responds to scroll events for a gesture in progress.
         * The distance in x and y is also supplied for convenience.
         */
        val onScroll: ((distanceX: Float, distanceY: Float) -> Unit)? = { _, _ -> run {} },

        /**
         * Responds to an in progress scroll occuring more on the vertical axis.
         * The scroll distance is also supplied for convenience.
         */
        val onVerticalScroll: ((distance: Float) -> Unit)? = {},

        /**
         * Responds to an in progress scroll occurring more on the horizontal axis.
         * The scroll distance is also supplied for convenience.
         */
        val onHorizontalScroll: ((distance: Float) -> Unit)? = {},

        /**
         * Responds to the the beginning of a new scale gesture.
         * Reported by new pointers going down.
         */
        val onScaleBegin: ((scaleFactor: Float) -> Unit)? = {},

        /**
         * Responds to scaling events for a gesture in progress.
         * The scaling factor is also supplied for convenience.
         * This value is represents the difference from the previous scale event to the current event.
         */
        val onScale: ((scaleFactor: Float) -> Unit)? = {},

        /**
         * Responds to the end of a scale gesture.
         * Reported by existing pointers going up.
         */
        val onScaleEnd: ((scaleFactor: Float) -> Unit)? = {}
    )

    private class CustomScrollDetectorListener(
        val onScrolling: (
            previousEvent: MotionEvent,
            currentEvent: MotionEvent,
            distanceX: Float,
            distanceY: Float
        ) -> Unit
    ) : GestureDetector.SimpleOnGestureListener() {
        override fun onScroll(
            e1: MotionEvent,
            e2: MotionEvent,
            distanceX: Float,
            distanceY: Float
        ): Boolean {
            onScrolling(e1, e2, distanceX, distanceY)
            return true
        }
    }

    private class CustomScaleDetectorListener(
        val onScaleBegin: (scaleFactor: Float) -> Unit = {},
        val onScale: (scaleFactor: Float) -> Unit = {},
        val onScaleEnd: (scaleFactor: Float) -> Unit = {}
    ) : ScaleGestureDetector.SimpleOnScaleGestureListener() {
        override fun onScaleBegin(detector: ScaleGestureDetector): Boolean {
            onScaleBegin(detector.scaleFactor)
            return true
        }

        override fun onScale(detector: ScaleGestureDetector): Boolean {
            onScale(detector.scaleFactor)
            return true
        }

        override fun onScaleEnd(detector: ScaleGestureDetector) {
            onScaleEnd(detector.scaleFactor)
        }
    }
}
